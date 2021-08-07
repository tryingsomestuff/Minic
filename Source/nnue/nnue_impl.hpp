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

#ifdef USE_AVX_INTRIN
#include "dot.hpp"
#endif

#ifdef WITH_BLAS
#include <cblas.h>
#endif

// Initially taken from Seer implementation in October 2020.
// see https://github.com/connormcmonigle/seer-nnue

#define NNUEALIGNMENT 64

#ifdef __clang__
#define CONSTEXPR
#else
#define CONSTEXPR constexpr
#endif

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

namespace nnue {

#ifdef EMBEDDEDNNUEPATH
namespace embedded {
INCBIN_EXTERN(weightsFile);
}
#endif

// quantization constantes

template<bool Q> struct Quantization {
   typedef float WT;
   typedef float WIT;
   typedef float BT;
   typedef float BIT;
   static constexpr float weightMax {100}; // dummy value
   static constexpr int   weightScale {1};
   static constexpr int   weightFactor {1};
   static constexpr int   biasFactor {1};
   static constexpr float outFactor {1.f / 600.f};
   static float           round(const float& x) { return x; }
};

template<> struct Quantization<true> {
   typedef int16_t WIT;
   typedef int8_t  WT;
   typedef int32_t BIT;
   typedef int32_t BT;
   static constexpr float weightMax {2.0f}; // supposed min/max of weights values
   static constexpr int   weightScale {std::numeric_limits<WT>::max()};
   static constexpr int   weightFactor {(int)(weightScale / weightMax)}; // quantization factor
   static constexpr int   biasFactor {weightScale * weightFactor};
   static constexpr float outFactor {(weightFactor * weightFactor * weightMax) / 600.f};
   static float           round(const float& x) { return std::round(x); }
};

template<bool Q> inline void quantizationInfo() {
   if constexpr (Q) {
      Logging::LogIt(Logging::logInfo) << "Quantization info :";
      Logging::LogIt(Logging::logInfo) << "weightMax    " << Quantization<true>::weightMax;
      Logging::LogIt(Logging::logInfo) << "weightScale  " << Quantization<true>::weightScale;
      Logging::LogIt(Logging::logInfo) << "weightFactor " << Quantization<true>::weightFactor;
      Logging::LogIt(Logging::logInfo) << "biasFactor   " << Quantization<true>::biasFactor;
      Logging::LogIt(Logging::logInfo) << "outFactor    " << Quantization<true>::outFactor;
   }
   else {
      Logging::LogIt(Logging::logInfo) << "No quantization, using float net";
   }
}

template<typename NT> struct WeightsStreamer {
   std::istream* file = nullptr;

   WeightsStreamer<NT>& readVersion(uint32_t& version) {
      assert(file);
      file->read((char*)&version, sizeof(uint32_t));
      return *this;
   }

   template<typename T, bool Q> WeightsStreamer<NT>& streamW(T* dst, const size_t request, size_t dim0, size_t dim1) {
      assert(file);
      NT minW = std::numeric_limits<NT>::max();
      NT maxW = std::numeric_limits<NT>::min();
      std::array<char, sizeof(NT)> singleElement {};
      const NT Wscale = Quantization<Q>::weightFactor;
      Logging::LogIt(Logging::logInfo) << "Reading inner weight";
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         minW = std::min(minW, tmp);
         maxW = std::max(maxW, tmp);
         if (Q && std::abs(tmp * Wscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Wscale, (NT)std::numeric_limits<T>::min(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow weight " << tmp2 << " -> " << (int)tmp;
         }
         else {
            tmp = tmp * Wscale;
         }
#if (defined USE_AVX_INTRIN) || (defined WITH_BLAS)
         // transpose data
         const size_t j = (i % dim1) * dim0 + i / dim1;
#else
         const size_t j = i;
#endif
         dst[j] = T(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
      return *this;
   }

   template<typename T, bool Q> WeightsStreamer<NT>& streamWI(T* dst, const size_t request) {
      assert(file);
      NT minW = std::numeric_limits<NT>::max();
      NT maxW = std::numeric_limits<NT>::min();
      std::array<char, sizeof(NT)> singleElement {};
      const NT Wscale = Quantization<Q>::weightScale;
      Logging::LogIt(Logging::logInfo) << "Reading input weight";
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         minW = std::min(minW, tmp);
         maxW = std::max(maxW, tmp);
         if (Q && std::abs(tmp * Wscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Wscale, (NT)std::numeric_limits<T>::min(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow weight " << tmp2 << " -> " << (int)tmp;
         }
         else {
            tmp = tmp * Wscale;
         }
         dst[i] = T(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
      return *this;
   }

   template<typename T, bool Q> WeightsStreamer<NT>& streamB(T* dst, const size_t request) {
      assert(file);
      NT minB = std::numeric_limits<NT>::max();
      NT maxB = std::numeric_limits<NT>::min();
      std::array<char, sizeof(NT)> singleElement {};
      const NT Bscale = Quantization<Q>::biasFactor;
      Logging::LogIt(Logging::logInfo) << "Reading inner bias";
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         minB = std::min(minB, tmp);
         maxB = std::max(maxB, tmp);
         if (Q && std::abs(tmp * Bscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Bscale, (NT)std::numeric_limits<T>::min(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow bias " << tmp2 << " -> " << (int)tmp;
         }
         else {
            tmp = tmp * Bscale;
         }
         dst[i] = T(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
      return *this;
   }

   template<typename T, bool Q> WeightsStreamer<NT>& streamBI(T* dst, const size_t request) {
      assert(file);
      NT minB = std::numeric_limits<NT>::max();
      NT maxB = std::numeric_limits<NT>::min();
      std::array<char, sizeof(NT)> singleElement {};
      const NT Bscale = Quantization<Q>::weightScale;
      Logging::LogIt(Logging::logInfo) << "Reading input bias";
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         minB = std::min(minB, tmp);
         maxB = std::max(maxB, tmp);
         if (Q && std::abs(tmp * Bscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Bscale, (NT)std::numeric_limits<T>::min(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow bias " << tmp2 << " -> " << (int)tmp;
         }
         else {
            tmp = tmp * Bscale;
         }
         dst[i] = T(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
      return *this;
   }

   WeightsStreamer(std::istream& stream): file(&stream) {}
};

#ifdef WITH_NNUE_CLIPPED_RELU
template<typename T, bool Q> inline constexpr T activationInput(const T& x) {
   return std::min(std::max(T(x), T {0}), T {Quantization<Q>::weightScale});
}

template<typename T, bool Q> inline constexpr T activation(const T& x) {
   return std::min(std::max(T(x / Quantization<Q>::weightFactor), T {0}), T {Quantization<Q>::weightScale});
}

template<typename T, bool Q> inline constexpr T activationQSingleLayer(const T& x) {
   return std::min(std::max(T(x), T {0}), T {Quantization<Q>::weightFactor});
}

#else // standard relu (not clipped)

template<typename T, bool Q> inline constexpr typename std::enable_if<!Q, T>::type activation(const T& x) { return std::max(T(x), T {0}); }

#define activationInput        activation
#define activationQSingleLayer activation

#endif

template<typename T, size_t dim> struct StackVector {
   alignas(NNUEALIGNMENT) T data[dim];
#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const StackVector<T, dim>& other) {
      static const T eps = std::numeric_limits<T>::epsilon() * 100;
      for (size_t i = 0; i < dim; ++i) {
         if (std::fabs(data[i] - other.data[i]) > eps) {
            std::cout << data[i] << "!=" << other.data[i] << std::endl;
            return false;
         }
      }
      return true;
   }

   bool operator!=(const StackVector<T, dim>& other) {
      static const T eps = std::numeric_limits<T>::epsilon() * 100;
      for (size_t i = 0; i < dim; ++i) {
         if (std::fabs(data[i] - other.data[i]) > eps) {
            std::cout << data[i] << "!=" << other.data[i] << std::endl;
            return true;
         }
      }
      return false;
   }
#endif

   /*
  template<typename F>
  constexpr StackVector<T, dim> apply(F&& f) const {
    return StackVector<T, dim>{*this}.apply_(std::forward<F>(f));
  }
*/

   template<typename F> CONSTEXPR StackVector<T, dim>& apply_(F&& f) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] = f(data[i]); }
      return *this;
   }

   template<typename T2> CONSTEXPR StackVector<T, dim>& add_(const T2* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] += other[i]; }
      return *this;
   }

   template<typename T2> CONSTEXPR StackVector<T, dim>& sub_(const T2* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] -= other[i]; }
      return *this;
   }

   template<typename T2, typename T3> CONSTEXPR StackVector<T, dim>& fma_(const T2 c, const T3* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] += c * other[i]; }
      return *this;
   }

#ifdef USE_AVX_INTRIN
   inline T dot_(const T* other) const { return dotProductFma<T, dim>(data, other); }
#endif

   constexpr T item() const {
      static_assert(dim == 1, "called item() on vector with dim != 1");
      return data[0];
   }

   static CONSTEXPR StackVector<T, dim> zeros() {
      StackVector<T, dim> result {};
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { result.data[i] = T(0); }
      return result; // RVO
   }

   template<typename T2> static CONSTEXPR StackVector<T, dim> from(const T2* data) {
      StackVector<T, dim> result {};
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { result.data[i] = T(data[i]); }
      return result; //RVO
   }
};

template<typename T, size_t dim> std::ostream& operator<<(std::ostream& ostr, const StackVector<T, dim>& vec) {
   static_assert(dim != 0, "can't stream empty vector.");
   ostr << "StackVector<T, " << dim << ">([";
   for (size_t i = 0; i < (dim - 1); ++i) { ostr << vec.data[i] << ", "; }
   ostr << vec.data[dim - 1] << "])";
   return ostr;
}

template<typename T, size_t dim0, size_t dim1>
CONSTEXPR StackVector<T, dim0 + dim1> splice(const StackVector<T, dim0>& a, const StackVector<T, dim1>& b) {
   auto c = StackVector<T, dim0 + dim1>::zeros();
#pragma omp simd
   for (size_t i = 0; i < dim0; ++i) { c.data[i] = a.data[i]; }
#pragma omp simd
   for (size_t i = 0; i < dim1; ++i) { c.data[dim0 + i] = b.data[i]; }
   return c; // RVO
}

template<typename NT, size_t dim0, size_t dim1, bool Q> struct StackAffine {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   typedef typename Quantization<Q>::BT  BT;
   typedef typename Quantization<Q>::BIT BIT;
   typedef typename Quantization<Q>::WT  WT;

   // dirty thing here, StackAffine is always for inner layer
   alignas(NNUEALIGNMENT) WT W[nbW];
   alignas(NNUEALIGNMENT) BT b[nbB];

   CONSTEXPR StackVector<BT, dim1> forward(const StackVector<BT, dim0>& x) const {
      auto result = StackVector<BT, dim1>::from(b);
#ifndef WITH_BLAS
#pragma omp simd
#ifdef USE_AVX_INTRIN
      for (size_t i = 0; i < dim1; ++i) { result.data[i] += x.dot_(W + i * dim0); }
#else
      for (size_t i = 0; i < dim0; ++i) { result.fma_(x.data[i], W + i * dim1); }
#endif // USE_AVX_INTRIN
#else
      static_assert(std::is_same<BT, float>::value, "only works with float");
      cblas_sgemv(CblasRowMajor, CblasNoTrans, dim1, dim0, 1, W, dim0, x.data, 1, 1, result.data, 1);
#endif
      return result;
   }

   ///@todo forward with return type of next layer
   template<typename T> CONSTEXPR StackVector<BT, dim1> forward(const StackVector<T, dim0>& x) const {
      auto result = StackVector<BT, dim1>::from(b);
#ifndef WITH_BLAS
#pragma omp simd
#ifdef USE_AVX_INTRIN
      for (size_t i = 0; i < dim1; ++i) { result.data[i] += x.dot_(W + i * dim0); }
#else
      for (size_t i = 0; i < dim0; ++i) { result.fma_(x.data[i], W + i * dim1); }
#endif // USE_AVX_INTRIN
#else
      static_assert(std::is_same<BT, float>::value, "only works with float");
      cblas_sgemv(CblasRowMajor, CblasNoTrans, dim1, dim0, 1, W, dim0, x.data, 1, 1, result.data, 1);
#endif
      return result; // RVO
   }

   StackAffine<NT, dim0, dim1, Q>& load_(WeightsStreamer<NT>& ws) {
      ws.template streamW<WT, Q>(W, nbW, dim0, dim1).template streamB<BT, Q>(b, nbB);
      return *this;
   }
};

template<typename NT, size_t dim0, size_t dim1, bool Q> struct BigAffine {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   typedef typename Quantization<Q>::BIT BIT;
   typedef typename Quantization<Q>::WIT WIT;

   // dirty thing here, BigAffine is always for input layer
   typename Quantization<Q>::WIT* W {nullptr};
   alignas(NNUEALIGNMENT) typename Quantization<Q>::BIT b[nbB];

   void insertIdx(const size_t idx, StackVector<BIT, nbB>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.add_(wPtr);
   }

   void eraseIdx(const size_t idx, StackVector<BIT, nbB>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.sub_(wPtr);
   }

   BigAffine<NT, dim0, dim1, Q>& load_(WeightsStreamer<NT>& ws) {
      ws.template streamWI<WIT, Q>(W, nbW).template streamBI<BIT, Q>(b, nbB);
      return *this;
   }

   BigAffine<NT, dim0, dim1, Q>& operator=(const BigAffine<NT, dim0, dim1, Q>& other) {
#pragma omp simd
      for (size_t i = 0; i < nbW; ++i) { W[i] = other.W[i]; }
#pragma omp simd
      for (size_t i = 0; i < nbB; ++i) { b[i] = other.b[i]; }
      return *this;
   }

   BigAffine<NT, dim0, dim1, Q>& operator=(BigAffine<NT, dim0, dim1, Q>&& other) {
      std::swap(W, other.W);
      std::swap(b, other.b);
      return *this;
   }

   BigAffine(const BigAffine<NT, dim0, dim1, Q>& other) {
      W = new WIT[nbW];
#pragma omp simd
      for (size_t i = 0; i < nbW; ++i) { W[i] = other.W[i]; }
#pragma omp simd
      for (size_t i = 0; i < nbB; ++i) { b[i] = other.b[i]; }
   }

   BigAffine(BigAffine<NT, dim0, dim1, Q>&& other) {
      std::swap(W, other.W);
      std::swap(b, other.b);
   }

   BigAffine() { W = new WIT[nbW]; }

   ~BigAffine() {
      if (W != nullptr) { delete[] W; }
   }
};

constexpr size_t inputLayerSize      = 12 * 64 * 64;
constexpr size_t firstInnerLayerSize = 128;

template<typename NT, bool Q> struct NNUEWeights {
   BigAffine<NT, inputLayerSize, firstInnerLayerSize, Q> w {};
   BigAffine<NT, inputLayerSize, firstInnerLayerSize, Q> b {};
   StackAffine<NT, 2 * firstInnerLayerSize, 32, Q>       fc0 {};
   StackAffine<NT, 32, 32, Q>                            fc1 {};
   StackAffine<NT, 64, 32, Q>                            fc2 {};
   StackAffine<NT, 96,  1, Q>                            fc3 {};

   uint32_t version = 0;

   NNUEWeights<NT, Q>& load(WeightsStreamer<NT>& ws, bool readVersion) {
      quantizationInfo<Q>();
      if (readVersion) ws.readVersion(version);
      w.load_(ws);
      b.load_(ws);
      fc0.load_(ws);
      fc1.load_(ws);
      fc2.load_(ws);
      fc3.load_(ws);
      return *this;
   }

   static bool load(const std::string& path, NNUEWeights<NT, Q>& loadedWeights) {
      static const uint32_t expectedVersion = 0xc0ffee00;
      static const int      expectedSize    = 50378504; // 50378500 + 4 for version
      static const bool     withVersion     = true;

      if (path != "embedded") { // read from disk
#ifndef __ANDROID__
#ifndef WITHOUT_FILESYSTEM
         std::error_code ec;
         auto            fsize = std::filesystem::file_size(path, ec);
         if (ec) {
            Logging::LogIt(Logging::logError) << "File " << path << " is not accessible";
            return false;
         }
         if (fsize != expectedSize) { // with or without version
            Logging::LogIt(Logging::logError) << "File " << path << " does not look like a compatible net";
            return false;
         }
#endif
#endif
         std::fstream stream(path, std::ios_base::in | std::ios_base::binary);
         auto         ws = WeightsStreamer<NT>(stream);
         loadedWeights.load(ws, withVersion);
      }
#ifdef EMBEDDEDNNUEPATH
      else {                                             // read from embedded data
         if (embedded::weightsFileSize != expectedSize) { // with or without version
            Logging::LogIt(Logging::logError) << "File " << path << " does not look like a compatible net";
            return false;
         }
         std::istringstream stream(std::string((const char*)embedded::weightsFileData, embedded::weightsFileSize), std::stringstream::binary);
         auto               ws = WeightsStreamer<NT>(stream);
         loadedWeights.load(ws, withVersion);
      }
#else
      else {
         Logging::LogIt(Logging::logError) << "Minic was not compiled with an embedded net";
         return false;
      }
#endif

      if (withVersion) {
         Logging::LogIt(Logging::logInfo) << "Expected net version : " << toHexString(expectedVersion);
         Logging::LogIt(Logging::logInfo) << "Read net version     : " << toHexString(loadedWeights.version);
         if (loadedWeights.version != expectedVersion) {
            Logging::LogIt(Logging::logError) << "File " << path << " is not a compatible version of the net";
            return false;
         }
      }
      return true;
   }
};

template<typename NT, bool Q> struct FeatureTransformer {
   const BigAffine<NT, inputLayerSize, firstInnerLayerSize, Q>* weights_;

   typedef typename Quantization<Q>::BIT BIT;
   typedef typename Quantization<Q>::WIT WIT;

   // dirty thing here, active_ is always for input layer
   StackVector<BIT, firstInnerLayerSize> active_;

   constexpr StackVector<BIT, firstInnerLayerSize> active() const { return active_; }

   void clear() {
      assert(weights_);
      active_ = StackVector<BIT, firstInnerLayerSize>::from(weights_->b);
   }

   void insert(const size_t idx) {
      assert(weights_);
      weights_->insertIdx(idx, active_);
   }

   void erase(const size_t idx) {
      assert(weights_);
      weights_->eraseIdx(idx, active_);
   }

   FeatureTransformer(const BigAffine<NT, inputLayerSize, firstInnerLayerSize, Q>* src): weights_ {src} { clear(); }

   FeatureTransformer(): weights_(nullptr) {}

   ~FeatureTransformer() {}

#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const FeatureTransformer<NT, Q>& other) { return active_ == other.active_; }

   bool operator!=(const FeatureTransformer<NT, Q>& other) { return active_ != other.active_; }
#endif
};

template<Color> struct them_ {};

template<> struct them_<Co_White> { static constexpr Color value = Co_Black; };

template<> struct them_<Co_Black> { static constexpr Color value = Co_White; };

template<typename T, typename U> struct sided {
   using returnType = U;

   T&       cast() { return static_cast<T&>(*this); }
   const T& cast() const { return static_cast<const T&>(*this); }

   template<Color c> returnType& us() {
      if constexpr (c == Co_White) { return cast().white; }
      else {
         return cast().black;
      }
   }

   template<Color c> const returnType& us() const {
      if constexpr (c == Co_White) { return cast().white; }
      else {
         return cast().black;
      }
   }

   template<Color c> returnType& them() { return us<them_<c>::value>(); }

   template<Color c> const returnType& them() const { return us<them_<c>::value>(); }

  private:
   sided() {};
   friend T;
};

template<typename NT, bool Q> struct NNUEEval : sided<NNUEEval<NT, Q>, FeatureTransformer<NT, Q>> {
   // common data (weights and bias)
   static NNUEWeights<NT, Q> weights;
   // instance data (active index)
   FeatureTransformer<NT, Q> white;
   FeatureTransformer<NT, Q> black;

   void clear() {
      white.clear();
      black.clear();
   }

   typedef typename Quantization<Q>::BT BT;

   constexpr float propagate(Color c) const {
      const auto w_x = white.active();
      const auto b_x = black.active();
      const auto x0  = c == Co_White ? splice(w_x, b_x).apply_(activationInput<BT, Q>) : splice(b_x, w_x).apply_(activationInput<BT, Q>);
      //std::cout << "x0 " << x0 << std::endl;
      //const StackVector<BT, 32> x1 = StackVector<BT, 32>::from((weights.fc0).forward(x0).apply_(activationQSingleLayer<BT,true>).data,1.f/Quantization<Q>::weightFactor);
      const auto x1 = (weights.fc0).forward(x0).apply_(activation<BT, Q>);
      //std::cout << "x1 " << x1 << std::endl;
      const auto x2 = splice(x1, (weights.fc1).forward(x1).apply_(activation<BT, Q>));
      //std::cout << "x2 " << x2 << std::endl;
      const auto x3 = splice(x2, (weights.fc2).forward(x2).apply_(activation<BT, Q>));
      //std::cout << "x3 " << x3 << std::endl;
      const float val = (weights.fc3).forward(x3).item();
      //std::cout << "val " << val / Quantization<Q>::outFactor << std::endl;
      return val / Quantization<Q>::outFactor;
   }

#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const NNUEEval<NT,Q>& other) {
      if (white != other.white || black != other.black) return false;
      return true;
   }

   bool operator!=(const NNUEEval<NT,Q>& other) {
      if (white != other.white || black != other.black) return true;
      return false;
   }
#endif

   // default CTOR always use loaded weights
   NNUEEval() {
      white = &weights.w;
      black = &weights.b;
   }
};

template<typename NT, bool Q> NNUEWeights<NT, Q> NNUEEval<NT, Q>::weights;

} // namespace nnue

#endif // WITH_NNUE
