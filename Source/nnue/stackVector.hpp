#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

template<typename T, size_t dim, bool Q> 
struct StackVector {
   alignas(NNUEALIGNMENT) T data[dim];

   template<typename T2> 
   CONSTEXPR StackVector<T, dim, Q>& add_(const T2* other) {
#ifdef USE_SIMD_INTRIN
      if constexpr (std::is_same_v<T, int16_t> && std::is_same_v<T2, int16_t>) {
         simdAdd_i16<dim>(data, other);
      } else
#endif
      {
#pragma omp simd
         for (size_t i = 0; i < dim; ++i) { data[i] += other[i]; }
      }
      return *this;
   }

   template<typename T2> 
   CONSTEXPR StackVector<T, dim, Q>& sub_(const T2* other) {
#ifdef USE_SIMD_INTRIN
      if constexpr (std::is_same_v<T, int16_t> && std::is_same_v<T2, int16_t>) {
         simdSub_i16<dim>(data, other);
      } else
#endif
      {
#pragma omp simd
         for (size_t i = 0; i < dim; ++i) { data[i] -= other[i]; }
      }
      return *this;
   }

#ifndef USE_SIMD_INTRIN
   template<typename T2, typename T3> 
   CONSTEXPR StackVector<T, dim, Q>& fma_(const T2 c, const T3* other) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] += c * other[i]; }
      return *this;
   }

   template<typename F> 
   CONSTEXPR StackVector<T, dim, Q>& apply_(F&& f) {
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { data[i] = f(data[i]); }
      return *this;
   }   
#else
   T dot_(const T* other) const { return simdDotProduct<dim,Q>(data, other); }
   CONSTEXPR StackVector<T, dim, Q>& activation_() { simdActivation<dim,Q>(data); return *this;}
#endif

   template<typename T2> 
   FORCE_FINLINE void from(const T2* other) {
#ifdef USE_SIMD_INTRIN
      if constexpr (std::is_same_v<T, int16_t> && std::is_same_v<T2, int16_t>) {
         simdCopy_i16<dim>(data, other);
      } else if constexpr (std::is_same_v<T, float> && std::is_same_v<T2, float>) {
         simdCopy_f32<dim>(data, other);
      } else
#endif
      {
#pragma omp simd
         for (size_t i = 0; i < dim; ++i) { data[i] = static_cast<T>(other[i]); }
      }
   }

  // note that quantization is done on read if needed (see weightReader)
  template <typename U> 
  [[nodiscard]] CONSTEXPR StackVector<U, dim, Q> dequantize(const U& scale) const {
    static_assert(std::is_integral_v<T> && std::is_floating_point_v<U>);
    StackVector<U, dim, Q> result;
#ifdef USE_SIMD_INTRIN
    if constexpr (std::is_same_v<T, int16_t> && std::is_same_v<U, float>) {
       simdDequantize_i16_f32<dim>(result.data, data, scale);
    } else
#endif
    {
#pragma omp simd
       for (size_t i = 0; i < dim; ++i) { result.data[i] = scale * static_cast<U>(data[i]); }
    }
    return result; // RVO
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
#ifdef USE_SIMD_INTRIN
   if constexpr (std::is_same_v<T, float>) {
      simdSplice_f32<dim0, dim1>(c.data, a.data, b.data);
   } else
#endif
   {
#pragma omp simd
      for (size_t i = 0; i < dim0; ++i) { c.data[i] = a.data[i]; }
#pragma omp simd
      for (size_t i = 0; i < dim1; ++i) { c.data[dim0 + i] = b.data[i]; }
   }
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