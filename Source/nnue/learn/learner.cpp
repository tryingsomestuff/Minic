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

bool use_draw_games_in_training = false;
bool use_draw_games_in_validation = false;
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
	return sigmoid(value * LearnerConfig::winning_probability_coefficient);
}

// A function that converts the evaluation value to the winning rate [0,1]
double winning_percentage_wdl(double value, int ply)
{
	constexpr double wdl_total = 1000.0;
	constexpr double draw_score = 0.5;

	const double wdl_w = FromSF::win_rate_model_double(value, ply);
	const double wdl_l = FromSF::win_rate_model_double(-value, ply);
	const double wdl_d = wdl_total - wdl_w - wdl_l;

	return (wdl_w + wdl_d * draw_score) / wdl_total;
}

// A function that converts the evaluation value to the winning rate [0,1]
double winning_percentage(double value, int ply)
{
	if (LearnerConfig::use_wdl)
	{
		return winning_percentage_wdl(value, ply);
	}
	else
	{
		return winning_percentage(value);
	}
}

double calc_cross_entropy_of_winning_percentage(
	double deep_win_rate,
	double shallow_eval,
	int ply)
{
	const double p = deep_win_rate;
	const double q = winning_percentage(shallow_eval, ply);
	return -p * std::log(q) - (1.0 - p) * std::log(1.0 - q);
}

double calc_d_cross_entropy_of_winning_percentage(
	double deep_win_rate,
	double shallow_eval,
	int ply)
{
	constexpr double epsilon = 0.000001;

	const double y1 = calc_cross_entropy_of_winning_percentage(
		deep_win_rate, shallow_eval, ply);

	const double y2 = calc_cross_entropy_of_winning_percentage(
		deep_win_rate, shallow_eval + epsilon, ply);

	// Divide by the winning_probability_coefficient to
	// match scale with the sigmoidal win rate
	return ((y2 - y1) / epsilon) / LearnerConfig::winning_probability_coefficient;
}

namespace{
// A constant used in elmo (WCSC27). Adjustment required.
// Since elmo does not internally divide the expression, the value is different.
// You can set this value with the learn command.
// 0.33 is equivalent to the constant (0.5) used in elmo (WCSC27)
double ELMO_LAMBDA = 0.33;
double ELMO_LAMBDA2 = 0.33;
double ELMO_LAMBDA_LIMIT = 32000;

typedef std::chrono::system_clock Clock;
std::chrono::time_point<Clock> startTime;
}

// Training Formula · Issue #71 · nodchip/Stockfish https://github.com/nodchip/Stockfish/issues/71
double get_scaled_signal(double signal)
{
	double scaled_signal = signal;

	// Normalize to [0.0, 1.0].
	scaled_signal =
		(scaled_signal - LearnerConfig::src_score_min_value)
		/ (LearnerConfig::src_score_max_value - LearnerConfig::src_score_min_value);

	// Scale to [dest_score_min_value, dest_score_max_value].
	scaled_signal =
		scaled_signal * (LearnerConfig::dest_score_max_value - LearnerConfig::dest_score_min_value)
		+ LearnerConfig::dest_score_min_value;

	return scaled_signal;
}

// Teacher winning probability.
double calculate_p(double teacher_signal, int ply)
{
	const double scaled_teacher_signal = get_scaled_signal(teacher_signal);
	return winning_percentage(scaled_teacher_signal, ply);
}

double calculate_lambda(double teacher_signal)
{
	// If the evaluation value in deep search exceeds ELMO_LAMBDA_LIMIT
	// then apply ELMO_LAMBDA2 instead of ELMO_LAMBDA.
	const double lambda =
		(std::abs(teacher_signal) >= ELMO_LAMBDA_LIMIT)
		? ELMO_LAMBDA2
		: ELMO_LAMBDA;

	return lambda;
}

double calculate_t(int game_result)
{
	// Use 1 as the correction term if the expected win rate is 1,
	// 0 if you lose, and 0.5 if you draw.
	// game_result = 1,0,-1 so add 1 and divide by 2.
	const double t = double(game_result + 1) * 0.5;

	return t;
}

