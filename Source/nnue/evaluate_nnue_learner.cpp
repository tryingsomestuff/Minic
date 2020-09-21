// Code for learning NNUE evaluation function

#include <assert.h>
#include <random>
#include <fstream>

#include "definition.hpp"

#ifdef WITH_LEARNER

#include "learn/learner.hpp"
#include "learn/learn_tools.hpp"

#include "dynamicConfig.hpp"

#include "evaluate_nnue.h"
#include "evaluate_nnue_learner.h"
#include "trainer/features/factorizer_feature_set.h"
#include "trainer/features/factorizer_half_kp.h"
#include "trainer/trainer_feature_transformer.h"
#include "trainer/trainer_input_slice.h"
#include "trainer/trainer_affine_transform.h"
#include "trainer/trainer_clipped_relu.h"
#include "trainer/trainer_sum.h"

double calc_grad(NNUEValue teacher_signal, NNUEValue shallow , const PackedSfenValue& psv); // forward decl, implemeted in learner.cpp

double calc_grad(NNUEValue shallow, const PackedSfenValue& psv) {
    return calc_grad((NNUEValue)psv.score, shallow, psv);
}

namespace NNUE {

namespace {

// learning data
std::vector<Example> examples;

// Mutex for exclusive control of examples
std::mutex examples_mutex;

// number of samples in mini-batch
uint64_t batch_size;

// random number generator
std::mt19937 rng;

// learner
std::shared_ptr<Trainer<Network>> trainer;

// Learning rate scale
double global_learning_rate_scale;

// Get the learning rate scale
double GetGlobalLearningRateScale() {
  return global_learning_rate_scale;
}

// Tell the learner options such as hyperparameters
void SendMessages(std::vector<Message> messages) {
  for (auto& message : messages) {
    trainer->SendMessage(&message);
    assert(message.num_receivers > 0);
  }
}

}  // namespace

namespace EvalLearningTools
{
	// -------------------------------------------------
	//   Array for learning that stores gradients etc.
	// -------------------------------------------------

#pragma pack(2)
	struct Weight
	{
		// cumulative value of one mini-batch gradient
		LearnFloatType g = LearnFloatType(0);

		// When ADA_GRAD_UPDATE. LearnFloatType == float,
		// total 4*2 + 4*2 + 1*2 = 18 bytes
		// It suffices to secure a Weight array that is 4.5 times the size of the evaluation function parameter of 1GB.
		// However, sizeof(Weight)==20 code is generated if the structure alignment is in 4-byte units, so
		// Specify pragma pack(2).

		// For SGD_UPDATE, this structure is reduced by 10 bytes to 8 bytes.

		// Learning rate η(eta) such as AdaGrad.
		// It is assumed that eta1,2,3,eta1_epoch,eta2_epoch have been set by the time updateFV() is called.
		// The epoch of update_weights() gradually changes from eta1 to eta2 until eta1_epoch.
		// After eta2_epoch, gradually change from eta2 to eta3.
		static double eta;
		static double eta1;
		static double eta2;
		static double eta3;
		static uint64_t eta1_epoch;
		static uint64_t eta2_epoch;

		// Batch initialization of eta. If 0 is passed, the default value will be set.
		static void init_eta(double eta1, double eta2, double eta3, uint64_t eta1_epoch, uint64_t eta2_epoch)
		{
			Weight::eta1 = (eta1 != 0) ? eta1 : 30.0;
			Weight::eta2 = (eta2 != 0) ? eta2 : 30.0;
			Weight::eta3 = (eta3 != 0) ? eta3 : 30.0;
			Weight::eta1_epoch = (eta1_epoch != 0) ? eta1_epoch : 0;
			Weight::eta2_epoch = (eta2_epoch != 0) ? eta2_epoch : 0;
		}

		// Set eta according to epoch.
		static void calc_eta(uint64_t epoch)
		{
			if (Weight::eta1_epoch == 0) // Exclude eta2
				Weight::eta = Weight::eta1;
			else if (epoch < Weight::eta1_epoch)
				// apportion
				Weight::eta = Weight::eta1 + (Weight::eta2 - Weight::eta1) * epoch / Weight::eta1_epoch;
			else if (Weight::eta2_epoch == 0) // Exclude eta3
				Weight::eta = Weight::eta2;
			else if (epoch < Weight::eta2_epoch)
				Weight::eta = Weight::eta2 + (Weight::eta3 - Weight::eta2) * (epoch - Weight::eta1_epoch) / (Weight::eta2_epoch - Weight::eta1_epoch);
			else
				Weight::eta = Weight::eta3;
		}

