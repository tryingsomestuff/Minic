#include <algorithm>
#include <cmath>
#include <ostream>
#include <shared_mutex>

#include <dirent.h>

#include "definition.hpp"

#ifdef WITH_LEARNER

#include "dynamicConfig.hpp"
#include "positionTools.hpp"
#include "moveGen.hpp"

#include "searcher.hpp"

#include "nnue.hpp"

#include "multi_think.h"
#include "sfensReader.hpp"
#include "evaluate_nnue_learner.h"
#include "trainer/trainer.h"
#include "learner.hpp"
#include "learn_tools.hpp"
#include "evaluate_nnue_learner.h"
#include "architectures/halfkp_256x2-32-32.h"

#include "trainer/trainer_affine_transform.h"
#include "trainer/trainer_clipped_relu.h"
#include "trainer/trainer_feature_transformer.h"
#include "trainer/trainer_input_slice.h"
//#include "trainer/trainer_sum.h"

namespace LearnerConfig{

bool use_draw_games_in_training = true;
bool use_draw_games_in_validation = true;
// 1.0 / PawnValueEg / 4.0 * log(10.0)
double winning_probability_coefficient = 0.00276753015984861260098316280611;
// Score scale factors
double src_score_min_value = 0.0;
double src_score_max_value = 1.0;
double dest_score_min_value = 0.0;
double dest_score_max_value = 1.0;
// Assume teacher signals are the scores of deep searches, and convert them into winning
// probabilities in the trainer. Sometimes we want to use the winning probabilities in the training
// data directly. In those cases, we set false to this variable.
bool convert_teacher_signal_to_winning_probability = true;
// Using WDL with win rate model instead of sigmoid
bool use_wdl = false;    

} // LearnerConfig

namespace FromSF {

double win_rate_model_double(double v, int ply) {

   // The model captures only up to 240 plies, so limit input (and rescale)
   double m = std::min(240, ply) / 64.0;

   // Coefficients of a 3rd order polynomial fit based on fishtest data
   // for two parameters needed to transform eval to the argument of a
   // logistic function.
   double as[] = {-8.24404295, 64.23892342, -95.73056462, 153.86478679};
   double bs[] = {-3.37154371, 28.44489198, -56.67657741,  72.05858751};
   double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
   double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

   // Transform eval to centipawns with limited range
     double x = std::clamp(double(100 * v) / 100, -1000.0, 1000.0);

   // Return win rate in per mille
   return 1000.0 / (1 + std::exp((a - x) / b));
}

} // FromSF

// Addition and subtraction definition for atomic<T>
// Aligned with atomicAdd() in Apery/learner.hpp.
template <typename T>
T operator += (std::atomic<T>& x, const T rhs)
{
	T old = x.load(std::memory_order_consume);
	// It is allowed that the value is rewritten from other thread at this timing.
	// The idea that the value is not destroyed is good.
	T desired = old + rhs;
	while (!x.compare_exchange_weak(old, desired, std::memory_order_release, std::memory_order_consume))
		desired = old + rhs;
	return desired;
}

template <typename T>
T operator -= (std::atomic<T>& x, const T rhs) { return x += -rhs; }

// ordinary sigmoid function
double sigmoid(double x)
{
	return 1.0 / (1.0 + std::exp(-x));
}

// A function that converts the evaluation value to the winning rate [0,1]
double winning_percentage(double value)
{
	// 1/(1+10^(-Eval/4))
	// = 1/(1+e^(-Eval/4*ln(10))
	// = sigmoid(Eval/4*ln(10))
	return sigmoid(value * LearnerConfig::winning_probability_coefficient);
}

// A function that converts the evaluation value to the winning rate [0,1]
double winning_percentage_wdl(double value, int ply)
{
	double wdl_w = FromSF::win_rate_model_double( value, ply);
	double wdl_l = FromSF::win_rate_model_double(-value, ply);
	double wdl_d = 1000.0 - wdl_w - wdl_l;

	return (wdl_w + wdl_d / 2.0) / 1000.0;
}

// A function that converts the evaluation value to the winning rate [0,1]
double winning_percentage(double value, int ply)
{
	if (LearnerConfig::use_wdl) {
		return winning_percentage_wdl(value, ply);
	}
	else {
		return winning_percentage(value);
	}
}

double calc_cross_entropy_of_winning_percentage(double deep_win_rate, double shallow_eval, int ply)
{
	double p = deep_win_rate;
	double q = winning_percentage(shallow_eval, ply);
	return -p * std::log(q) - (1 - p) * std::log(1 - q);
}

double calc_d_cross_entropy_of_winning_percentage(double deep_win_rate, double shallow_eval, int ply)
{
	constexpr double epsilon = 0.000001;
	double y1 = calc_cross_entropy_of_winning_percentage(deep_win_rate, shallow_eval          , ply);
	double y2 = calc_cross_entropy_of_winning_percentage(deep_win_rate, shallow_eval + epsilon, ply);

	// Divide by the winning_probability_coefficient to match scale with the sigmoidal win rate
	return ((y2 - y1) / epsilon) / LearnerConfig::winning_probability_coefficient;
}

double dsigmoid(double x)
{
	// Sigmoid function
	// f(x) = 1/(1+exp(-x))
	// the first derivative is
	// f'(x) = df/dx = f(x)・{ 1-f(x)}
	// becomes
	return sigmoid(x) * (1.0 - sigmoid(x));
}

// A constant used in elmo (WCSC27). Adjustment required.
// Since elmo does not internally divide the expression, the value is different.
// You can set this value with the learn command.
// 0.33 is equivalent to the constant (0.5) used in elmo (WCSC27)
double ELMO_LAMBDA = 0.33;
double ELMO_LAMBDA2 = 0.33;
double ELMO_LAMBDA_LIMIT = 32000;

