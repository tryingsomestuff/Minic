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
// But now uses different architecture.
// see https://github.com/connormcmonigle/seer-nnue

#define NNUEALIGNMENT alignas(64)

namespace nnue{

template<typename T>
struct weights_streamer{
  std::fstream file;
  
  weights_streamer<T>& stream(T* dst, const size_t request){
    std::array<char, sizeof(T)> single_element{};
    for(size_t i(0); i < request; ++i){
      file.read(single_element.data(), single_element.size());
      std::memcpy(dst + i, single_element.data(), single_element.size());
    }
    return *this;
  }
  
  weights_streamer(const std::string& name) : file(name, std::ios_base::in | std::ios_base::binary) {}
};

template<typename T>
constexpr T relu(const T& x){ return std::max(x, T{0}); }

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

  constexpr stack_vector<T, dim>& add_(const T* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] += other[i];
    }
    return *this;
  }
  
  constexpr stack_vector<T, dim>& sub_(const T* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] -= other[i];
    }
    return *this;
  }
  
  constexpr stack_vector<T, dim>& fma_(const T c, const T* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] += c * other[i];
    }
    return *this;
  }
  
  constexpr stack_vector<T, dim>& set_(const T* other){
    #pragma omp simd
    for(size_t i = 0; i < dim; ++i){
      data[i] = other[i];
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
  
  static constexpr stack_vector<T, dim> from(const T* data){
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

template<typename T, size_t dim0, size_t dim1>
struct stack_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;
  
  NNUEALIGNMENT T W[W_numel];
  NNUEALIGNMENT T b[b_numel];
  
  constexpr size_t num_parameters() const {
    return W_numel + b_numel;
  }
  
  constexpr stack_vector<T, dim1> forward(const stack_vector<T, dim0>& x) const {
    auto result = stack_vector<T, dim1>::from(b);
    #pragma omp simd
    for(size_t i = 0; i < dim0; ++i){
      result.fma_(x.data[i], W + i * dim1);
    }
    return result;
  }
  
  stack_affine<T, dim0, dim1>& load_(weights_streamer<T>& ws){
    ws.stream(W, W_numel).stream(b, b_numel);
    return *this;
  }
};

template<typename T, size_t dim0, size_t dim1>
struct big_affine{
  static constexpr size_t W_numel = dim0*dim1;
  static constexpr size_t b_numel = dim1;

  T* W{nullptr};
  NNUEALIGNMENT T b[b_numel];

  constexpr size_t num_parameters() const {
    return W_numel + b_numel;
  }

  void insert_idx(const size_t idx, stack_vector<T, b_numel>& x) const {
    const T* mem_region = W + idx * dim1;
    x.add_(mem_region);
  }
  
  void erase_idx(const size_t idx, stack_vector<T, b_numel>& x) const {
    const T* mem_region = W + idx * dim1;
    x.sub_(mem_region);
  }

  big_affine<T, dim0, dim1>& load_(weights_streamer<T>& ws){
    ws.stream(W, W_numel).stream(b, b_numel);
    return *this;
  }

  big_affine<T, dim0, dim1>& operator=(const big_affine<T, dim0, dim1>& other){
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
    return *this;
  }

  big_affine<T, dim0, dim1>& operator=(big_affine<T, dim0, dim1>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
    return *this;
  }

  big_affine(const big_affine<T, dim0, dim1>& other){
    W = new T[W_numel];
    #pragma omp simd
    for(size_t i = 0; i < W_numel; ++i){ W[i] = other.W[i]; }
    #pragma omp simd
    for(size_t i = 0; i < b_numel; ++i){ b[i] = other.b[i]; }
  }

  big_affine(big_affine<T, dim0, dim1>&& other){
    std::swap(W, other.W);
    std::swap(b, other.b);
  }
  
  big_affine(){ W = new T[W_numel]; }
  ~big_affine(){ if(W != nullptr){ delete[] W; } }

};

constexpr size_t half_ka_numel = 12*64*64;
constexpr size_t base_dim = 128;

template<typename T>
struct half_kp_weights{
  big_affine<T, half_ka_numel, base_dim> w{};
  big_affine<T, half_ka_numel, base_dim> b{};
  stack_affine<T, 2*base_dim, 32> fc0{};
  stack_affine<T, 32, 32> fc1{};
  stack_affine<T, 64, 32> fc2{};
  stack_affine<T, 96, 1> fc3{};

  size_t num_parameters() const {
    return w.num_parameters() +
           b.num_parameters() +
           fc0.num_parameters() +
           fc1.num_parameters() +
           fc2.num_parameters() +
           fc3.num_parameters();
  }
  
  half_kp_weights<T>& load(weights_streamer<T>& ws){
    w.load_(ws);
    b.load_(ws);
    fc0.load_(ws);
    fc1.load_(ws);
    fc2.load_(ws);
    fc3.load_(ws);
    return *this;
  }
  
  bool load(const std::string& path, half_kp_weights<T>& loadedWeights){
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
    auto ws = weights_streamer<T>(path);
    loadedWeights = load(ws);
    return true;
  }
};

template<typename T>
struct feature_transformer{
  const big_affine<T, half_ka_numel, base_dim>* weights_;
  stack_vector<T, base_dim> active_;
  constexpr stack_vector<T, base_dim> active() const { return active_; }

  void clear(){ active_ = stack_vector<T, base_dim>::from(weights_ -> b); }
  void insert(const size_t idx){ 
    //std::cout << idx << std::endl; 
    weights_ -> insert_idx(idx, active_); 
  }
  void erase(const size_t idx){ weights_ -> erase_idx(idx, active_); }

  feature_transformer(const big_affine<T, half_ka_numel, base_dim>* src) : weights_{src} {
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

template<typename T>
struct half_kp_eval : sided<half_kp_eval<T>, feature_transformer<T>>{
  static half_kp_weights<T> weights;
  const half_kp_weights<T>* weights_;
  feature_transformer<T> white;
  feature_transformer<T> black;

  constexpr T propagate(Color c) const {
    const auto w_x = white.active();
    const auto b_x = black.active();
    const auto x0 = c == Co_White ? splice(w_x, b_x).apply(relu<T>) : splice(b_x, w_x).apply_(relu<T>);
    const auto x1 = (weights_ -> fc0).forward(x0).apply_(relu<T>);
    const auto x2 = splice(x1, (weights_ -> fc1).forward(x1).apply_(relu<T>));
    const auto x3 = splice(x2, (weights_ -> fc2).forward(x2).apply_(relu<T>));
    const T val = (weights_ -> fc3).forward(x3).item();
    return val;
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

template<typename T>
half_kp_weights<T> half_kp_eval<T>::weights;

} // nnue

#endif // WITH_NNUE
