#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include <algorithm>
#include <array>
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

#ifdef WITH_QUANTIZATION
// quantization constantes
///@todo doc
const float weightMax    = 4.0f; // supposed min/max of weights values
const int weightScale    = 1024;  // int8 scaling
const int weightFactor   = 256;//weightScale/weightMax; // quantization factor
const int biasFactor     = weightScale*weightFactor;
const float outFactor    = (weightFactor*weightFactor*weightMax)/600.f;
#define ROUNDFUNC(x) std::round(x)
#else
const float weightMax  = 100.0f;
const int weightScale  = 1;
const int weightFactor = 1;
const int biasFactor   = 1;
const float outFactor  = 1.f/600.f;
#define ROUNDFUNC(x) (x)
#endif

template<typename WIT, typename WT, typename BIT, typename BT, typename NT>
struct weights_streamer{
  std::fstream file;
  
  weights_streamer<WIT,WT,BIT,BT,NT>& streamW(WT* dst, const size_t request, bool isInput = false){
    std::array<char, sizeof(NT)> single_element{};
    const float Wscale = isInput ? weightScale : weightFactor;
    //std::cout << "****reading inner weight" << std::endl;
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      dst[i] = WT( ROUNDFUNC(Wscale * (isInput ? tmp : std::clamp(tmp,NT(-weightMax),NT(weightMax)) )) );
      //if ( std::abs(tmp)> weightMax) std::cout << "weight " << tmp << " " << int(dst[i]) << std::endl;
    }
    return *this;
  }

  template<typename T = WIT>
  typename std::enable_if<!std::is_same<WT, T>::value,weights_streamer<WIT,WT,BIT,BT,NT>>::type & streamW(WIT* dst, const size_t request, bool isInput = false){
    std::array<char, sizeof(NT)> single_element{};
    const float Wscale = isInput ? weightScale : weightFactor;
    //std::cout << "****reading input weight" << std::endl;
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      dst[i] = WIT( ROUNDFUNC(Wscale * (isInput ? tmp : std::clamp(tmp,NT(-weightMax),NT(weightMax)) )) );
      //std::cout << "weight " << tmp << " " << int(dst[i]) << std::endl;
    }
    return *this;
  }

  weights_streamer<WIT,WT,BIT,BT,NT> & streamB(BT* dst, const size_t request, bool isInput = false){
    std::array<char, sizeof(NT)> single_element{};
    const float Bscale = isInput ? weightScale : biasFactor;
    //std::cout << "****reading inner bias" << std::endl;
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      dst[i] = BT(ROUNDFUNC(Bscale * tmp));
      //std::cout << "bias " << tmp << " " << dst[i] << std::endl;
    }
    return *this;
  }

  template<typename T = BIT>
  typename std::enable_if<!std::is_same<BT, T>::value,weights_streamer<WIT,WT,BIT,BT,NT>>::type & streamB(BIT* dst, const size_t request, bool isInput = false){
    std::array<char, sizeof(NT)> single_element{};
    const float Bscale = isInput ? weightScale : biasFactor;
    //std::cout << "****reading input bias" << std::endl;
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      NT tmp{0};
      std::memcpy(&tmp, single_element.data(), single_element.size());
      dst[i] = BIT(ROUNDFUNC(Bscale * tmp));
      //std::cout << "bias " << tmp << " " << dst[i] << std::endl;
    }
    return *this;
  }  

  weights_streamer(const std::string& name) : file(name, std::ios_base::in | std::ios_base::binary) {}
};

template<typename T>
inline constexpr T clippedreluInput(const T& x){ return std::min(std::max(T(x) , T{0}), T{weightScale}); }

template<typename T>
inline constexpr T clippedrelu(const T& x){ return std::min(std::max(T(x/weightFactor) , T{0}), T{weightScale}); }

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

  template<typename F>
  constexpr stack_vector<T, dim> apply(F&& f) const {
    return stack_vector<T, dim>{*this}.apply_(std::forward<F>(f));
  }
  
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
  
  template <typename T2>
  constexpr stack_vector<T, dim>& fma_(const T c, const T2* other){
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
    ///@todo memcopy instead ?
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      result.data[i] = T(0);
    }
    return result;
  }
  
  template <typename T2>
  static constexpr stack_vector<T, dim> from(const T2* data){
    stack_vector<T, dim> result{};
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      result.data[i] = data[i];
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
  for(size_t i = 0; i < dim1; ++i){
    c.data[dim0 + i] = b.data[i];
  }
  return c;
}

template<typename WIT, typename WT, typename BIT, typename BT, typename NT, size_t dim0, size_t dim1>
struct stack_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;
  
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
  
  template<typename T = BIT>
  constexpr typename std::enable_if<!std::is_same<BT, T>::value,stack_vector<BIT, dim1>>::type
    forward(const stack_vector<BIT, dim0>& x) const {
    auto result = stack_vector<BIT, dim1>::from(b);
    #pragma omp simd
    for(size_t i = 0; i < dim0; ++i){
      result.fma_(x.data[i], W + i * dim1);
    }
    return result;
  }

  stack_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& load_(weights_streamer<WIT,WT,BIT,BT,NT>& ws){
    ws.streamW(W, W_numel).streamB(b, b_numel);
    return *this;
  }
};