double calc_grad(NNUEValue teacher_signal, NNUEValue shallow , const PackedSfenValue& psv)
{
	// elmo (WCSC27) method
	// Correct with the actual game wins and losses.

	// Training Formula · Issue #71 · nodchip/Stockfish https://github.com/nodchip/Stockfish/issues/71
	double scaled_teacher_signal = teacher_signal;
	// Normalize to [0.0, 1.0].
	scaled_teacher_signal = (scaled_teacher_signal - LearnerConfig::src_score_min_value) / (LearnerConfig::src_score_max_value - LearnerConfig::src_score_min_value);
	// Scale to [dest_score_min_value, dest_score_max_value].
	scaled_teacher_signal = scaled_teacher_signal * (LearnerConfig::dest_score_max_value - LearnerConfig::dest_score_min_value) + LearnerConfig::dest_score_min_value;

	const double q = winning_percentage(shallow, psv.gamePly);
	// Teacher winning probability.
	double p = scaled_teacher_signal;
	if (LearnerConfig::convert_teacher_signal_to_winning_probability) {
		p = winning_percentage(scaled_teacher_signal, psv.gamePly);
	}

	// Use 1 as the correction term if the expected win rate is 1, 0 if you lose, and 0.5 if you draw.
	// game_result = 1,0,-1 so add 1 and divide by 2.
	const double t = double(psv.game_result + 1) / 2;

	// If the evaluation value in deep search exceeds ELMO_LAMBDA_LIMIT, apply ELMO_LAMBDA2 instead of ELMO_LAMBDA.
	const double lambda = (abs(teacher_signal) >= ELMO_LAMBDA_LIMIT) ? ELMO_LAMBDA2 : ELMO_LAMBDA;

	double grad;
	if (LearnerConfig::use_wdl) {
		double dce_p = calc_d_cross_entropy_of_winning_percentage(p, shallow, psv.gamePly);
		double dce_t = calc_d_cross_entropy_of_winning_percentage(t, shallow, psv.gamePly);
		grad = lambda * dce_p + (1.0 - lambda) * dce_t;
	}
	else {
		// Use the actual win rate as a correction term.
		// This is the idea of ​​elmo (WCSC27), modern O-parts.
		grad = lambda * (q - p) + (1.0 - lambda) * (q - t);
	}

	return grad;
}

// Calculate cross entropy during learning
// The individual cross entropy of the win/loss term and win rate term of the elmo expression is returned to the arguments cross_entropy_eval and cross_entropy_win.
void calc_cross_entropy(NNUEValue teacher_signal, NNUEValue shallow, const PackedSfenValue& psv,
	double& cross_entropy_eval, double& cross_entropy_win, double& cross_entropy,
	double& entropy_eval, double& entropy_win, double& entropy)
{
	// Training Formula · Issue #71 · nodchip/Stockfish https://github.com/nodchip/Stockfish/issues/71
	double scaled_teacher_signal = teacher_signal;
	// Normalize to [0.0, 1.0].
	scaled_teacher_signal = (scaled_teacher_signal - LearnerConfig::src_score_min_value) / (LearnerConfig::src_score_max_value - LearnerConfig::src_score_min_value);
	// Scale to [dest_score_min_value, dest_score_max_value].
	scaled_teacher_signal = scaled_teacher_signal * (LearnerConfig::dest_score_max_value - LearnerConfig::dest_score_min_value) + LearnerConfig::dest_score_min_value;

	// Teacher winning probability.
	double p = scaled_teacher_signal;
	if (LearnerConfig::convert_teacher_signal_to_winning_probability) {
		p = winning_percentage(scaled_teacher_signal);
	}
	const double q /* eval_winrate    */ = winning_percentage(shallow);
	const double t = double(psv.game_result + 1) / 2;

	constexpr double epsilon = 0.000001;

	// If the evaluation value in deep search exceeds ELMO_LAMBDA_LIMIT, apply ELMO_LAMBDA2 instead of ELMO_LAMBDA.
	const double lambda = (abs(teacher_signal) >= ELMO_LAMBDA_LIMIT) ? ELMO_LAMBDA2 : ELMO_LAMBDA;

	const double m = (1.0 - lambda) * t + lambda * p;

	cross_entropy_eval = (-p * std::log(q + epsilon) - (1.0 - p) * std::log(1.0 - q + epsilon));
	cross_entropy_win = (-t * std::log(q + epsilon) - (1.0 - t) * std::log(1.0 - q + epsilon));
	entropy_eval = (-p * std::log(p + epsilon) - (1.0 - p) * std::log(1.0 - p + epsilon));
	entropy_win = (-t * std::log(t + epsilon) - (1.0 - t) * std::log(1.0 - t + epsilon));
	cross_entropy = (-m * std::log(q + epsilon) - (1.0 - m) * std::log(1.0 - q + epsilon));
	entropy = (-m * std::log(m + epsilon) - (1.0 - m) * std::log(1.0 - m + epsilon));
}

// Class to generate sfen with multiple threads
struct LearnerThink: public MultiThink
{
	LearnerThink(SfenReader& sr_):sr(sr_),stop_flag(false), save_only_once(false)
	{
		learn_sum_cross_entropy_eval = 0.0;
		learn_sum_cross_entropy_win = 0.0;
		learn_sum_cross_entropy = 0.0;
		learn_sum_entropy_eval = 0.0;
		learn_sum_entropy_win = 0.0;
		learn_sum_entropy = 0.0;

		newbob_scale = 1.0;
		newbob_decay = 1.0;
		newbob_num_trials = 2;
		best_loss = std::numeric_limits<double>::infinity();
		latest_loss_sum = 0.0;
		latest_loss_count = 0;
	}

	virtual void thread_worker(size_t thread_id);

	// Start a thread that loads the phase file in the background.
	void start_file_read_worker() { sr.start_file_read_worker(); }

	// save merit function parameters to a file
	bool save(bool is_final=false);

	// sfen reader
	SfenReader& sr;

	// Learning iteration counter
	uint64_t epoch = 0;

	// Mini batch size size. Be sure to set it on the side that uses this class.
	uint64_t mini_batch_size = 1000*1000;

	bool stop_flag;

	// Discount rate
	double discount_rate;

	// Option to exclude early stage from learning
	int reduction_gameply;

	// Option not to learn kk/kkp/kpp/kppp
	std::array<bool,4> freeze;

	// If the absolute value of the evaluation value of the deep search of the teacher phase exceeds this value, discard the teacher phase.
	int eval_limit;

	// Flag whether to dig a folder each time the evaluation function is saved.
	// If true, do not dig the folder.
	bool save_only_once;

	// --- loss calculation

	// For calculation of learning data loss
	std::atomic<double> learn_sum_cross_entropy_eval;
	std::atomic<double> learn_sum_cross_entropy_win;
	std::atomic<double> learn_sum_cross_entropy;
	std::atomic<double> learn_sum_entropy_eval;
	std::atomic<double> learn_sum_entropy_win;
	std::atomic<double> learn_sum_entropy;

	std::shared_timed_mutex nn_mutex;
	double newbob_scale;
	double newbob_decay;
	int newbob_num_trials;
	double best_loss;
	double latest_loss_sum;
	uint64_t latest_loss_count;
	std::string best_nn_directory;

	uint64_t eval_save_interval;
	uint64_t loss_output_interval;
	uint64_t mirror_percentage;

	// Loss calculation.
	// done: Number of phases targeted this time
	void calc_loss(size_t thread_id , uint64_t done);

	// Define the loss calculation in as a task and execute it
	TaskDispatcher task_dispatcher;
};

