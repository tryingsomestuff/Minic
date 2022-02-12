#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

template<typename T, size_t dim> struct StackVector {
   alignas(NNUEALIGNMENT) T data[dim];
#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const StackVector<T, dim>& other) {
      constexpr T eps = std::numeric_limits<T>::epsilon() * 100;
      for (size_t i = 0; i < dim; ++i) {
         if (std::fabs(data[i] - other.data[i]) > eps) {
            std::cout << data[i] << "!=" << other.data[i] << std::endl;
            return false;
         }
      }
      return true;
   }

   bool operator!=(const StackVector<T, dim>& other) {
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

#ifdef USE_SIMD_INTRIN
   inline T dot_(const T* other) const { return dotProductFma<dim>(data, other); }
#endif

   constexpr T item() const {
      static_assert(dim == 1, "called item() on vector with dim != 1");
      return data[0];
   }

   static CONSTEXPR StackVector<T, dim> zeros() {
      alignas(NNUEALIGNMENT) StackVector<T, dim> result {};
#pragma omp simd
      for (size_t i = 0; i < dim; ++i) { result.data[i] = T(0); }
      return result; // RVO
   }

   template<typename T2> static CONSTEXPR StackVector<T, dim> from(const T2* data) {
      alignas(NNUEALIGNMENT) StackVector<T, dim> result {};
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
   alignas(NNUEALIGNMENT) auto c = StackVector<T, dim0 + dim1>::zeros();
#pragma omp simd
   for (size_t i = 0; i < dim0; ++i) { c.data[i] = a.data[i]; }
#pragma omp simd
   for (size_t i = 0; i < dim1; ++i) { c.data[dim0 + i] = b.data[i]; }
   return c; // RVO
}

#endif // WITH_NNUE