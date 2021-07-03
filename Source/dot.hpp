#pragma once

template<typename T, size_t N> inline T dotProductFma_(const T* a, const T* b) {
   T sum {0};
#pragma omp simd
   for (size_t i = 0; i < N; ++i) sum += a[i] * b[i];
   return sum;
}

#ifdef __AVX2__

#include <assert.h>
#include <immintrin.h>

template<int offsetRegs> [[nodiscard]]  inline __m256 mul8(const float* p1, const float* p2) {
   constexpr int lanes = offsetRegs * 8;
   const __m256  a     = _mm256_loadu_ps(p1 + lanes);
   const __m256  b     = _mm256_loadu_ps(p2 + lanes);
   return _mm256_mul_ps(a, b);
}

template<int offsetRegs> [[nodiscard]]  inline __m256 fma8(__m256 acc, const float* p1, const float* p2) {
   constexpr int lanes = offsetRegs * 8;
   const __m256  a     = _mm256_loadu_ps(p1 + lanes);
   const __m256  b     = _mm256_loadu_ps(p2 + lanes);
   return _mm256_fmadd_ps(a, b, acc);
}

// dot product of float vectors, using 8-wide FMA instructions.
// from https://stackoverflow.com/questions/59494745/avx2-computing-dot-product-of-512-float-arrays
template<typename T, size_t N> [[nodiscard]] T dotProductFma(const T* a, const T* b) {
   if constexpr (std::is_same_v<T, float> && (N % 32) == 0) { // only for float

      if constexpr (N == 0) return 0;

      const float*       p1    = a;
      const float* const p1End = p1 + N;
      const float*       p2    = b;

      // Process initial 32 values. Nothing to add yet, just multiplying.
      __m256 dot0 = mul8<0>(p1, p2);
      __m256 dot1 = mul8<1>(p1, p2);
      __m256 dot2 = mul8<2>(p1, p2);
      __m256 dot3 = mul8<3>(p1, p2);
      p1 += 8 * 4;
      p2 += 8 * 4;

      // Process the rest of the data.
      // The code uses FMA instructions to multiply + accumulate, consuming 32 values per loop iteration.
      while (p1 < p1End) {
         dot0 = fma8<0>(dot0, p1, p2);
         dot1 = fma8<1>(dot1, p1, p2);
         dot2 = fma8<2>(dot2, p1, p2);
         dot3 = fma8<3>(dot3, p1, p2);
         p1 += 8 * 4;
         p2 += 8 * 4;
      }

      // Add 32 values into 8
      const __m256 dot01   = _mm256_add_ps(dot0, dot1);
      const __m256 dot23   = _mm256_add_ps(dot2, dot3);
      const __m256 dot0123 = _mm256_add_ps(dot01, dot23);
      // Add 8 values into 4
      const __m128 r4 = _mm_add_ps(_mm256_castps256_ps128(dot0123), _mm256_extractf128_ps(dot0123, 1));
      // Add 4 values into 2
      const __m128 r2 = _mm_add_ps(r4, _mm_movehl_ps(r4, r4));
      // Add 2 lower values into the final result
      const __m128 r1 = _mm_add_ss(r2, _mm_movehdup_ps(r2));
      // Return the lowest lane of the result vector.
      // The intrinsic below compiles into noop, modern compilers return floats in the lowest lane of xmm0 register.
      return _mm_cvtss_f32(r1);
   }
   else { // not float
      return dotProductFma_<T, N>(a, b);
   }
}

#else // not __AVX2__

template<typename T, size_t dim> [[nodiscard]] T dotProductFma(const T* a, const T* b) { return dotProductFma_<T, dim>(a, b); }
#endif