double calc_grad(NNUEValue teacher_signal, NNUEValue shallow, const PackedSfenValue& psv)
{
	// elmo (WCSC27) method
	// Correct with the actual game wins and losses.
	const double q = winning_percentage(shallow, psv.gamePly);
	const double p = calculate_p(teacher_signal, psv.gamePly);
	const double t = calculate_t(psv.game_result);
	const double lambda = calculate_lambda(teacher_signal);

	double grad;
	if (LearnerConfig::use_wdl)
	{
		const double dce_p = calc_d_cross_entropy_of_winning_percentage(p, shallow, psv.gamePly);
		const double dce_t = calc_d_cross_entropy_of_winning_percentage(t, shallow, psv.gamePly);
		grad = lambda * dce_p + (1.0 - lambda) * dce_t;
	}
	else
	{
		// Use the actual win rate as a correction term.
		// This is the idea of ​​elmo (WCSC27), modern O-parts.
		grad = lambda * (q - p) + (1.0 - lambda) * (q - t);
	}

	return grad;
}

// Calculate cross entropy during learning
// The individual cross entropy of the win/loss term and win
// rate term of the elmo expression is returned
// to the arguments cross_entropy_eval and cross_entropy_win.
void calc_cross_entropy(
	NNUEValue teacher_signal,
	NNUEValue shallow,
	const PackedSfenValue& psv,
	double& cross_entropy_eval,
	double& cross_entropy_win,
	double& cross_entropy,
	double& entropy_eval,
	double& entropy_win,
	double& entropy)
{
	// Teacher winning probability.
	const double q = winning_percentage(shallow, psv.gamePly);
	const double p = calculate_p(teacher_signal, psv.gamePly);
	const double t = calculate_t(psv.game_result);
	const double lambda = calculate_lambda(teacher_signal);

	constexpr double epsilon = 0.000001;

	const double m = (1.0 - lambda) * t + lambda * p;

	cross_entropy_eval =
		(-p * std::log(q + epsilon) - (1.0 - p) * std::log(1.0 - q + epsilon));
	cross_entropy_win =
		(-t * std::log(q + epsilon) - (1.0 - t) * std::log(1.0 - q + epsilon));
	entropy_eval =
		(-p * std::log(p + epsilon) - (1.0 - p) * std::log(1.0 - p + epsilon));
	entropy_win =
		(-t * std::log(t + epsilon) - (1.0 - t) * std::log(1.0 - t + epsilon));

	cross_entropy =
		(-m * std::log(q + epsilon) - (1.0 - m) * std::log(1.0 - q + epsilon));
	entropy =
		(-m * std::log(m + epsilon) - (1.0 - m) * std::log(1.0 - m + epsilon));
}

// Class to generate sfen with multiple threads
struct LearnerThink: public MultiThink
{
	LearnerThink(SfenReader& sr_, const std::string& seed) : 
	   MultiThink(seed),
	   sr(sr_),stop_flag(false), save_only_once(false)
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

    NNUEValue get_shallow_value(Position& task_pos, size_t thread_id);
  
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

	std::atomic<bool> stop_flag;

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

	// Loss calculation.
	// done: Number of phases targeted this time
	void calc_loss(size_t thread_id , uint64_t done);

	// Define the loss calculation in as a task and execute it
	TaskDispatcher task_dispatcher;
};

NNUEValue LearnerThink::get_shallow_value(Position& task_pos, size_t thread_id)
{

	// Evaluation value for shallow search
	// The value of evaluate() may be used, but when calculating loss, learn_cross_entropy and
	// Use qsearch() because it is difficult to compare the values.
	// EvalHash has been disabled in advance. (If not, the same value will be returned every time)
	DepthType seldepth = 0;
	PVList pv;
	Searcher & contxt = *ThreadPool::instance().at(thread_id);

    Position p2 = task_pos;
	NNUE::Accumulator acc;
	p2.setAccumulator(acc);

	contxt.qsearchNoPruning(-MATE,MATE,p2,0,seldepth,&pv);

	const auto rootColor = p2.side_to_move();

	for (auto m : pv){
		if ( !applyMove(p2,m,false)) break;
	}

	EvalData data;
	Searcher & context = *ThreadPool::instance().at(thread_id);
	const NNUEValue shallow_value =
		(rootColor == p2.side_to_move())
		? eval(p2,data,context)
		: -eval(p2,data,context);

	return shallow_value;
}

