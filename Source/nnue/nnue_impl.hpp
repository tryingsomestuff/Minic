#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

// Taken from Seer version 1 implementation.
// But now uses different architecture. Especially quantization.
// see https://github.com/connormcmonigle/seer-nnue

#define NNUEALIGNMENT alignas(64)

namespace nnue{

// quantization constantes

template<bool Q>
struct Quantization{
   static constexpr float weightMax  = 100.0f;
   static constexpr int weightScale  = 1;
   static constexpr int weightFactor = 1;
   static constexpr int biasFactor   = 1;
   static constexpr float outFactor  = 1.f/600.f;
   static float round(const float &x){return x;}
   typedef float WT;
   typedef float WIT;
   typedef float BT;
   typedef float BIT;
};

template <>
struct Quantization<true>{
   static constexpr float weightMax    = 4.0f; // supposed min/max of weights values
   static constexpr int weightScale    = 4096; // int16 scaling
   static constexpr int weightFactor   = (int)(weightScale/weightMax); // quantization factor
   static constexpr int biasFactor     = weightScale*weightFactor;
   static constexpr float outFactor    = (weightFactor*weightFactor*weightMax)/600.f;
   static float round(const float & x){return std::round(x);}
   typedef int16_t WT;
   typedef int16_t WIT;
   typedef int32_t BT;
   typedef int32_t BIT;
};

inline void quantizationInfo(){
   Logging::LogIt(Logging::logInfo) << "Quantization info :";
   Logging::LogIt(Logging::logInfo) << "weightMax    " <<  Quantization<true>::weightMax;
   Logging::LogIt(Logging::logInfo) << "weightScale  " <<  Quantization<true>::weightScale;
   Logging::LogIt(Logging::logInfo) << "weightFactor " <<  Quantization<true>::weightFactor;
   Logging::LogIt(Logging::logInfo) << "biasFactor   " <<  Quantization<true>::biasFactor;
   Logging::LogIt(Logging::logInfo) << "outFactor    " <<  Quantization<true>::outFactor;
}

template<typename NT>
struct weights_streamer{
  std::fstream file;
  
  template<typename T, bool Q>
  weights_streamer<NT>& streamW(T* dst, const size_t request){
    float minW = std::numeric_limits<float>::max();
    float maxW = std::numeric_limits<float>::min();
    std::array<char, sizeof(NT)> single_element{};
    const float Wscale = Quantization<Q>::weightFactor;
    Logging::LogIt(Logging::logInfo) << "Reading inner weight";
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      dst[i] = Q? T( Quantization<Q>::round(Wscale * std::clamp(tmp,NT(-Quantization<Q>::weightMax),NT(Quantization<Q>::weightMax)) )) : tmp;
      if ( Q && std::abs(tmp) > (NT)Quantization<Q>::weightMax) Logging::LogIt(Logging::logWarn) << "Clamped weight " << tmp << " " << int(dst[i]);
      minW = std::min(minW,tmp);
      maxW = std::max(maxW,tmp);
    }
    Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
    return *this;
  }

  template<typename T, bool Q>
  weights_streamer<NT> & streamWI(T* dst, const size_t request){
    float minW = std::numeric_limits<float>::max();
    float maxW = std::numeric_limits<float>::min();
    std::array<char, sizeof(NT)> single_element{};
    const float Wscale = Quantization<Q>::weightScale;
    Logging::LogIt(Logging::logInfo) << "Reading input weight";
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      if ( Q && std::abs(tmp*Wscale) > (NT)std::numeric_limits<T>::max()) Logging::LogIt(Logging::logWarn) << "Overflow weight " << tmp << " " << (long long int)(Wscale * tmp);      
      dst[i] = Q ? T( Quantization<Q>::round(Wscale * tmp)) : tmp;
      minW = std::min(minW,tmp);
      maxW = std::max(maxW,tmp);
    }
    Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
    return *this;
  }

  template<typename T, bool Q>
  weights_streamer<NT> & streamB(T* dst, const size_t request){
    float minB = std::numeric_limits<float>::max();
    float maxB = std::numeric_limits<float>::min();
    std::array<char, sizeof(NT)> single_element{};
    const float Bscale = Quantization<Q>::biasFactor;
    Logging::LogIt(Logging::logInfo) << "Reading inner bias";
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      if ( Q && std::abs(tmp*Bscale) > (NT)std::numeric_limits<T>::max()) Logging::LogIt(Logging::logWarn) << "Overflow bias " << tmp << " " << (long long int)(Bscale * tmp);
      dst[i] = Q ? T(Quantization<Q>::round(Bscale * tmp)) : tmp;
      minB = std::min(minB,tmp);
      maxB = std::max(maxB,tmp);
    }
    Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
    return *this;
  }

  template<typename T, bool Q>
  weights_streamer<NT> & streamBI(T* dst, const size_t request){
    float minB = std::numeric_limits<float>::max();
    float maxB = std::numeric_limits<float>::min();
    std::array<char, sizeof(NT)> single_element{};
    const float Bscale = Quantization<Q>::weightScale;
    Logging::LogIt(Logging::logInfo) << "Reading input bias";
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      if ( Q && std::abs(tmp*Bscale) > (NT)std::numeric_limits<T>::max()) Logging::LogIt(Logging::logWarn) << "Overflow bias " << tmp << " " << (long long int)(Bscale * tmp);
      dst[i] = Q ? T(Quantization<Q>::round(Bscale * tmp)) : tmp;
      minB = std::min(minB,tmp);
      maxB = std::max(maxB,tmp);
    }
    Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
    return *this;
  }  

  weights_streamer(const std::string& name) : file(name, std::ios_base::in | std::ios_base::binary) {}
};