void LearnerThink::calc_loss(size_t thread_id, uint64_t done)
{
	std::cout << "PROGRESS: " ;//<< now_string() << ", ";
	std::cout << sr.total_done << " sfens";
	std::cout << ", iteration " << epoch;
	std::cout << ", eta = " << NNUE::get_eta() << ", ";

	// For calculation of verification data loss
	std::atomic<double> test_sum_cross_entropy_eval,test_sum_cross_entropy_win,test_sum_cross_entropy;
	std::atomic<double> test_sum_entropy_eval,test_sum_entropy_win,test_sum_entropy;
	test_sum_cross_entropy_eval = 0;
	test_sum_cross_entropy_win = 0;
	test_sum_cross_entropy = 0;
	test_sum_entropy_eval = 0;
	test_sum_entropy_win = 0;
	test_sum_entropy = 0;

	// norm for learning
	std::atomic<double> sum_norm;
	sum_norm = 0;

	// The number of times the pv first move of deep search matches the pv first move of search(1).
	std::atomic<int> move_accord_count;
	move_accord_count = 0;

	// Display the value of eval() in the initial stage of Hirate and see the shaking.
	{
	Position pos;
    readFEN(startPosition,pos,true);
	EvalData data;
	Searcher & context = *ThreadPool::instance().at(thread_id);
    std::cout << "hirate eval = " << eval(pos,data,context);
	}

	//print_eval_stat(pos);

	// It's better to parallelize here, but it's a bit troublesome because the search before slave has not finished.
	// I created a mechanism to call task, so I will use it.

	// The number of tasks to do.
	std::atomic<int> task_count;
	task_count = (int)sr.sfen_for_mse.size();
	task_dispatcher.task_reserve(task_count);

	// Create a task to search for the situation and give it to each thread.
	for (const auto& ps : sr.sfen_for_mse)
	{
		// Assign work to each thread using TaskDispatcher.
		// A task definition for that.
		// It is not possible to capture pos used in ↑, so specify the variables you want to capture one by one.
		auto task = [&ps,&test_sum_cross_entropy_eval,
                         &test_sum_cross_entropy_win,
                         &test_sum_cross_entropy,
                         &test_sum_entropy_eval,
                         &test_sum_entropy_win,
                         &test_sum_entropy, 
                         &sum_norm,
                         &task_count ,&move_accord_count](size_t t_id)
		{
			// Does C++ properly capture a new ps instance for each loop?.
            Position p;
			//readFEN(startPosition,p,true);
			if (set_from_packed_sfen(p,*const_cast<PackedSfen*>(&ps.sfen), false) != 0)
			{
				// Unfortunately, as an sfen for rmse calculation, an invalid sfen was drawn.
				std::cout << "Error! : illegal packed sfen " << GetFEN(p) << std::endl;
			}

			// Evaluation value for shallow search using qsearch (using current network state)
			DepthType seldepth;
			PVList pv;
			Searcher & contxt = *ThreadPool::instance().at(t_id);
			auto shallow_value = contxt.qsearchNoPruning(-MATE,MATE,p,0,seldepth,&pv);

			// Evaluation value of deep search (from training data)
			auto deep_value = (NNUEValue)ps.score;

			// For the time being, regarding the win rate and loss terms only in the elmo method
			// Calculate and display the cross entropy.
			double test_cross_entropy_eval, test_cross_entropy_win, test_cross_entropy;
			double test_entropy_eval, test_entropy_win, test_entropy;
			calc_cross_entropy(deep_value, shallow_value, ps, test_cross_entropy_eval, test_cross_entropy_win, test_cross_entropy, test_entropy_eval, test_entropy_win, test_entropy);
			// The total cross entropy need not be abs() by definition.
			test_sum_cross_entropy_eval += test_cross_entropy_eval;
			test_sum_cross_entropy_win += test_cross_entropy_win;
			test_sum_cross_entropy += test_cross_entropy;
			test_sum_entropy_eval += test_entropy_eval;
			test_sum_entropy_win += test_entropy_win;
			test_sum_entropy += test_entropy;
			sum_norm += (double)abs(shallow_value);

/*
            ///@todo
			// Determine if the teacher's move and the score of the shallow search match
			{
				auto r = search(p,1);
				if ((uint16_t)r.second[0] == ps.move)
					move_accord_count.fetch_add(1, std::memory_order_relaxed);
			}
*/
			// Reduced one task because I did it
			--task_count;
		};

		// Throw the defined task to slave.
		task_dispatcher.push_task_async(task);
	}

	// join yourself as a slave
	task_dispatcher.on_idle(thread_id);

	// wait for all tasks to complete
	while (task_count)
		sleep(1);

	latest_loss_sum += test_sum_cross_entropy - test_sum_entropy;
	latest_loss_count += sr.sfen_for_mse.size();

// learn_cross_entropy may be called train cross entropy in the world of machine learning,
// When omitting the acronym, it is nice to be able to distinguish it from test cross entropy(tce) by writing it as lce.

	if (sr.sfen_for_mse.size() && done)
	{
		std::cout
			<< " , test_cross_entropy_eval = "  << test_sum_cross_entropy_eval / sr.sfen_for_mse.size()
			<< " , test_cross_entropy_win = "   << test_sum_cross_entropy_win / sr.sfen_for_mse.size()
			<< " , test_entropy_eval = "        << test_sum_entropy_eval / sr.sfen_for_mse.size()
			<< " , test_entropy_win = "         << test_sum_entropy_win / sr.sfen_for_mse.size()
			<< " , test_cross_entropy = "       << test_sum_cross_entropy / sr.sfen_for_mse.size()
			<< " , test_entropy = "             << test_sum_entropy / sr.sfen_for_mse.size()
			<< " , norm = "						<< sum_norm
			<< " , move accuracy = "			<< (move_accord_count * 100.0 / sr.sfen_for_mse.size()) << "%";
		if (done != static_cast<uint64_t>(-1))
		{
			std::cout
				<< " , learn_cross_entropy_eval = " << learn_sum_cross_entropy_eval / done
				<< " , learn_cross_entropy_win = "  << learn_sum_cross_entropy_win / done
				<< " , learn_entropy_eval = "       << learn_sum_entropy_eval / done
				<< " , learn_entropy_win = "        << learn_sum_entropy_win / done
				<< " , learn_cross_entropy = "      << learn_sum_cross_entropy / done
				<< " , learn_entropy = "            << learn_sum_entropy / done;
		}
		std::cout << std::endl;
	}
	else {
		std::cout << "Error! : sr.sfen_for_mse.size() = " << sr.sfen_for_mse.size() << " ,  done = " << done << std::endl;
	}

	// Clear 0 for next time.
	learn_sum_cross_entropy_eval = 0.0;
	learn_sum_cross_entropy_win = 0.0;
	learn_sum_cross_entropy = 0.0;
	learn_sum_entropy_eval = 0.0;
	learn_sum_entropy_win = 0.0;
	learn_sum_entropy = 0.0;

}