namespace{
	std::string now_string()
	{
	#if defined(_MSC_VER)
	// C4996 : 'ctime' : This function or variable may be unsafe.Consider using ctime_s instead.
	#pragma warning(disable : 4996)
	#endif
		auto now = std::chrono::system_clock::now();
		auto tp = std::chrono::system_clock::to_time_t(now);
		auto result = std::string(std::ctime(&tp));
		while (*result.rbegin() == '\n' || (*result.rbegin() == '\r')) result.pop_back();
		return result + " ";
	}	
}

void LearnerThink::calc_loss(size_t thread_id, uint64_t done)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
	std::cout << now_string() << "PROGRESS: "  << ", ";
	std::cout << sr.total_done << " sfens" << ", ";
	std::cout << sr.total_done * 1000 / elapsed  << " sfens/second, ";
	std::cout << "iteration " << epoch;

	// For calculation of verification data loss
	std::atomic<double> test_sum_cross_entropy_eval, test_sum_cross_entropy_win, test_sum_cross_entropy;
	std::atomic<double> test_sum_entropy_eval, test_sum_entropy_win, test_sum_entropy;
	test_sum_cross_entropy_eval = 0;
	test_sum_cross_entropy_win = 0;
	test_sum_cross_entropy = 0;
	test_sum_entropy_eval = 0;
	test_sum_entropy_win = 0;
	test_sum_entropy = 0;

	// norm for learning
	std::atomic<double> sum_norm;
	sum_norm = 0;

	// The number of times the pv first move of deep
	// search matches the pv first move of search(1).
	std::atomic<int> move_accord_count;
	move_accord_count = 0;

	// Display the value of eval() in the initial stage of Hirate and see the shaking.
	{
	Position pos;
    readFEN(startPosition,pos,true);
	NNUE::Accumulator acc;
	pos.setAccumulator(acc);	
	EvalData data;
	Searcher & context = *ThreadPool::instance().at(thread_id);
    std::cout << ", hirate eval = " << eval(pos,data,context) << std::endl;
	}

	// It's better to parallelize here, but it's a bit
	// troublesome because the search before slave has not finished.
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
		// It is not possible to capture pos used in ↑,
		// so specify the variables you want to capture one by one.
		auto task =
			[
				this,
				&ps,
				&test_sum_cross_entropy_eval,
				&test_sum_cross_entropy_win,
				&test_sum_cross_entropy,
				&test_sum_entropy_eval,
				&test_sum_entropy_win,
				&test_sum_entropy,
				&sum_norm,
				&task_count,
				&move_accord_count
			](size_t task_thread_id)
		{
			// Does C++ properly capture a new ps instance for each loop?.
            Position task_pos;
			if (set_from_packed_sfen(task_pos,*const_cast<PackedSfen*>(&ps.sfen)) != 0){
				// Unfortunately, as an sfen for rmse calculation, an invalid sfen was drawn.
				std::cout << "Error! : illegal packed sfen " << GetFEN(task_pos) << std::endl;
			}

			const NNUEValue shallow_value = get_shallow_value(task_pos,task_thread_id);

			// Evaluation value of deep search
			auto deep_value = (NNUEValue)ps.score;

			// Note) This code does not consider when
			//       eval_limit is specified in the learn command.

			// --- calculation of cross entropy

			// For the time being, regarding the win rate and loss terms only in the elmo method
			// Calculate and display the cross entropy.

			double test_cross_entropy_eval, test_cross_entropy_win, test_cross_entropy;
			double test_entropy_eval, test_entropy_win, test_entropy;
			calc_cross_entropy(
				deep_value,
				shallow_value,
				ps,
				test_cross_entropy_eval,
				test_cross_entropy_win,
				test_cross_entropy,
				test_entropy_eval,
				test_entropy_win,
				test_entropy);

			// The total cross entropy need not be abs() by definition.
			test_sum_cross_entropy_eval += test_cross_entropy_eval;
			test_sum_cross_entropy_win += test_cross_entropy_win;
			test_sum_cross_entropy += test_cross_entropy;
			test_sum_entropy_eval += test_entropy_eval;
			test_sum_entropy_win += test_entropy_win;
			test_sum_entropy += test_entropy;
			sum_norm += (double)abs(shallow_value);

			// Determine if the teacher's move and the score of the shallow search match
			{
				TimeMan::isDynamic       = false;
				TimeMan::nbMoveInTC      = -1;
				TimeMan::msecPerMove     = INFINITETIME;
				TimeMan::msecInTC        = -1;
				TimeMan::msecInc         = -1;
				TimeMan::msecUntilNextTC = -1;
				DynamicConfig::quiet = true;
				ThreadPool::instance().currentMoveMs = TimeMan::GetNextMSecPerMove(task_pos);
				PVList pv;
				ThreadData d = {1,0,0,task_pos,INVALIDMOVE,pv,SearchData()}; // only input coef is depth here
				ThreadPool::instance().main().subSearch = true;
				ThreadPool::instance().search(d);
				Move bestMove = ThreadPool::instance().main().getData().best; // here output results
				ThreadPool::instance().main().subSearch = false;
				DynamicConfig::quiet = false;
				if (sameMove(FromSFMove(task_pos,ps.move),bestMove))
					move_accord_count.fetch_add(1, std::memory_order_relaxed);
			}

			// Reduced one task because I did it
			--task_count;
		};

		// Throw the defined task to slave.
		task_dispatcher.push_task_async(task);
	}

	// join yourself as a slave
	task_dispatcher.on_idle(thread_id);

	// wait for all tasks to complete
	while (task_count){
		sleep(1);
	}

	latest_loss_sum += test_sum_cross_entropy - test_sum_entropy;
	latest_loss_count += sr.sfen_for_mse.size();

	// learn_cross_entropy may be called train cross
	// entropy in the world of machine learning,
	// When omitting the acronym, it is nice to be able to
	// distinguish it from test cross entropy(tce) by writing it as lce.

	if (sr.sfen_for_mse.size() && done)
	{
		std::cout << now_string()
			<< " , test_cross_entropy_eval = " << test_sum_cross_entropy_eval / sr.sfen_for_mse.size()
			<< " , test_cross_entropy_win = " << test_sum_cross_entropy_win / sr.sfen_for_mse.size()
			<< " , test_entropy_eval = " << test_sum_entropy_eval / sr.sfen_for_mse.size()
			<< " , test_entropy_win = " << test_sum_entropy_win / sr.sfen_for_mse.size()
			<< " , test_cross_entropy = " << test_sum_cross_entropy / sr.sfen_for_mse.size()
			<< " , test_entropy = " << test_sum_entropy / sr.sfen_for_mse.size()
			<< " , norm = " << sum_norm
			<< " , move accuracy = " << (move_accord_count * 100.0 / sr.sfen_for_mse.size()) << "%";

		if (done != static_cast<uint64_t>(-1))
		{
			std::cout << now_string()
				<< " , learn_cross_entropy_eval = " << learn_sum_cross_entropy_eval / done
				<< " , learn_cross_entropy_win = " << learn_sum_cross_entropy_win / done
				<< " , learn_entropy_eval = " << learn_sum_entropy_eval / done
				<< " , learn_entropy_win = " << learn_sum_entropy_win / done
				<< " , learn_cross_entropy = " << learn_sum_cross_entropy / done
				<< " , learn_entropy = " << learn_sum_entropy / done;
		}
		std::cout << std::endl;
	}
	else
	{
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
					NNUE::UpdateParameters();
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
		if (eval_limit < abs(ps.score)){
			std::cout << "skipping bad eval" << std::endl;
			goto RetryRead;
		}

		if (!LearnerConfig::use_draw_games_in_training && ps.game_result == 0){
			std::cout << "skipping draw" << std::endl;
			goto RetryRead;
		}			

		// Skip over the opening phase
		if (ps.gamePly < prng.rand(reduction_gameply)){
			std::cout << "skipping game ply " << ps.gamePly << std::endl;
			goto RetryRead;
		}			

	    Position pos;
		if (set_from_packed_sfen(pos,ps.sfen) != 0)
		{
			std::cout << "Error! : illegal packed sfen = " << GetFEN(pos) << std::endl;
			goto RetryRead;
		}
		NNUE::Accumulator acc;
		pos.setAccumulator(acc);

		auto rootColor = pos.side_to_move();

		// There is a possibility that all the pieces are blocked and stuck.
        MoveList moves;
		MoveGen::generate(pos,moves);
		if ( moves.empty() ){
			std::cout << "skipping empty move" << std::endl;
			goto RetryRead;
		}	

		// Evaluation value of shallow search (qsearch)
		DepthType seldepth = 0;
		PVList pv;
		auto r = context.qsearchNoPruning(-MATE,MATE,pos,0,seldepth,&pv);

        if ( isMateScore(r)){
			//std::cout << "skipping near mate " << r << " " << ToString(pos) << std::endl;
			goto RetryRead;
		}	

		// Evaluation value of deep search
		auto deep_value = (NNUEValue)ps.score;

		// I feel that the mini batch has a better gradient.
		// Go to the leaf node as it is, add only to the gradient array, and later try AdaGrad at the time of rmse aggregation.

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
            if ( !applyMove(pos,m,false)){
				illegal_move = true;
				break;
			}
		}

		if (illegal_move) {
			std::cout << "An illecal move was detected... Excluded the position from the learning data..." << std::endl;
			continue;
		}

		// Since we have reached the end phase of PV, add the slope here.
		pos_add_grad();

        // restore pos
		pos = psave;
		pos.resetAccumulator();
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
	else if (is_final)
	{
		NNUE::save_eval("final");
		return true;
	}
	else
	{
		static int dir_number = 0;
		const std::string dir_name = std::to_string(dir_number++);
		NNUE::save_eval(dir_name);

		if (newbob_decay != 1.0 && latest_loss_count > 0) {
			static int trials = newbob_num_trials;
			const double latest_loss = latest_loss_sum / latest_loss_count;
			latest_loss_sum = 0.0;
			latest_loss_count = 0;
			std::cout << "loss: " << latest_loss;
			if (latest_loss < best_loss) ///@todo try "true" !
			{
				std::cout << " < best (" << best_loss << "), accepted" << std::endl;
				best_loss = latest_loss;
				best_nn_directory = Path::Combine("evalsave", dir_name);
				trials = newbob_num_trials;
			}
			else
			{
				std::cout << " >= best (" << best_loss << "), rejected" << std::endl;
				best_nn_directory = Path::Combine("evalsave", dir_name);

				if (--trials > 0 && !is_final)
				{
					std::cout
						<< "reducing learning rate from " << NNUE::global_learning_rate
						<< " to " << (NNUE::global_learning_rate * newbob_decay)
						<< " (" << trials << " more trials)" << std::endl;

					NNUE::global_learning_rate *= newbob_decay;
				}
			}

			if (trials == 0)
			{
				std::cout << "converged" << std::endl;
				return true;
			}
		}
	}
	return false;
}