template<typename T, bool Q>
inline constexpr T clippedreluInput(const T& x){ return std::min(std::max(T(x) , T{0}), T{Quantization<Q>::weightScale}); }

template<typename T, bool Q>
inline constexpr T clippedrelu(const T& x){ return std::min(std::max(T(x/Quantization<Q>::weightFactor) , T{0}), T{Quantization<Q>::weightScale}); }

template<typename T, bool Q>
inline constexpr T clippedreluQSingleLayer(const T& x){ return std::min(std::max(T(x) , T{0}), T{Quantization<Q>::weightFactor}); }

template<typename T, size_t dim>
struct stack_vector{

#ifdef DEBUG_NNUE_UPDATE  
  std::array<T,dim> data;
  
  bool operator==(const stack_vector<T,dim> & other){
    static const T eps = std::numeric_limits<T>::epsilon() * 100;
    for (size_t i = 0 ; i < dim ; ++i){
       if ( std::fabs(data[i] - other.data[i]) > eps ){
         std::cout << data[i] << "!=" << other.data[i] << std::endl;
         return false;
       }
    }
    return true;
  }

  bool operator!=(const stack_vector<T,dim> & other){
    static const T eps = std::numeric_limits<T>::epsilon() * 100;
    for (size_t i = 0 ; i < dim ; ++i){
       if ( std::fabs(data[i] - other.data[i]) > eps ){
         std::cout << data[i] << "!=" << other.data[i] << std::endl;
         return true;
       }
    }
    return false;
  }
#else
  NNUEALIGNMENT T data[dim];
#endif

/*
  template<typename F>
  constexpr stack_vector<T, dim> apply(F&& f) const {
    return stack_vector<T, dim>{*this}.apply_(std::forward<F>(f));
  }
*/
  
  template<typename F>
  constexpr stack_vector<T, dim>& apply_(F&& f){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] = f(data[i]);
    }
    return *this;
  }

  template <typename T2>
  constexpr stack_vector<T, dim>& add_(const T2* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] += other[i];
    }
    return *this;
  }
  
  template <typename T2>
  constexpr stack_vector<T, dim>& sub_(const T2* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] -= other[i];
    }
    return *this;
  }
  
  template <typename T2, typename T3>
  constexpr stack_vector<T, dim>& fma_(const T2 c, const T3* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] += c * other[i];
    }
    return *this;
  }
  
  constexpr T item() const {
    static_assert(dim == 1, "called item() on vector with dim != 1");
    return data[0];
  }
  
  static constexpr stack_vector<T, dim> zeros(){
    stack_vector<T, dim> result{};
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      result.data[i] = T(0);
    }
    return result;
  }
  
  template <typename T2, typename T3 = int>
  static constexpr stack_vector<T, dim> from(const T2* data, const T3 & factor = T3{1}){
    stack_vector<T, dim> result{};
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      result.data[i] = T(data[i]*factor);
    }
    return result;
  }  

};

template<typename T, size_t dim>
std::ostream& operator<<(std::ostream& ostr, const stack_vector<T, dim>& vec){
  static_assert(dim != 0, "can't stream empty vector.");
  ostr << "stack_vector<T, " << dim << ">([";
  for(size_t i = 0; i < (dim-1); ++i){
    ostr << vec.data[i] << ", ";
  }
  ostr << vec.data[dim-1] << "])";
  return ostr;
}