void LearnerThink::thread_worker(size_t thread_id)
{
#if defined(_OPENMP)
	omp_set_num_threads(DynamicConfig::threads);
#endif

    Searcher & context = *ThreadPool::instance().at(thread_id);

	while (true)
	{
	// display mse (this is sometimes done only for thread 0)
	// Immediately after being read from the file...

	// Lock the evaluation function so that it is not used during updating.
	std::shared_lock<std::shared_timed_mutex> read_lock(nn_mutex, std::defer_lock);
	if (sr.next_update_weights <= sr.total_done ||
		    (thread_id != 0 && !read_lock.try_lock()))
		{
			if (thread_id != 0)
			{
				// Wait except thread_id == 0.

				if (stop_flag)
					break;

				// I want to parallelize rmse calculation etc., so if task() is loaded, process it.
				task_dispatcher.on_idle(thread_id);
				continue;
			}
			else
			{
				// Only thread_id == 0 performs the following update process.

				// The weight array is not updated for the first time.
				if (sr.next_update_weights == 0)
				{
					sr.next_update_weights += mini_batch_size;
					continue;
				}

				{
					// update parameters

					// Lock the evaluation function so that it is not used during updating.
					std::lock_guard<std::shared_timed_mutex> write_lock(nn_mutex);
					NNUE::UpdateParameters(epoch);
				}
				++epoch;

				// Save
				if (++sr.save_count * mini_batch_size >= eval_save_interval)
				{
					sr.save_count = 0;

					// During this time, as the gradient calculation proceeds, the value becomes too large and I feel annoyed, so stop other threads.
					const bool converged = save();
					if (converged)
					{
						stop_flag = true;
						sr.stop_flag = true;
						break;
					}
				}

				// Calculate rmse. This is done for samples of 10,000 phases.
				// If you do with 40 cores, update_weights every 1 million phases
				// I don't think it's so good to be tiring.
				static uint64_t loss_output_count = 0;
				if (++loss_output_count * mini_batch_size >= loss_output_interval)
				{
					loss_output_count = 0;

					// Number of cases processed this time
					uint64_t done = sr.total_done - sr.last_done;

					// loss calculation
					calc_loss(thread_id , done);

					NNUE::CheckHealth();

					// Make a note of how far you have totaled.
					sr.last_done = sr.total_done;
				}

				// Next time, I want you to do this series of processing again when you process only mini_batch_size.
				sr.next_update_weights += mini_batch_size;

				// Since I was waiting for the update of this sr.next_update_weights except the main thread,
				// Once this value is updated, it will start moving again.
			}
		}

		PackedSfenValue ps;

	RetryRead:;
		if (!sr.read_to_thread_buffer(thread_id, ps))
		{
			// ran out of thread pool for my thread.
			// Because there are almost no phases left,
			// Terminate all other threads.

			stop_flag = true;
			break;
		}

		// The evaluation value exceeds the learning target value.
		// Ignore this aspect information.
		if (eval_limit < abs(ps.score))
			goto RetryRead;

		if (!LearnerConfig::use_draw_games_in_training && ps.game_result == 0)
			goto RetryRead;

		// Skip over the opening phase
		if (ps.gamePly < prng.rand(reduction_gameply))
			goto RetryRead;

		const bool mirror = prng.rand(100) < mirror_percentage;
	    Position pos;
		if (set_from_packed_sfen(pos,ps.sfen,mirror) != 0)
		{
			std::cout << "Error! : illegal packed sfen = " << GetFEN(pos) << std::endl;
			goto RetryRead;
		}

		// There is a possibility that all the pieces are blocked and stuck.
        MoveList moves;
		MoveGen::generate(pos,moves);
		if ( moves.empty() )
			goto RetryRead;

		// Evaluation value of shallow search (qsearch)
		DepthType seldepth;
		PVList pv;
		/*auto r =*/ context.qsearchNoPruning(-MATE,MATE,pos,0,seldepth,&pv);

		// Evaluation value of deep search
		auto deep_value = (NNUEValue)ps.score;

		// I feel that the mini batch has a better gradient.
		// Go to the leaf node as it is, add only to the gradient array, and later try AdaGrad at the time of rmse aggregation.

		auto rootColor = pos.side_to_move();

		// A helper function that adds the gradient to the current phase.
		auto pos_add_grad = [&]() {
			// Use the value of evaluate in leaf as shallow_value.
			// Using the return value of qsearch() as shallow_value,
			// If PV is interrupted in the middle, the phase where evaluate() is called to calculate the gradient, and
			// I don't think this is a very desirable property, as the aspect that gives that gradient will be different.
			// I have turned off the substitution table, but since the pv array has not been updated due to one stumbling block etc...
            EvalData data;
			NNUEValue shallow_value = (rootColor == pos.side_to_move()) ? eval(pos,data,ThreadPool::instance().main()) : -eval(pos,data,ThreadPool::instance().main());

			// Calculate loss for training data
			double learn_cross_entropy_eval, learn_cross_entropy_win, learn_cross_entropy;
			double learn_entropy_eval, learn_entropy_win, learn_entropy;
			calc_cross_entropy(deep_value, shallow_value, ps, learn_cross_entropy_eval, learn_cross_entropy_win, learn_cross_entropy, learn_entropy_eval, learn_entropy_win, learn_entropy);
			learn_sum_cross_entropy_eval += learn_cross_entropy_eval;
			learn_sum_cross_entropy_win += learn_cross_entropy_win;
			learn_sum_cross_entropy += learn_cross_entropy;
			learn_sum_entropy_eval += learn_entropy_eval;
			learn_sum_entropy_win += learn_entropy_win;
			learn_sum_entropy += learn_entropy;

			const double example_weight = 1.0;
			NNUE::AddExample(pos, rootColor, ps, example_weight);

			// Since the processing is completed, the counter of the processed number is incremented
			sr.total_done++;
		};

        Position psave = pos;

		bool illegal_move = false;
		for (auto m : pv)
		{
			// Processing when adding the gradient to the node on each PV.
			// If discount_rate is 0, this process is not performed.
			if (discount_rate != 0)
				pos_add_grad();

            if ( !applyMove(pos,m)){
				illegal_move = true;
				break;
			}
			// Since the value of evaluate in leaf is used, the difference is updated.
			NNUE::update_eval(pos);
		}

		if (illegal_move) {
			std::cout << "An illical move was detected... Excluded the position from the learning data..." << std::endl;
			continue;
		}

		// Since we have reached the end phase of PV, add the slope here.
		pos_add_grad();

        // restore pos
		pos = psave;
	}
}