template<typename WIT, typename WT, typename BIT, typename BT, typename NT, size_t dim0, size_t dim1>
struct big_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;

  // dirty thing here, big_affine is always for input layer
  WIT* W{nullptr};
  NNUEALIGNMENT BIT b[b_numel];

  void insert_idx(const size_t idx, stack_vector<BIT, b_numel>& x) const {
    const WIT* mem_region = W + idx * dim1;
    x.add_(mem_region);
  }
  
  void erase_idx(const size_t idx, stack_vector<BIT, b_numel>& x) const {
    const WIT* mem_region = W + idx * dim1;
    x.sub_(mem_region);
  }

  big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& load_(weights_streamer<WIT,WT,BIT,BT,NT>& ws){
    ws.streamW(W, W_numel,true).streamB(b, b_numel,true);
    return *this;
  }

  big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& operator=(const big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& other){
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
    return *this;
  }

  big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& operator=(big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
    return *this;
  }

  big_affine(const big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>& other){
    W = new WIT[W_numel];
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
  }

  big_affine(big_affine<WIT, WT, BIT, BT, NT, dim0, dim1>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
  }
  
  big_affine(){ W = new WIT[W_numel]; }

  ~big_affine(){ if(W != nullptr){ delete[] W; } }

};

constexpr size_t half_ka_numel = 12*64*64;
constexpr size_t base_dim = 128;

template<typename WIT, typename WT, typename BIT, typename BT, typename NT>
struct half_kp_weights{
  big_affine  <WIT, WT, BIT, BT, NT, half_ka_numel, base_dim> w{};
  big_affine  <WIT, WT, BIT, BT, NT, half_ka_numel, base_dim> b{};
  stack_affine<WIT, WT, BIT, BT, NT, 2*base_dim   , 32>       fc0{};
  stack_affine<WIT, WT, BIT, BT, NT, 32           , 32>       fc1{};
  stack_affine<WIT, WT, BIT, BT, NT, 64           , 32>       fc2{};
  stack_affine<WIT, WT, BIT, BT, NT, 96           , 1>        fc3{};

  half_kp_weights<WIT,WT,BIT,BT,NT>& load(weights_streamer<WIT,WT,BIT,BT,NT>& ws){
    ///@todo read a version number first !
    w.load_(ws);
    b.load_(ws);
    fc0.load_(ws);
    fc1.load_(ws);
    fc2.load_(ws);
    fc3.load_(ws);
    return *this;
  }
  
  bool load(const std::string& path, half_kp_weights<WIT,WT,BIT,BT,NT>& loadedWeights){
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
    auto ws = weights_streamer<WIT,WT,BIT,BT,NT>(path);
    loadedWeights = load(ws);
    return true;
  }
};

template<typename WIT, typename WT, typename BIT, typename BT, typename NT>
struct feature_transformer{
  const big_affine<WIT, WT, BIT, BT, NT, half_ka_numel, base_dim>* weights_;
  // dirty thing here, active_ is always for input layer
  stack_vector<BIT, base_dim> active_;

  constexpr stack_vector<BIT, base_dim> active() const { return active_; }

  void clear(){ active_ = stack_vector<BIT, base_dim>::from(weights_ -> b); }

  void insert(const size_t idx){ weights_ -> insert_idx(idx, active_); }

  void erase(const size_t idx){ weights_ -> erase_idx(idx, active_); }

  feature_transformer(const big_affine<WIT, WT, BIT, BT, NT, half_ka_numel, base_dim>* src) : weights_{src} {
    clear();
  }

#ifdef DEBUG_NNUE_UPDATE
  bool operator==(const feature_transformer<T> & other){
    return active_ == other.active_;
  }

  bool operator!=(const feature_transformer<T> & other){
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

template<typename WIT, typename WT, typename BIT, typename BT,typename NT>
struct half_kp_eval : sided<half_kp_eval<WIT,WT,BIT,BT,NT>, feature_transformer<WIT,WT,BIT,BT,NT>>{
  static half_kp_weights<WIT,WT,BIT,BT,NT> weights;
  const half_kp_weights<WIT,WT,BIT,BT,NT>* weights_;
  feature_transformer<WIT,WT,BIT,BT,NT> white;
  feature_transformer<WIT,WT,BIT,BT,NT> black;

  constexpr float propagate(Color c) const {
    const auto w_x = white.active();
    const auto b_x = black.active();
    const auto x0 = c == Co_White ? splice(w_x, b_x).apply(clippedreluInput<BT>) : splice(b_x, w_x).apply_(clippedreluInput<BT>);
    //std::cout << "x0 " << x0 << std::endl;
    const auto x1 = (weights_ -> fc0).forward(x0).apply_(clippedrelu<BT>);
    //std::cout << "x1 " << x1 << std::endl;
    const auto x2 = splice(x1, (weights_ -> fc1).forward(x1).apply_(clippedrelu<BT>));
    //std::cout << "x2 " << x2 << std::endl;
    const auto x3 = splice(x2, (weights_ -> fc2).forward(x2).apply_(clippedrelu<BT>));
    //std::cout << "x3 " << x3 << std::endl;
    const float val = (weights_ -> fc3).forward(x3).item();    
    //std::cout << "val " << val / outFactor << std::endl;
    return val / outFactor;
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

template<typename WIT, typename WT, typename BIT, typename BT, typename NT>
half_kp_weights<WIT,WT,BIT,BT,NT> half_kp_eval<WIT,WT,BIT,BT,NT>::weights;

} // nnue

#endif // WITH_NNUE
