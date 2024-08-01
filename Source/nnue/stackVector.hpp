#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

template<typename T, size_t dim, bool Q> 
struct StackVector {
   alignas(NNUEALIGNMENT) T data[dim];

   template<typename T2> 
   CONSTEXPR StackVector<T, dim, Q>& add_(const T2* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] += other[i]; }
      return *this;
   }

   template<typename T2> 
   CONSTEXPR StackVector<T, dim, Q>& sub_(const T2* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] -= other[i]; }
      return *this;
   }

#ifndef USE_SIMD_INTRIN
   template<typename T2, typename T3> 
   CONSTEXPR StackVector<T, dim, Q>& fma_(const T2 c, const T3* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] += c * other[i]; }
      return *this;
   }

   template<typename F, typename T2> 
   CONSTEXPR StackVector<T, dim, Q>& apply_(F&& f, const T2 * param) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] = f(data[i], param[i]); }
      return *this;
   }   
#else
   T dot_(const T* other) const { return simdDotProduct<dim,Q>(data, other); }
   CONSTEXPR StackVector<T, dim, Q>& activation_(const T* slopes) { simdActivation<dim,Q>(data, slopes); return *this;}
#endif

   template<typename T2> 
   FORCE_FINLINE void from(const T2* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] = static_cast<T>(other[i]); }
   }

  // note that quantization is done on read if needed (see weightReader)
  template <typename U> 
  [[nodiscard]] CONSTEXPR StackVector<U, dim, Q> dequantize(const U& scale) const {
    static_assert(std::is_integral_v<T> && std::is_floating_point_v<U>);
    StackVector<U, dim, Q> result;
#pragma omp simd
    for (size_t i = 0; i < dim; ++i) { result.data[i] = scale * static_cast<U>(data[i]); }
    return result; //RVO
  }

#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const StackVector<T, dim, Q>& other) {
      constexpr T eps = std::numeric_limits<T>::epsilon() * 100;
      for (size_t i = 0; i < dim; ++i) {
         if (std::fabs(data[i] - other.data[i]) > eps) {
            std::cout << data[i] << "!=" << other.data[i] << std::endl;
            return false;
         }
      }
      return true;
   }

   bool operator!=(const StackVector<T, dim, Q>& other) {
      constexpr T eps = std::numeric_limits<T>::epsilon() * 100;
      for (size_t i = 0; i < dim; ++i) {
         if (std::fabs(data[i] - other.data[i]) > eps) {
            std::cout << data[i] << "!=" << other.data[i] << std::endl;
            return true;
         }
      }
      return false;
   }
#endif   
};

template<typename T, size_t dim0, size_t dim1, bool Q>
CONSTEXPR StackVector<T, dim0 + dim1, Q> splice(const StackVector<T, dim0, Q>& a, const StackVector<T, dim1, Q>& b) {
   StackVector<T, dim0 + dim1, Q> c;
#pragma omp simd
   for (size_t i = 0; i < dim0; ++i) { c.data[i] = a.data[i]; }
#pragma omp simd
   for (size_t i = 0; i < dim1; ++i) { c.data[dim0 + i] = b.data[i]; }
   return c; // RVO
}

template<typename T, size_t dim, bool Q> 
inline std::ostream& operator<<(std::ostream& ostr, const StackVector<T, dim, Q>& vec) {
   static_assert(dim != 0, "can't stream empty vector.");
   ostr << "StackVector<T, " << dim << ">([";
   for (size_t i = 0; i < (dim - 1); ++i) { ostr << vec.data[i] << ", "; }
   ostr << vec.data[dim - 1] << "])";
   return ostr;
}

#endif // WITH_NNUE