// Write evaluation function file.
bool LearnerThink::save(bool is_final)
{
	// Each time you save, change the extension part of the file name like "0","1","2",..
	// (Because I want to compare the winning rate for each evaluation function parameter later)

	if (save_only_once)
	{
		// When EVAL_SAVE_ONLY_ONCE is defined,
		// Do not dig a subfolder because I want to save it only once.
		NNUE::save_eval("");
	}
	else if (is_final) {
		NNUE::save_eval("final");
		return true;
	}
	else {
		static int dir_number = 0;
		const std::string dir_name = std::to_string(dir_number++);
		NNUE::save_eval(dir_name);

		if (newbob_decay != 1.0 && latest_loss_count > 0) {
			static int trials = newbob_num_trials;
			const double latest_loss = latest_loss_sum / latest_loss_count;
			latest_loss_sum = 0.0;
			latest_loss_count = 0;
			std::cout << "loss: " << latest_loss;
			if (latest_loss < best_loss) {
				std::cout << " < best (" << best_loss << "), accepted" << std::endl;
				best_loss = latest_loss;
				best_nn_directory = Path::Combine("evalsave", dir_name);
				trials = newbob_num_trials;
			} else {
				std::cout << " >= best (" << best_loss << "), rejected" << std::endl;
				if (best_nn_directory.empty()) {
					std::cout << "WARNING: no improvement from initial model" << std::endl;
				} else {
					std::cout << "restoring parameters from " << best_nn_directory << std::endl;
					NNUE::RestoreParameters(best_nn_directory);
				}
				if (--trials > 0 && !is_final) {
					std::cout << "reducing learning rate scale from " << newbob_scale
					          << " to " << (newbob_scale * newbob_decay)
					          << " (" << trials << " more trials)" << std::endl;
					newbob_scale *= newbob_decay;
					NNUE::SetGlobalLearningRateScale(newbob_scale);
				}
			}
			if (trials == 0) {
				std::cout << "converged" << std::endl;
				return true;
			}
		}
	}
	return false;
}

// Shuffle_files(), shuffle_files_quick() subcontracting, writing part.
// output_file_name: Name of the file to write
// prng: random number
// afs: fstream of each teacher phase file
// a_count: The number of teacher positions inherent in each file.
void shuffle_write(const std::string& output_file_name , FromSF::PRNG& prng , std::vector<std::fstream>& afs , std::vector<uint64_t>& a_count)
{
	uint64_t total_sfen_count = 0;
	for (auto c : a_count)
		total_sfen_count += c;

	// number of exported phases
	uint64_t write_sfen_count = 0;

	// Output the progress on the screen for each phase.
	const uint64_t buffer_size = 10000000;

	auto print_status = [&]()
	{
		// Output progress every 10M phase or when all writing is completed
		if (((write_sfen_count % buffer_size) == 0) ||
			(write_sfen_count == total_sfen_count))
			std::cout << write_sfen_count << " / " << total_sfen_count << std::endl;
	};


	std::cout << std::endl <<  "write : " << output_file_name << std::endl;

	std::fstream fs(output_file_name, std::ios::out | std::ios::binary);

	// total teacher positions
	uint64_t sum = 0;
	for (auto c : a_count)
		sum += c;

	while (sum != 0)
	{
		auto r = prng.rand(sum);

		// Aspects stored in fs[0] file ... Aspects stored in fs[1] file ...
		//Think of it as a series like, and determine in which file r is pointing.
		// The contents of the file are shuffled, so you can take the next element from that file.
		// Each file has a_count[x] phases, so this process can be written as follows.

		uint64_t n = 0;
		while (a_count[n] <= r)
			r -= a_count[n++];

		// This confirms n. Before you forget it, reduce the remaining number.

		--a_count[n];
		--sum;

		PackedSfenValue psv;
		// It's better to read and write all at once until the performance is not so good...
		if (afs[n].read((char*)&psv, sizeof(PackedSfenValue)))
		{
			fs.write((char*)&psv, sizeof(PackedSfenValue));
			++write_sfen_count;
			print_status();
		}
	}
	print_status();
	fs.close();
	std::cout << "done!" << std::endl;
}

// Subcontracting the teacher shuffle "learn shuffle" command.
// output_file_name: name of the output file where the shuffled teacher positions will be written
void shuffle_files(const std::vector<std::string>& filenames , const std::string& output_file_name , uint64_t buffer_size )
{
	// The destination folder is
	// tmp/ for temporary writing

	// Temporary file is written to tmp/ folder for each buffer_size phase.
	// For example, if buffer_size = 20M, you need a buffer of 20M*40bytes = 800MB.
	// In a PC with a small memory, it would be better to reduce this.
	// However, if the number of files increases too much, it will not be possible to open at the same time due to OS restrictions.
	// There should have been a limit of 512 per process on Windows, so you can open here as 500,
	// The current setting is 500 files x 20M = 10G = 10 billion phases.

	PSVector buf;
	buf.resize(buffer_size);
	// ↑ buffer, a marker that indicates how much you have used
	uint64_t buf_write_marker = 0;

	// File name to write (incremental counter because it is a serial number)
	uint64_t write_file_count = 0;

	// random number to shuffle
	// Do not use std::random_device().  Because it always the same integers on MinGW.
	FromSF::PRNG prng(std::chrono::system_clock::now().time_since_epoch().count());

	// generate the name of the temporary file
	auto make_filename = [](uint64_t i)
	{
		return "tmp/" + std::to_string(i) + ".bin";
	};

	// Exported files in tmp/ folder, number of teacher positions stored in each
	std::vector<uint64_t> a_count;

	auto write_buffer = [&](uint64_t size)
	{
		// shuffle from buf[0] to buf[size-1]
		for (uint64_t i = 0; i < size; ++i)
			std::swap(buf[i], buf[(uint64_t)(prng.rand(size - i) + i)]);

		// write to a file
		std::fstream fs;
		fs.open(make_filename(write_file_count++), std::ios::out | std::ios::binary);
		fs.write((char*)&buf[0], size * sizeof(PackedSfenValue));
		fs.close();
		a_count.push_back(size);

		buf_write_marker = 0;
		std::cout << ".";
	};

	Dependency::mkdir("tmp");

	// Shuffle and export as a 10M phase shredded file.
	for (auto filename : filenames)
	{
		std::fstream fs(filename, std::ios::in | std::ios::binary);
		std::cout << std::endl << "open file = " << filename;
		while (fs.read((char*)&buf[buf_write_marker], sizeof(PackedSfenValue)))
			if (++buf_write_marker == buffer_size)
				write_buffer(buffer_size);

		// Read in units of sizeof(PackedSfenValue),
		// Ignore the last remaining fraction. (Fails in fs.read, so exit while)
		// (The remaining fraction seems to be half-finished data that was created because it was stopped halfway during teacher generation.)

	}

	if (buf_write_marker != 0)
		write_buffer(buf_write_marker);

	// Only shuffled files have been written write_file_count.
	// As a second pass, if you open all of them at the same time, select one at random and load one phase at a time
	// Now you have shuffled.

	// Original file for shirt full + tmp file + file to write requires 3 times the storage capacity of the original file.
	// 1 billion SSD is not enough for shuffling because it is 400GB for 10 billion phases.
	// If you want to delete (or delete by hand) the original file at this point after writing to tmp,
	// The storage capacity is about twice that of the original file.
	// So, maybe we should have an option to delete the original file.

	// Files are opened at the same time. It is highly possible that this will exceed FOPEN_MAX.
	// In that case, rather than adjusting buffer_size to reduce the number of files.

	std::vector<std::fstream> afs;
	for (uint64_t i = 0; i < write_file_count; ++i)
		afs.emplace_back(std::fstream(make_filename(i),std::ios::in | std::ios::binary));

	// Throw to the subcontract function and end.
	shuffle_write(output_file_name, prng, afs, a_count);
}