		template <typename T> void updateFV(T& v) { updateFV(v, 1.0); }

		// Since the maximum value that can be accurately calculated with float is INT16_MAX*256-1
		// Keep the small value as a marker.
		const LearnFloatType V0_NOT_INIT = (INT16_MAX * 128);

		// What holds v internally. The previous implementation kept a fixed decimal with only a fractional part to save memory,
		// Since it is doubtful in accuracy and the visibility is bad, it was abolished.
		LearnFloatType v0 = LearnFloatType(V0_NOT_INIT);

		// AdaGrad g2
		LearnFloatType g2 = LearnFloatType(0);

		// update with AdaGrad
		// When executing this function, the value of g and the member do not change
		// Guaranteed by the caller. It does not have to be an atomic operation.
		// k is a coefficient for eta. 1.0 is usually sufficient. If you want to lower eta for your turn item, set this to 1/8.0 etc.
		template <typename T>
		void updateFV(T& v,double k)
		{
			// AdaGrad update formula
			// Gradient vector is g, vector to be updated is v, η(eta) is a constant,
			//     g2 = g2 + g^2
			//     v = v - ηg/sqrt(g2)

			constexpr double epsilon = 0.000001;

			if (g == LearnFloatType(0))
				return;

			g2 += g * g;

			// If v0 is V0_NOT_INIT, it means that the value is not initialized with the value of KK/KKP/KPP array,
			// In this case, read the value of v from the one passed in the argument.
			double V = (v0 == V0_NOT_INIT) ? v : v0;

			V -= k * eta * (double)g / sqrt((double)g2 + epsilon);

			// Limit the value of V to be within the range of types.
			// By the way, windows.h defines the min and max macros, so to avoid it,
			// Here, it is enclosed in parentheses so that it is not treated as a function-like macro.
			V = (std::min)((double)(std::numeric_limits<T>::max)() , V);
			V = (std::max)((double)(std::numeric_limits<T>::min)() , V);

			v0 = (LearnFloatType)V;
			v = (T)round(V);

			// Clear g because one update of mini-batch for this element is over
			// g[i] = 0;
			// → There is a problem of dimension reduction, so this will be done by the caller.
		}

		// grad setting
		template <typename T> void set_grad(const T& g_) { g = g_; }

		// Add grad
		template <typename T> void add_grad(const T& g_) { g += g_; }

		LearnFloatType get_grad() const { return g; }
	};
#pragma pack(0)

  double Weight::eta;
  double Weight::eta1;
  double Weight::eta2;
  double Weight::eta3;
  uint64_t Weight::eta1_epoch;
  uint64_t Weight::eta2_epoch;

	// Turned weight array
	// In order to be able to handle it transparently, let's have the same member as Weight.
	struct Weight2
	{
		Weight w[2];

		//Evaluate your turn, eta 1/8.
		template <typename T> void updateFV(std::array<T, 2>& v) { w[0].updateFV(v[0] , 1.0); w[1].updateFV(v[1],1.0/8.0); }

		template <typename T> void set_grad(const std::array<T, 2>& g) { for (int i = 0; i<2; ++i) w[i].set_grad(g[i]); }
		template <typename T> void add_grad(const std::array<T, 2>& g) { for (int i = 0; i<2; ++i) w[i].add_grad(g[i]); }