// Learning from the generated game record
void learn(std::istringstream& is)
{
	const auto thread_num = DynamicConfig::threads;

    startTime = Clock::now();

	std::vector<std::string> filenames;

	// mini_batch_size 1M aspect by default. This can be increased.
	auto mini_batch_size =  1000 * 1000 * 1;

	// Number of loops (read the game record file this number of times)
	int loop = 1;

	// Game file storage folder (get game file with relative path from here)
	std::string base_dir;

	std::string target_dir;

    // If the absolute value of the evaluation value
	// in the deep search of the teacher phase exceeds this value,
	// that phase is discarded.
	int eval_limit = 32000;

	// Flag to save the evaluation function file only once near the end.
	bool save_only_once = false;

	// Shuffle about what you are pre-reading on the teacher aspect.
	// (Shuffle of about 10 million phases)
	// Turn on if you want to pass a pre-shuffled file.
	bool no_shuffle = false;

	NNUE::global_learning_rate = 1.0;

	// elmo lambda
	ELMO_LAMBDA = 0.33;
	ELMO_LAMBDA2 = 0.33;
	ELMO_LAMBDA_LIMIT = 32000;

	// if (gamePly <rand(reduction_gameply)) continue;
	// An option to exclude the early stage from the learning target moderately like
	// If set to 1, rand(1)==0, so nothing is excluded.
	int reduction_gameply = 1;

	uint64_t nn_batch_size = 1000;
	double newbob_decay = 1.0;
	int newbob_num_trials = 2;
	std::string nn_options;

	uint64_t eval_save_interval = 1000000000ULL;
	uint64_t loss_output_interval = 0;

	std::string validation_set_file_name;
	std::string seed = "minic42";

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
		else if (option == "lr")        is >> NNUE::global_learning_rate;

		// Accept also the old option name.
		else if (option == "use_draw_in_training"
				|| option == "use_draw_games_in_training")
			is >> LearnerConfig::use_draw_games_in_training;

		// Accept also the old option name.
		else if (option == "use_draw_in_validation"
				|| option == "use_draw_games_in_validation")
			is >> LearnerConfig::use_draw_games_in_validation;

		else if (option == "winning_probability_coefficient") is >> LearnerConfig::winning_probability_coefficient;

		// Using WDL with win rate model instead of sigmoid
		else if (option == "use_wdl") is >> LearnerConfig::use_wdl;


		// LAMBDA
		else if (option == "lambda")       is >> ELMO_LAMBDA;
		else if (option == "lambda2")      is >> ELMO_LAMBDA2;
		else if (option == "lambda_limit") is >> ELMO_LAMBDA_LIMIT;

		else if (option == "reduction_gameply") is >> reduction_gameply;

		else if (option == "save_only_once") save_only_once = true;
		else if (option == "no_shuffle") no_shuffle = true;

		else if (option == "nn_batch_size") is >> nn_batch_size;
		else if (option == "newbob_decay") is >> newbob_decay;
		else if (option == "newbob_num_trials") is >> newbob_num_trials;
		else if (option == "nn_options") is >> nn_options;

		else if (option == "eval_save_interval") is >> eval_save_interval;
		else if (option == "loss_output_interval") is >> loss_output_interval;
		else if (option == "validation_set_file_name") is >> validation_set_file_name;

		else if (option == "src_score_min_value") is >> LearnerConfig::src_score_min_value;
		else if (option == "src_score_max_value") is >> LearnerConfig::src_score_max_value;
		else if (option == "dest_score_min_value") is >> LearnerConfig::dest_score_min_value;
		else if (option == "dest_score_max_value") is >> LearnerConfig::dest_score_max_value;
		else if (option == "seed") is >> seed;
		// Otherwise, it's a filename.
		else
			filenames.push_back(option);
	}

	if (loss_output_interval == 0)
	{
		loss_output_interval = mini_batch_size;
	}

	std::cout << "learn command , ";

	// Issue a warning if OpenMP is disabled.