// Subcontracting the teacher shuffle "learn shuffleq" command.
// This is written in 1 pass.
// output_file_name: name of the output file where the shuffled teacher positions will be written
void shuffle_files_quick(const std::vector<std::string>& filenames, const std::string& output_file_name)
{
	// random number to shuffle
	// Do not use std::random_device().  Because it always the same integers on MinGW.
	FromSF::PRNG prng(std::chrono::system_clock::now().time_since_epoch().count());

	// number of files
	size_t file_count = filenames.size();

	// Number of teacher positions stored in each file in filenames
	std::vector<uint64_t> a_count(file_count);

	// Count the number of teacher aspects in each file.
	std::vector<std::fstream> afs(file_count);

	for (size_t i = 0; i <file_count ;++i)
	{
		auto filename = filenames[i];
		auto& fs = afs[i];

		fs.open(filename, std::ios::in | std::ios::binary);
		fs.seekg(0, std::fstream::end);
		uint64_t eofPos = (uint64_t)fs.tellg();
		fs.clear(); // Otherwise, the next seek may fail.
		fs.seekg(0, std::fstream::beg);
		uint64_t begPos = (uint64_t)fs.tellg();
		uint64_t file_size = eofPos - begPos;
		uint64_t sfen_count = file_size / sizeof(PackedSfenValue);
		a_count[i] = sfen_count;

		// Output the number of sfen stored in each file.
		std::cout << filename << " = " << sfen_count << " sfens." << std::endl;
	}

	// Since we know the file size of each file,
	// open them all at once (already open),
	// Select one at a time and load one phase at a time
	// Now you have shuffled.

	// Throw to the subcontract function and end.
	shuffle_write(output_file_name, prng, afs, a_count);
}

int read_file_to_memory(std::string filename, std::function<void* (uint64_t)> callback_func)
{
    std::fstream fs(filename, std::ios::in | std::ios::binary);
    if (fs.fail())
        return 1;

    fs.seekg(0, std::fstream::end);
    uint64_t eofPos = (uint64_t)fs.tellg();
    fs.clear(); // Otherwise the next seek may fail.
    fs.seekg(0, std::fstream::beg);
    uint64_t begPos = (uint64_t)fs.tellg();
    uint64_t file_size = eofPos - begPos;
    //std::cout << "filename = " << filename << " , file_size = " << file_size << endl;

    // I know the file size, so call callback_func to get a buffer for this,
    // Get the pointer.
    void* ptr = callback_func(file_size);

    // If the buffer could not be secured, or if the file size is different from the expected file size,
    // It is supposed to return nullptr. At this time, reading is interrupted and an error is returned.
    if (ptr == nullptr)
        return 2;

    // read in pieces

    const uint64_t block_size = 1024 * 1024 * 1024; // number of elements to read in one read (1GB)
    for (uint64_t pos = 0; pos < file_size; pos += block_size)
    {
        // size to read this time
        uint64_t read_size = (pos + block_size < file_size) ? block_size : (file_size - pos);
        fs.read((char*)ptr + pos, read_size);

        // Read error occurred in the middle of the file.
        if (fs.fail())
            return 2;

        //cout << ".";
    }
    fs.close();

    return 0;
}

int write_memory_to_file(std::string filename, void* ptr, uint64_t size)
{
    std::fstream fs(filename, std::ios::out | std::ios::binary);
    if (fs.fail())
        return 1;

    const uint64_t block_size = 1024 * 1024 * 1024; // number of elements to write in one write (1GB)
    for (uint64_t pos = 0; pos < size; pos += block_size)
    {
        // Memory size to write this time
        uint64_t write_size = (pos + block_size < size) ? block_size : (size - pos);
        fs.write((char*)ptr + pos, write_size);
        //cout << ".";
    }
    fs.close();
    return 0;
}

// Subcontracting the teacher shuffle "learn shufflem" command.
// Read the whole memory and write it out with the specified file name.
void shuffle_files_on_memory(const std::vector<std::string>& filenames,const std::string output_file_name)
{
	PSVector buf;

	for (auto filename : filenames)
	{
		std::cout << "read : " << filename << std::endl;
		read_file_to_memory(filename, [&buf](uint64_t size) {
			assert((size % sizeof(PackedSfenValue)) == 0);
			// Expand the buffer and read after the last end.
			uint64_t last = buf.size();
			buf.resize(last + size / sizeof(PackedSfenValue));
			return (void*)&buf[last];
		});
	}

	// shuffle from buf[0] to buf[size-1]
	// Do not use std::random_device().  Because it always the same integers on MinGW.
	FromSF::PRNG prng(std::chrono::system_clock::now().time_since_epoch().count());
	uint64_t size = (uint64_t)buf.size();
	std::cout << "shuffle buf.size() = " << size << std::endl;
	for (uint64_t i = 0; i < size; ++i)
		std::swap(buf[i], buf[(uint64_t)(prng.rand(size - i) + i)]);

	std::cout << "write : " << output_file_name << std::endl;

	// If the file to be written exceeds 2GB, it cannot be written in one shot with fstream::write, so use wrapper.
	write_memory_to_file(output_file_name, (void*)&buf[0], (uint64_t)sizeof(PackedSfenValue)*(uint64_t)buf.size());

	std::cout << "..shuffle_on_memory done." << std::endl;
}