		std::array<LearnFloatType, 2> get_grad() const { return std::array<LearnFloatType, 2>{w[0].get_grad(), w[1].get_grad()}; }
	};

} // EvalLearningTools


// Initialize learning
void InitializeTraining(double eta1, uint64_t eta1_epoch,
                        double eta2, uint64_t eta2_epoch, double eta3) {
  std::cout << "Initializing NN training for "
            << GetArchitectureString() << std::endl;

  assert(__feature_transformer);
  assert(__network);
  trainer = Trainer<Network>::Create(__network.get(), __feature_transformer.get());

  if (DynamicConfig::skipLoadingEval) {
    trainer->Initialize(rng);
  }

  global_learning_rate_scale = 1.0;
  EvalLearningTools::Weight::init_eta(eta1, eta2, eta3, eta1_epoch, eta2_epoch);
}

// set the number of samples in the mini-batch
void SetBatchSize(uint64_t size) {
  assert(size > 0);
  batch_size = size;
}

// set the learning rate scale
void SetGlobalLearningRateScale(double scale) {
  global_learning_rate_scale = scale;
}

// Set options such as hyperparameters
void SetOptions(const std::string& options) {
  std::vector<Message> messages;
  for (const auto& option : Split(options, ',')) {
    const auto fields = Split(option, '=');
    assert(fields.size() == 1 || fields.size() == 2);
    if (fields.size() == 1) {
      messages.emplace_back(fields[0]);
    } else {
      messages.emplace_back(fields[0], fields[1]);
    }
  }
  SendMessages(std::move(messages));
}

// Reread the evaluation function parameters for learning from the file
void RestoreParameters(const std::string& dir_name) {
  const std::string file_name = Path::Combine(dir_name, NNUE::savedfileName);
  std::ifstream stream(file_name, std::ios::binary);
  ReadParameters(stream);
  SendMessages({{"reset"}});
}

// Add 1 sample of learning data
void AddExample(Position& pos, Color rootColor,
                const PackedSfenValue& psv, double weight) {
  Example example;
  if (rootColor == pos.side_to_move()) {
    example.sign = 1;
  } else {
    example.sign = -1;
  }
  example.psv = psv;
  example.weight = weight;

  Features::IndexList active_indices[2];
  for (const auto trigger : kRefreshTriggers) {
    RawFeatures::AppendActiveIndices(pos, trigger, active_indices);
  }
  if (pos.side_to_move() != WHITE) {
    active_indices[0].swap(active_indices[1]);
  }
  for (const auto color : {Co_White, Co_Black}) {
    std::vector<TrainingFeature> training_features;
    for (const auto base_index : active_indices[color]) {
      static_assert(Features::Factorizer<RawFeatures>::GetDimensions() <
                    (1 << TrainingFeature::kIndexBits), "");
      Features::Factorizer<RawFeatures>::AppendTrainingFeatures(
          base_index, &training_features);
    }
    std::sort(training_features.begin(), training_features.end());

    auto& unique_features = example.training_features[color];
    for (const auto& feature : training_features) {
      if (!unique_features.empty() &&
          feature.GetIndex() == unique_features.back().GetIndex()) {
        unique_features.back() += feature;
      } else {
        unique_features.push_back(feature);
      }
    }
  }

  std::lock_guard<std::mutex> lock(examples_mutex);
  examples.push_back(std::move(example));
}

// update the evaluation function parameters
void UpdateParameters(uint64_t epoch) {
  assert(batch_size > 0);

  EvalLearningTools::Weight::calc_eta(epoch);
  const auto learning_rate = static_cast<LearnFloatType>(
      get_eta() / batch_size);

  std::lock_guard<std::mutex> lock(examples_mutex);
  std::shuffle(examples.begin(), examples.end(), rng);
  while (examples.size() >= batch_size) {
    std::vector<Example> batch(examples.end() - batch_size, examples.end());
    examples.resize(examples.size() - batch_size);

    const auto network_output = trainer->Propagate(batch);

    std::vector<LearnFloatType> gradients(batch.size());
    for (std::size_t b = 0; b < batch.size(); ++b) {
      const auto shallow = static_cast<NNUEValue>(Round<std::int32_t>(
          batch[b].sign * network_output[b] * kPonanzaConstant));
      const auto& psv = batch[b].psv;
      const double gradient = batch[b].sign * calc_grad(shallow, psv);
      gradients[b] = static_cast<LearnFloatType>(gradient * batch[b].weight);
    }

    trainer->Backpropagate(gradients.data(), learning_rate);
  }
  SendMessages({{"quantize_parameters"}});
}

// Check if there are any problems with learning
void CheckHealth() {
  SendMessages({{"check_health"}});
}

// Proceed with the difference calculation if possible
void update_eval(const Position& pos) {
  UpdateAccumulatorIfPossible(pos);
}

// save merit function parameters to a file
void save_eval(std::string dir_name) {
  auto eval_dir = Path::Combine("evalsave", dir_name);
  std::cout << "save_eval() start. folder = " << eval_dir << std::endl;

  // mkdir() will fail if this folder already exists, but
  // Apart from that. If not, I just want you to make it.
  // Also, assume that the folders up to EvalSaveDir have been dug.
  Dependency::mkdir(eval_dir);

  if (DynamicConfig::skipLoadingEval && NNUE::trainer) {
    NNUE::SendMessages({{"clear_unobserved_feature_weights"}});
  }

  const std::string file_name = Path::Combine(eval_dir, NNUE::savedfileName);
  std::ofstream stream(file_name, std::ios::binary);
  NNUE::WriteParameters(stream);

  std::cout << "save_eval() finished. folder = " << eval_dir << std::endl;
}

// get the current eta
double get_eta() {
  return NNUE::GetGlobalLearningRateScale() * EvalLearningTools::Weight::eta;
}

}  // namespace NNUE

#endif