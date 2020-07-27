#pragma once

#include "definition.hpp"
#include "logging.hpp"
#include "score.hpp"

#ifdef WITH_MLP
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include "tiny_dnn/tiny_dnn.h"
#pragma GCC diagnostic pop

const float kSig = 1.205;

inline float sigmoid(float x){
  return 1.f/(1+pow(10,-kSig*x/400));
}

inline float reverseSigmoid(float x){
  return - 400 * log(1.f/x-1.f)/(kSig*log(10));
}

namespace NN{
    extern tiny_dnn::network<tiny_dnn::sequential> net;

    inline void loadNet(){
       Logging::LogIt(Logging::logInfo) << "Loading net ...";
       net.load("net.bin");
    }

    inline ScoreType EvalNN(const EvalFeatures & f, const float gp){
        const tiny_dnn::vec_t X = {(float)f.scores[F_material][MG],
                                   (float)f.scores[F_material][EG],
                                   (float)f.scores[F_positional][MG],
                                   (float)f.scores[F_positional][EG],
                                   (float)f.scores[F_development][MG],
                                   (float)f.scores[F_development][EG],
                                   (float)f.scores[F_mobility][MG],
                                   (float)f.scores[F_mobility][EG],
                                   (float)f.scores[F_pawnStruct][MG],
                                   (float)f.scores[F_pawnStruct][EG],
                                   (float)f.scores[F_attack][MG],
                                   (float)f.scores[F_attack][EG],
                                   (float)f.scores[F_complexity][MG],
                                   (float)f.scores[F_complexity][EG],
                                   f.scalingFactor,
                                   gp};
        return (ScoreType)reverseSigmoid(std::min(0.99f,std::max(0.01f,net.predict(X)[0])));
    }

}
#endif