template<typename T, size_t dim0, size_t dim1>
constexpr stack_vector<T, dim0 + dim1> splice(const stack_vector<T, dim0>& a, const stack_vector<T, dim1>& b){
  auto c = stack_vector<T, dim0 + dim1>::zeros();
  #pragma omp simd
  for(size_t i = 0; i < dim0; ++i){
    c.data[i] = a.data[i];
  }
  #pragma omp simd  
  for(size_t i = 0; i < dim1; ++i){
    c.data[dim0 + i] = b.data[i];
  }
  return c;
}

template<typename NT, size_t dim0, size_t dim1, bool Q>
struct stack_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;
  
  typedef typename Quantization<Q>::BT BT;
  typedef typename Quantization<Q>::BIT BIT;
  typedef typename Quantization<Q>::WT WT;

  // dirty thing here, stack_affine is always for inner layer
  NNUEALIGNMENT WT W[W_numel];
  NNUEALIGNMENT BT b[b_numel];
  
  constexpr stack_vector<BT, dim1> forward(const stack_vector<BT, dim0>& x) const {
    auto result = stack_vector<BT, dim1>::from(b);
    #pragma omp simd
    for(size_t i = 0; i < dim0; ++i){
      result.fma_(x.data[i], W + i * dim1);
    }
    return result;
  }
  
  ///@todo forward that return type of next layer
  template<typename T>
  constexpr stack_vector<BT, dim1> forward(const stack_vector<T, dim0>& x) const {
    auto result = stack_vector<BT, dim1>::from(b);
    #pragma omp simd
    for(size_t i = 0; i < dim0; ++i){
      result.fma_(x.data[i], W + i * dim1);
    }
    return result;
  }

  stack_affine<NT, dim0, dim1, Q>& load_(weights_streamer<NT>& ws){
    ws.template streamW<WT, Q>(W, W_numel).template streamB<BT, Q>(b, b_numel);
    return *this;
  }
};

template<typename NT, size_t dim0, size_t dim1, bool Q>
struct big_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;

  typedef typename Quantization<Q>::BIT BIT;
  typedef typename Quantization<Q>::WIT WIT;

  // dirty thing here, big_affine is always for input layer
  typename Quantization<Q>::WIT* W{nullptr};
  NNUEALIGNMENT typename Quantization<Q>::BIT b[b_numel];

  void insert_idx(const size_t idx, stack_vector<BIT, b_numel>& x) const {
    const WIT* mem_region = W + idx * dim1;
    x.add_(mem_region);
  }
  
  void erase_idx(const size_t idx, stack_vector<BIT, b_numel>& x) const {
    const WIT* mem_region = W + idx * dim1;
    x.sub_(mem_region);
  }

  big_affine<NT, dim0, dim1, Q>& load_(weights_streamer<NT>& ws){
    ws.template streamWI<WIT, Q>(W, W_numel).template streamBI<BIT, Q>(b, b_numel);
    return *this;
  }

  big_affine<NT, dim0, dim1, Q>& operator=(const big_affine<NT, dim0, dim1, Q>& other){
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
    return *this;
  }

  big_affine<NT, dim0, dim1, Q>& operator=(big_affine<NT, dim0, dim1, Q>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
    return *this;
  }

  big_affine(const big_affine<NT, dim0, dim1, Q>& other){
    W = new WIT[W_numel];
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
  }

  big_affine(big_affine<NT, dim0, dim1, Q>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
  }
  
  big_affine(){ W = new WIT[W_numel]; }

  ~big_affine(){ if(W != nullptr){ delete[] W; } }
};

constexpr size_t half_ka_numel = 12*64*64;
constexpr size_t base_dim = 128;

template<typename NT, bool Q>
struct half_kp_weights{
  big_affine  <NT, half_ka_numel, base_dim, Q> w{};
  big_affine  <NT, half_ka_numel, base_dim, Q> b{};
  stack_affine<NT, 2*base_dim   , 32      , Q> fc0{};
  stack_affine<NT, 32           , 32      , Q> fc1{};
  stack_affine<NT, 64           , 32      , Q> fc2{};
  stack_affine<NT, 96           , 1       , Q> fc3{};

  half_kp_weights<NT,Q>& load(weights_streamer<NT>& ws){
    quantizationInfo();
    ///@todo read a version number first !
    w.load_(ws);
    b.load_(ws);
    fc0.load_(ws);
    fc1.load_(ws);
    fc2.load_(ws);
    fc3.load_(ws);
    return *this;
  }
  
  bool load(const std::string& path, half_kp_weights<NT,Q>& loadedWeights){
#ifndef __ANDROID__
#ifndef WITHOUT_FILESYSTEM 
    static const int expectedSize = 50378500;
    std::error_code ec;
    auto fsize = std::filesystem::file_size(path,ec);
    if ( ec ){
      Logging::LogIt(Logging::logError) << "File " << path << " is not accessible";
      return false;
    }
    if ( fsize != expectedSize ){
      Logging::LogIt(Logging::logError) << "File " << path << " does not look like a compatible net";
      return false;
    }
#endif
#endif
    auto ws = weights_streamer<NT>(path);
    loadedWeights = load(ws);
    return true;
  }
};