// Learning from the generated game record
void learn(std::istringstream& is)
{
	auto thread_num = DynamicConfig::threads;
	SfenReader sr(thread_num);

	LearnerThink learn_think(sr);
	std::vector<std::string> filenames;

	// mini_batch_size 1M aspect by default. This can be increased.
	auto mini_batch_size = 1000*1000;

	// Number of loops (read the game record file this number of times)
	int loop = 1;

	// Game file storage folder (get game file with relative path from here)
	std::string base_dir;

	std::string target_dir;

	// If 0, it will be the default value.
	double eta1 = 0.0;
	double eta2 = 0.0;
	double eta3 = 0.0;
	uint64_t eta1_epoch = 0; // eta2 is not applied by default
	uint64_t eta2_epoch = 0; // eta3 is not applied by default

	// normal shuffle
	bool shuffle_normal = false;
	uint64_t buffer_size = 20000000;
	// fast shuffling assuming each file is shuffled
	bool shuffle_quick = false;
	// A function to read the entire file in memory and shuffle it. (Requires file size memory)
	bool shuffle_on_memory = false;
    bool interpolate_eval = 0;
	bool check_invalid_fen = false;
	bool check_illegal_move = false;
	// convert teacher in pgn-extract format to Yaneura King's bin
	bool pgn_eval_side_to_move = false;
	bool convert_no_eval_fens_as_score_zero = false;
	// File name to write in those cases (default is "shuffled_sfen.bin")
	std::string output_file_name = "shuffled_sfen.bin";

	// If the absolute value of the evaluation value in the deep search of the teacher phase exceeds this value, that phase is discarded.
	int eval_limit = 32000;

	// Flag to save the evaluation function file only once near the end.
	bool save_only_once = false;

	// Shuffle about what you are pre-reading on the teacher aspect. (Shuffle of about 10 million phases)
	// Turn on if you want to pass a pre-shuffled file.
	bool no_shuffle = false;

	// elmo lambda
	ELMO_LAMBDA = 0.33;
	ELMO_LAMBDA2 = 0.33;
	ELMO_LAMBDA_LIMIT = 32000;

	// Discount rate. If this is set to a value other than 0, the slope will be added even at other than the PV termination. (At that time, apply this discount rate)
	double discount_rate = 0;

	// if (gamePly <rand(reduction_gameply)) continue;
	// An option to exclude the early stage from the learning target moderately like
	// If set to 1, rand(1)==0, so nothing is excluded.
	int reduction_gameply = 1;

	// Optional item that does not let you learn KK/KKP/KPP/KPPP
	std::array<bool,4> freeze = {};

	uint64_t nn_batch_size = 1000;
	double newbob_decay = 1.0;
	int newbob_num_trials = 2;
	std::string nn_options;

	uint64_t eval_save_interval = 1000000000ULL;
	uint64_t loss_output_interval = 0;
	uint64_t mirror_percentage = 0;

	std::string validation_set_file_name;

	// Assume the filenames are staggered.
	while (true)
	{
		std::string option;
		is >> option;

		if (option == "")
			break;

		// specify the number of phases of mini-batch
		if (option == "bat")
		{
			is >> mini_batch_size;
			mini_batch_size *= 10000; // Unit is ten thousand
		}

		// Specify the folder in which the game record is stored and make it the rooting target.
		else if (option == "targetdir") is >> target_dir;

		// Specify the number of loops
		else if (option == "loop")      is >> loop;

		// Game file storage folder (get game file with relative path from here)
		else if (option == "basedir")   is >> base_dir;

		// Mini batch size
		else if (option == "batchsize") is >> mini_batch_size;

		// learning rate
		else if (option == "eta")        is >> eta1;
		else if (option == "eta1")       is >> eta1; // alias
		else if (option == "eta2")       is >> eta2;
		else if (option == "eta3")       is >> eta3;
		else if (option == "eta1_epoch") is >> eta1_epoch;
		else if (option == "eta2_epoch") is >> eta2_epoch;
		else if (option == "use_draw_in_training" || option == "use_draw_games_in_training") is >> LearnerConfig::use_draw_games_in_training;
		else if (option == "use_draw_in_validation" || option == "use_draw_games_in_validation") is >> LearnerConfig::use_draw_games_in_validation;
		else if (option == "winning_probability_coefficient") is >> LearnerConfig::winning_probability_coefficient;
		else if (option == "discount_rate") is >> discount_rate;
		else if (option == "use_wdl") is >> LearnerConfig::use_wdl;
		else if (option == "freeze_kk")    is >> freeze[0];
		else if (option == "freeze_kkp")   is >> freeze[1];
		else if (option == "freeze_kpp")   is >> freeze[2];
#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_KPP_KKPT_FV_VAR) || defined(EVAL_NABLA)

#elif defined(EVAL_KPPPT) || defined(EVAL_KPPP_KKPT) || defined(EVAL_HELICES)
		else if (option == "freeze_kppp")  is >> freeze[3];
#elif defined(EVAL_KKPP_KKPT) || defined(EVAL_KKPPT)
		else if (option == "freeze_kkpp")  is >> freeze[3];
#endif

		// LAMBDA
		else if (option == "lambda")       is >> ELMO_LAMBDA;
		else if (option == "lambda2")      is >> ELMO_LAMBDA2;
		else if (option == "lambda_limit") is >> ELMO_LAMBDA_LIMIT;

		else if (option == "reduction_gameply") is >> reduction_gameply;

		// shuffle related
		else if (option == "shuffle")	shuffle_normal = true;
		else if (option == "buffer_size") is >> buffer_size;
		else if (option == "shuffleq")	shuffle_quick = true;
		else if (option == "shufflem")	shuffle_on_memory = true;
		else if (option == "output_file_name") is >> output_file_name;

		else if (option == "eval_limit") is >> eval_limit;
		else if (option == "save_only_once") save_only_once = true;
		else if (option == "no_shuffle") no_shuffle = true;

		else if (option == "nn_batch_size") is >> nn_batch_size;
		else if (option == "newbob_decay") is >> newbob_decay;
		else if (option == "newbob_num_trials") is >> newbob_num_trials;
		else if (option == "nn_options") is >> nn_options;

		else if (option == "eval_save_interval") is >> eval_save_interval;
		else if (option == "loss_output_interval") is >> loss_output_interval;
		else if (option == "mirror_percentage") is >> mirror_percentage;
		else if (option == "validation_set_file_name") is >> validation_set_file_name;

		// Rabbit convert related
		else if (option == "interpolate_eval") is >> interpolate_eval;
		else if (option == "check_invalid_fen") is >> check_invalid_fen;
		else if (option == "check_illegal_move") is >> check_illegal_move;
		else if (option == "pgn_eval_side_to_move") is >> pgn_eval_side_to_move;
		else if (option == "convert_no_eval_fens_as_score_zero") is >> convert_no_eval_fens_as_score_zero;
		else if (option == "src_score_min_value") is >> LearnerConfig::src_score_min_value;
		else if (option == "src_score_max_value") is >> LearnerConfig::src_score_max_value;
		else if (option == "dest_score_min_value") is >> LearnerConfig::dest_score_min_value;
		else if (option == "dest_score_max_value") is >> LearnerConfig::dest_score_max_value;
		else if (option == "convert_teacher_signal_to_winning_probability") is >> LearnerConfig::convert_teacher_signal_to_winning_probability;

		// Otherwise, it's a filename.
		else
			filenames.push_back(option);
	}
	if (loss_output_interval == 0)
		loss_output_interval = mini_batch_size;

	std::cout << "learn command , ";

	// Issue a warning if OpenMP is disabled.
#if !defined(_OPENMP)
	std::cout << "Warning! OpenMP disabled." << std::endl;
#endif

	// Display learning game file
	if (target_dir != "")
	{
		std::string kif_base_dir = Path::Combine(base_dir, target_dir);

		// Remove this folder. Keep it relative to base_dir.
		auto ends_with = [](std::string const & value, std::string const & ending)
		{
			if (ending.size() > value.size()) return false;
			return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
		};

		// It can't be helped, so read it using dirent.h.
		DIR *dp; // pointer to directory
		dirent* entry; // entry point returned by readdir()

		dp = opendir(kif_base_dir.c_str());
		if (dp != NULL)
		{
			do {
				entry = readdir(dp);
				// Only list files ending with ".bin"
				// →I hate this restriction when generating files with serial numbers...
				if (entry != NULL  && ends_with(entry->d_name, ".bin")  )
				{
					//cout << entry->d_name << endl;
					filenames.push_back(Path::Combine(target_dir, entry->d_name));
				}
			} while (entry != NULL);
			closedir(dp);
		}
	}

	std::cout << "learn from ";
	for (auto s : filenames)
		std::cout << s << " , ";
	std::cout << std::endl;
	if (!validation_set_file_name.empty())
	{
		std::cout << "validation set  : " << validation_set_file_name << std::endl;
	}

	std::cout << "base dir        : " << base_dir   << std::endl;
	std::cout << "target dir      : " << target_dir << std::endl;

	// shuffle mode
	if (shuffle_normal)
	{
		std::cout << "buffer_size     : " << buffer_size << std::endl;
		std::cout << "shuffle mode.." << std::endl;
		shuffle_files(filenames,output_file_name , buffer_size);
		return;
	}
	if (shuffle_quick)
	{
		std::cout << "quick shuffle mode.." << std::endl;
		shuffle_files_quick(filenames, output_file_name);
		return;
	}
	if (shuffle_on_memory)
	{
		std::cout << "shuffle on memory.." << std::endl;
		shuffle_files_on_memory(filenames,output_file_name);
		return;
	}

	std::cout << "loop              : " << loop << std::endl;
	std::cout << "eval_limit        : " << eval_limit << std::endl;
	std::cout << "save_only_once    : " << (save_only_once ? "true" : "false") << std::endl;
	std::cout << "no_shuffle        : " << (no_shuffle ? "true" : "false") << std::endl;

	// Insert the file name for the number of loops.
	for (int i = 0; i < loop; ++i)
		// sfen reader, I'll read it in reverse order so I'll reverse it here. I'm sorry.
		for (auto it = filenames.rbegin(); it != filenames.rend(); ++it)
			sr.filenames.push_back(Path::Combine(base_dir, *it));

	std::cout << "Loss Function     : " << "Elmo method"     << std::endl;
	std::cout << "mini-batch size   : " << mini_batch_size   << std::endl;
	std::cout << "nn_batch_size     : " << nn_batch_size     << std::endl;
	std::cout << "nn_options        : " << nn_options        << std::endl;
	std::cout << "learning rate     : " << eta1 << " , " << eta2 << " , " << eta3 << std::endl;
	std::cout << "eta_epoch         : " << eta1_epoch << " , " << eta2_epoch << std::endl;
	std::cout << "use_draw_games_in_training : " << LearnerConfig::use_draw_games_in_training << std::endl;
	std::cout << "use_draw_games_in_validation : " << LearnerConfig::use_draw_games_in_validation << std::endl;
	if (newbob_decay != 1.0) {
		std::cout << "scheduling        : newbob with decay = " << newbob_decay
		     << ", " << newbob_num_trials << " trials" << std::endl;
	} else {
		std::cout << "scheduling        : default" << std::endl;
	}
	std::cout << "discount rate     : " << discount_rate     << std::endl;

	// If reduction_gameply is set to 0, rand(0) will be divided by 0, so correct it to 1.
	reduction_gameply = std::max(reduction_gameply, 1);
	std::cout << "reduction_gameply : " << reduction_gameply << std::endl;

	std::cout << "LAMBDA            : " << ELMO_LAMBDA       << std::endl;
	std::cout << "LAMBDA2           : " << ELMO_LAMBDA2      << std::endl;
	std::cout << "LAMBDA_LIMIT      : " << ELMO_LAMBDA_LIMIT << std::endl;

	std::cout << "mirror_percentage : " << mirror_percentage << std::endl;
	std::cout << "eval_save_interval  : " << eval_save_interval << " sfens" << std::endl;
	std::cout << "loss_output_interval: " << loss_output_interval << " sfens" << std::endl;

	// -----------------------------------
	// various initialization
	// -----------------------------------

	std::cout << "init.." << std::endl;

	// Read evaluation function parameters
	NNUEWrapper::init_NNUE();

	std::cout << "init_training.." << std::endl;
	NNUE::InitializeTraining(eta1,eta1_epoch,eta2,eta2_epoch,eta3);
	NNUE::SetBatchSize(nn_batch_size);
	NNUE::SetOptions(nn_options);

	if (newbob_decay != 1.0 && !DynamicConfig::skipLoadingEval) {
		learn_think.best_nn_directory = "Tourney"; // get the lastest nn.bin inside Tourney ?
	}

	std::cout << "init done." << std::endl;

	// Reflect other option settings.
	learn_think.discount_rate = discount_rate;
	learn_think.eval_limit = eval_limit;
	learn_think.save_only_once = save_only_once;
	learn_think.sr.no_shuffle = no_shuffle;
	learn_think.freeze = freeze;
	learn_think.reduction_gameply = reduction_gameply;
	learn_think.newbob_scale = 1.0;
	learn_think.newbob_decay = newbob_decay;
	learn_think.newbob_num_trials = newbob_num_trials;
	learn_think.eval_save_interval = eval_save_interval;
	learn_think.loss_output_interval = loss_output_interval;
	learn_think.mirror_percentage = mirror_percentage;

	// Start a thread that loads the phase file in the background
	// (If this is not started, mse cannot be calculated.)
	learn_think.start_file_read_worker();

	learn_think.mini_batch_size = mini_batch_size;

	if (validation_set_file_name.empty()) {
	// Get about 10,000 data for mse calculation.
		sr.read_for_mse();
	} else {
		sr.read_validation_set(validation_set_file_name, eval_limit);
	}

	// Calculate rmse once at this point (timing of 0 sfen)
	// sr.calc_rmse();
	if (newbob_decay != 1.0) {
		learn_think.calc_loss(0, -1);
		learn_think.best_loss = learn_think.latest_loss_sum / learn_think.latest_loss_count;
		learn_think.latest_loss_sum = 0.0;
		learn_think.latest_loss_count = 0;
		std::cout << "initial loss: " << learn_think.best_loss << std::endl;
	}

	// -----------------------------------
	// start learning evaluation function parameters
	// -----------------------------------

	// Start learning.
	learn_think.go_think();

	// Save once at the end.
	learn_think.save(true);

}

#endif