#if !defined(_OPENMP)
	std::cout << "Warning! OpenMP disabled." << std::endl;
#endif

	SfenReader sr(thread_num, seed);
	LearnerThink learn_think(sr, seed);

	// Display learning game file
	if (target_dir != "")
	{
		std::string kif_base_dir = Path::Combine(base_dir, target_dir);

		namespace sys = std::filesystem;
		sys::path p(kif_base_dir); // Origin of enumeration
		std::for_each(sys::directory_iterator(p), sys::directory_iterator(),
			[&](const sys::path& path) {
				if (sys::is_regular_file(path))
					filenames.push_back(Path::Combine(target_dir, path.filename().generic_string()));
			});
	}

	std::cout << "learn from ";
	for (auto s : filenames)
		std::cout << s << " , ";

	std::cout << std::endl;
	if (!validation_set_file_name.empty())
	{
		std::cout << "validation set  : " << validation_set_file_name << std::endl;
	}

	std::cout << "base dir        : " << base_dir << std::endl;
	std::cout << "target dir      : " << target_dir << std::endl;

	std::cout << "loop              : " << loop << std::endl;
	std::cout << "eval_limit        : " << eval_limit << std::endl;
	std::cout << "save_only_once    : " << (save_only_once ? "true" : "false") << std::endl;
	std::cout << "no_shuffle        : " << (no_shuffle ? "true" : "false") << std::endl;

	// Insert the file name for the number of loops.
	for (int i = 0; i < loop; ++i)
	{
		// sfen reader, I'll read it in reverse
		// order so I'll reverse it here. I'm sorry.
		for (auto it = filenames.rbegin(); it != filenames.rend(); ++it)
		{
			sr.filenames.push_back(Path::Combine(base_dir, *it));
		}
	}

	std::cout << "Loss Function     : " << "ELMO_METHOD(WCSC27)" << std::endl;
	std::cout << "mini-batch size   : " << mini_batch_size << std::endl;

	std::cout << "nn_batch_size     : " << nn_batch_size << std::endl;
	std::cout << "nn_options        : " << nn_options << std::endl;

	std::cout << "learning rate     : " << NNUE::global_learning_rate << std::endl;
	std::cout << "use_draw_games_in_training : " << LearnerConfig::use_draw_games_in_training << std::endl;
	std::cout << "use_draw_games_in_validation : " << LearnerConfig::use_draw_games_in_validation << std::endl;

	if (newbob_decay != 1.0) {
		std::cout << "scheduling        : newbob with decay = " << newbob_decay
			<< ", " << newbob_num_trials << " trials" << std::endl;
	}
	else {
		std::cout << "scheduling        : default" << std::endl;
	}

	// If reduction_gameply is set to 0, rand(0) will be divided by 0, so correct it to 1.
	reduction_gameply = std::max(reduction_gameply, 1);
	std::cout << "reduction_gameply : " << reduction_gameply << std::endl;

	std::cout << "LAMBDA            : " << ELMO_LAMBDA << std::endl;
	std::cout << "LAMBDA2           : " << ELMO_LAMBDA2 << std::endl;
	std::cout << "LAMBDA_LIMIT      : " << ELMO_LAMBDA_LIMIT << std::endl;
	std::cout << "eval_save_interval  : " << eval_save_interval << " sfens" << std::endl;
	std::cout << "loss_output_interval: " << loss_output_interval << " sfens" << std::endl;

	// -----------------------------------
	// various initialization
	// -----------------------------------

	std::cout << "init..." << std::endl;

	// Read evaluation function parameters
	NNUEWrapper::init_NNUE();

	std::cout << "init_training.." << std::endl;
	NNUE::InitializeTraining(seed);
	NNUE::SetBatchSize(nn_batch_size);
	NNUE::SetOptions(nn_options);
	if (newbob_decay != 1.0 && !DynamicConfig::skipLoadingEval) {
		// Save the current net to [EvalSaveDir]\original.
		NNUE::save_eval("original");

		// Set the folder above to best_nn_directory so that the trainer can
		// resotre the network parameters from the original net file.
		learn_think.best_nn_directory =
			Path::Combine("evalsave", "original");
	}

	std::cout << "init done." << std::endl;

	// Reflect other option settings.
	learn_think.eval_limit = eval_limit;
	learn_think.save_only_once = save_only_once;
	learn_think.sr.no_shuffle = no_shuffle;
	learn_think.reduction_gameply = reduction_gameply;

	learn_think.newbob_decay = newbob_decay;
	learn_think.newbob_num_trials = newbob_num_trials;

	learn_think.eval_save_interval = eval_save_interval;
	learn_think.loss_output_interval = loss_output_interval;

	// Start a thread that loads the phase file in the background
	// (If this is not started, mse cannot be calculated.)
	learn_think.start_file_read_worker();

	learn_think.mini_batch_size = mini_batch_size;

	if (validation_set_file_name.empty())
	{
		// Get about 10,000 data for mse calculation.
		sr.read_for_mse();
	}
	else
	{
		sr.read_validation_set(validation_set_file_name, eval_limit);
	}

	// Calculate rmse once at this point (timing of 0 sfen)
	// sr.calc_rmse();

	if (newbob_decay != 1.0) {
		learn_think.calc_loss(0, -1);
		learn_think.best_loss = learn_think.latest_loss_sum / learn_think.latest_loss_count;
		learn_think.latest_loss_sum = 0.0;
		learn_think.latest_loss_count = 0;
		std::cout << now_string() << "initial loss: " << learn_think.best_loss << std::endl;
	}

	// -----------------------------------
	// start learning evaluation function parameters
	// -----------------------------------

	// Start learning.
	learn_think.go_think();

	NNUE::FinalizeNet();

	// Save once at the end.
	learn_think.save(true);
}


#endif