template<typename NT, bool Q>
struct feature_transformer{
  const big_affine<NT, half_ka_numel, base_dim, Q>* weights_;

  typedef typename Quantization<Q>::BIT BIT;
  typedef typename Quantization<Q>::WIT WIT;

  // dirty thing here, active_ is always for input layer
  stack_vector<BIT, base_dim> active_;

  constexpr stack_vector<BIT, base_dim> active() const { return active_; }

  void clear(){ active_ = stack_vector<BIT, base_dim>::from(weights_ -> b); }

  void insert(const size_t idx){ weights_ -> insert_idx(idx, active_); }

  void erase(const size_t idx){ weights_ -> erase_idx(idx, active_); }

  feature_transformer(const big_affine<NT, half_ka_numel, base_dim, Q>* src) : weights_{src} {
    clear();
  }

#ifdef DEBUG_NNUE_UPDATE
  bool operator==(const feature_transformer<T,Q> & other){
    return active_ == other.active_;
  }

  bool operator!=(const feature_transformer<T,Q> & other){
    return active_ != other.active_;
  }
#endif

};

template<Color> struct them_{};

template<>
struct them_<Co_White>{
  static constexpr Color value = Co_Black;
};

template<>
struct them_<Co_Black>{
  static constexpr Color value = Co_White;
};

template<typename T, typename U>
struct sided{
  using return_type = U;
  
  T& cast(){ return static_cast<T&>(*this); }
  const T& cast() const { return static_cast<const T&>(*this); }
  
  template<Color c>
  return_type& us(){
    if constexpr(c == Co_White){ return cast().white; }
    else{ return cast().black; }
  }
  
  template<Color c>
  const return_type& us() const {
    if constexpr(c == Co_White){ return cast().white; }
    else{ return cast().black; }
  }
  
  template<Color c>
  return_type& them(){ return us<them_<c>::value>(); }

  template<Color c>
  const return_type& them() const { return us<them_<c>::value>(); }
  
 private:
  sided(){};
  friend T;
};

template<typename NT, bool Q>
struct half_kp_eval : sided<half_kp_eval<NT,Q>, feature_transformer<NT,Q>>{
  static half_kp_weights<NT,Q> weights;
  const half_kp_weights<NT,Q>* weights_;
  feature_transformer<NT,Q> white;
  feature_transformer<NT,Q> black;

  typedef typename Quantization<Q>::BT BT;

  constexpr float propagate(Color c) const {
    const auto w_x = white.active();
    const auto b_x = black.active();
    const auto x0 = c == Co_White ? splice(w_x, b_x).apply_(clippedreluInput<BT,Q>) : splice(b_x, w_x).apply_(clippedreluInput<BT,Q>);
    //std::cout << "x0 " << x0 << std::endl;
    //const stack_vector<BT, 32> x1 = stack_vector<BT, 32>::from((weights_ -> fc0).forward(x0).apply_(clippedreluQSingleLayer<QBT,true>).data,1.f/Quantization<true>::weightFactor);
    const auto x1 = (weights_ -> fc0).forward(x0).apply_(clippedrelu<BT,Q>);
    //std::cout << "x1 " << x1 << std::endl;
    const auto x2 = splice(x1, (weights_ -> fc1).forward(x1).apply_(clippedrelu<BT,Q>));
    //std::cout << "x2 " << x2 << std::endl;
    const auto x3 = splice(x2, (weights_ -> fc2).forward(x2).apply_(clippedrelu<BT,Q>));
    //std::cout << "x3 " << x3 << std::endl;
    const float val = (weights_ -> fc3).forward(x3).item();    
    //std::cout << "val " << val / Quantization<false>::outFactor << std::endl;
    return val / Quantization<Q>::outFactor;
  }

#ifdef DEBUG_NNUE_UPDATE
  bool operator==(const half_kp_eval<T> & other){
    return white == other.white && black == other.black;
  }

  bool operator!=(const half_kp_eval<T> & other){
    return white != other.white || black != other.black;
  }
#endif

  // default CTOR always use loaded weights
  half_kp_eval() : weights_{&weights}, white{&(weights . w)}, black{&(weights . b)} {}
};

template<typename NT, bool Q>
half_kp_weights<NT,Q> half_kp_eval<NT,Q>::weights;

} // nnue

#endif // WITH_NNUE
