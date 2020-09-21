// Interface used for learning NNUE evaluation function

#ifndef _EVALUATE_NNUE_LEARNER_H_
#define _EVALUATE_NNUE_LEARNER_H_

#include "definition.hpp"
#include "learn/learn_tools.hpp"

#include <cstdint>
#include <string>

struct Position;

namespace NNUE {

// Initialize learning
void InitializeTraining(double eta1, uint64_t eta1_epoch,
                        double eta2, uint64_t eta2_epoch, double eta3);

// set the number of samples in the mini-batch
void SetBatchSize(uint64_t size);

// set the learning rate scale
void SetGlobalLearningRateScale(double scale);

// Set options such as hyperparameters
void SetOptions(const std::string& options);

// Reread the evaluation function parameters for learning from the file
void RestoreParameters(const std::string& dir_name);

// Add 1 sample of learning data
void AddExample(Position& pos, Color rootColor,
                const PackedSfenValue& psv, double weight);

// update the evaluation function parameters
void UpdateParameters(uint64_t epoch);

// Check if there are any problems with learning
void CheckHealth();

// save net on disk
void save_eval(std::string dir_name);

// Proceed with the difference calculation if possible
void update_eval(const Position& pos);

// get the current eta
double get_eta();

}  // namespace NNUE

#endif
