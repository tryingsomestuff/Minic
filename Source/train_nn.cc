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
  net << tiny_dnn::fully_connected_layer(16, 8);
  net << tiny_dnn::relu_layer();
  net << tiny_dnn::fully_connected_layer(8, 1);

  std::vector<tiny_dnn::vec_t> X;
  std::vector<tiny_dnn::vec_t> Y;

  std::cout << "Reading input data ..." << std::endl;

  std::ifstream str("learn.data");
  if (str) {
      std::string line;
      int k = 0;
      int r = 0;
      while (std::getline(str, line)){
        std::stringstream stream(line);
        float result, color, f[14], scaling, gp, eval, score;
        stream >> result >> color;
        for (auto i=0 ; i < 14; ++i) stream >> f[i];
        stream >> scaling >> gp >> eval >> score;
        if ( std::abs(score-eval) < 200){
           X.push_back({f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7],f[8],f[9],f[10],f[11],f[12],f[13],scaling,gp});
           Y.push_back({score});
        }
        else ++r;
        ++k;
      	if ( !(k%100000) ) std::cout << eval << " " << score << std::endl;
      }
      std::cout << "rejected data " << r << "/" << k << std::endl;
  }
  else {
      return 1;
  }

  

  std::vector<tiny_dnn::vec_t> XTrain(X); XTrain.resize(X.size()/2);
  std::vector<tiny_dnn::vec_t> YTrain(Y); YTrain.resize(Y.size()/2);

  // set learning parameters
  const size_t batch_size = X.size()/100;    // number of samples for each network weight update (iteration)
  const int epochs        = 500;  // presentation of all samples
  tiny_dnn::adamax opt;

  // this lambda function will be called after each epoch
  int iEpoch              = 0;
  auto on_enumerate_batch = [&]() {
    std::cout << "." << std::flush;
  };  

  auto on_enumerate_epoch = [&]() {
    iEpoch++;
    //if (iEpoch % 100) return;
    const float loss = net.get_loss<tiny_dnn::mse>(X, Y);
    std::cout << " epoch=" << iEpoch << "/" << epochs << " loss=" << loss << std::endl;
    // save network
    net.save("net.bin");
  };

  // learn
  std::cout << "learning ..." << std::endl;
  net.fit<tiny_dnn::mse>(opt, XTrain, YTrain, batch_size, epochs, on_enumerate_batch, on_enumerate_epoch);

  std::cout << "Training finished, saving the net ..." << std::endl;

  // use prediction inside eval as a correction term
  // compare prediction and desired output
  float fMaxError = 0.f;
  for (auto i = X.size()/2 ; i < X.size() ; ++i) {
      float fPredicted   = net.predict(X[i])[0];
      float fDesired     = Y[i][0];
      std::cout << " actual score= " << fDesired  
                << " predicted=    " << fPredicted << std::endl;
      const float fError = fabs(fPredicted - fDesired);
      if (fMaxError < fError) fMaxError = fError;
  }

  std::cout << std::endl << "max_error=" << fMaxError << std::endl;
  std::cout << "bye" << std::endl;
  return 0;
}
