#include <iostream>
#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1 
#include "tiny_dnn/tiny_dnn.h"

const float kSig = 1.205;

inline float sigmoid(float x){
  return 1.f/(1+pow(10,-kSig*x/400));
}

inline float reverseSigmoid(float x){
  return - 400 * log(1.f/x-1.f)/(kSig*log(10));
}

int main() {

  // create a simple network 
  tiny_dnn::network<tiny_dnn::sequential> net;
  net.load("net.bin");

  std::vector<tiny_dnn::vec_t> X;
  std::vector<tiny_dnn::vec_t> Y;

  std::cout << "Reading input data ..." << std::endl;

  std::ifstream str("learn.data");
  if (str) {
      std::string line;
      while (std::getline(str, line)){
        std::stringstream stream(line);
        float result, color, f[14], scaling, gp, eval, score;
        stream >> result >> color;
        for (auto i=0 ; i < 14; ++i) stream >> f[i];
        stream >> scaling >> gp >> eval >> score;
        X.push_back({f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7],f[8],f[9],f[10],f[11],f[12],f[13],scaling,gp,eval});
        Y.push_back({score});
      }
  }
  else {
      return 1;
  }

  // use prediction inside eval as a correction term
  // compare prediction and desired output
  float fMaxError = 0.f;
  for (auto i = 0; i < X.size(); ++i) {
      float fPredicted   = net.predict(X[i])[0];
      float fDesired     = Y[i][0];
      std::cout << " actual score= " << fDesired 
                << " predicted   = " << fPredicted << std::endl;
      const float fError = fabs(fPredicted - fDesired);
      if (fMaxError < fError) fMaxError = fError;
  }

  std::cout << std::endl << "max_error=" << fMaxError << std::endl;
  std::cout << "bye" << std::endl;
  return 0;
}
