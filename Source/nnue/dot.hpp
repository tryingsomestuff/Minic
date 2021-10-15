#pragma once

// Highly inspired by/copied from https://github.com/xianyi/OpenBLAS, same naming convention here.

/*
My gcc (and clang) gives those macros for simd extension :

>> gcc -march=skylake-avx512 -dM -E - < /dev/null | egrep "SSE|AVX" | sort

#define __AVX__ 1
#define __AVX2__ 1
#define __AVX512BW__ 1
#define __AVX512CD__ 1
#define __AVX512DQ__ 1
#define __AVX512F__ 1
#define __AVX512VL__ 1
#define __MMX_WITH_SSE__ 1
#define __SSE__ 1
#define __SSE2__ 1
#define __SSE2_MATH__ 1
#define __SSE3__ 1
#define __SSE4_1__ 1
#define __SSE4_2__ 1
#define __SSE_MATH__ 1
#define __SSSE3__ 1
*/

#if defined(_MSC_VER)
#define DOT_INLINE __inline
#elif defined(__GNUC__)
#if defined(__STRICT_ANSI__)
#define DOT_INLINE __inline__
#else
#define DOT_INLINE inline
#endif
#else
#define DOT_INLINE
#endif

#ifdef _MSC_VER
#define DOT_FINLINE static __forceinline
#elif defined(__GNUC__)
#define DOT_FINLINE static DOT_INLINE __attribute__((always_inline))
#else
#define DOT_FINLINE static
#endif

/** SSE **/
#ifdef __SSE__
#include <xmmintrin.h>
#endif
/** SSE2 **/
#ifdef __SSE2__
#include <emmintrin.h>
#endif
/** SSE3 **/
#ifdef __SSE3__
#include <pmmintrin.h>
#endif
/** SSSE3 **/
#ifdef __SSSE3__
#include <tmmintrin.h>
#endif
/** SSE41 **/
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif
/** AVX **/
#if defined(__AVX__) || defined(__FMA__)
#include <immintrin.h>
#endif

/*
//----------------------------------
// AVX512
//----------------------------------
#if defined(__AVX512VL__)
#define V_SIMD 512
typedef __m512 v_f32;
#define v_nlanes_f32 16
#define v_add_f32    _mm512_add_ps
#define v_mul_f32    _mm512_mul_ps
#define v_muladd_f32 _mm512_fmadd_ps
DOT_FINLINE float v_sum_f32(v_f32 a) {
   __m512 h64   = _mm512_shuffle_f32x4(a, a, _MM_SHUFFLE(3, 2, 3, 2));
   __m512 sum32 = _mm512_add_ps(a, h64);
   __m512 h32   = _mm512_shuffle_f32x4(sum32, sum32, _MM_SHUFFLE(1, 0, 3, 2));
   __m512 sum16 = _mm512_add_ps(sum32, h32);
   __m512 h16   = _mm512_permute_ps(sum16, _MM_SHUFFLE(1, 0, 3, 2));
   __m512 sum8  = _mm512_add_ps(sum16, h16);
   __m512 h4    = _mm512_permute_ps(sum8, _MM_SHUFFLE(2, 3, 0, 1));
   __m512 sum4  = _mm512_add_ps(sum8, h4);
   return _mm_cvtss_f32(_mm512_castps512_ps128(sum4));
}
#define v_load_f32(PTR) _mm512_loadu_ps((const __m512*)(PTR))
#define v_zero_f32      _mm512_setzero_ps

//----------------------------------
// AVX
//----------------------------------
#el*/
#if defined(__AVX2__)
#define V_SIMD 256
typedef __m256 v_f32;
#define v_nlanes_f32 8
#define v_add_f32    _mm256_add_ps
#define v_mul_f32    _mm256_mul_ps
#ifdef __FMA__
#define v_muladd_f32 _mm256_fmadd_ps
#else
DOT_FINLINE v_f32 v_muladd_f32(v_f32 a, v_f32 b, v_f32 c) { return v_add_f32(v_mul_f32(a, b), c); }
#endif
DOT_FINLINE float v_sum_f32(__m256 a) {
   __m256 sum_halves = _mm256_hadd_ps(a, a);
   sum_halves        = _mm256_hadd_ps(sum_halves, sum_halves);
   __m128 lo         = _mm256_castps256_ps128(sum_halves);
   __m128 hi         = _mm256_extractf128_ps(sum_halves, 1);
   __m128 sum        = _mm_add_ps(lo, hi);
   return _mm_cvtss_f32(sum);
}
#define v_load_f32 _mm256_loadu_ps
#define v_zero_f32 _mm256_setzero_ps

//----------------------------------
// SSE
//----------------------------------
#elif defined(__SSE2__)
#define V_SIMD 128
typedef __m128 v_f32;
#define v_nlanes_f32 4
#define v_add_f32    _mm_add_ps
#define v_mul_f32    _mm_mul_ps
#ifdef __FMA__
#define v_muladd_f32 _mm_fmadd_ps
#elif defined(__FMA4__)
#define v_muladd_f32 _mm_macc_ps
#else
DOT_FINLINE v_f32 v_muladd_f32(v_f32 a, v_f32 b, v_f32 c) { return v_add_f32(v_mul_f32(a, b), c); }
#endif
DOT_FINLINE float v_sum_f32(__m128 a) {
#ifdef __SSE3__
   __m128 sum_halves = _mm_hadd_ps(a, a);
   return _mm_cvtss_f32(_mm_hadd_ps(sum_halves, sum_halves));
#else
   __m128 t1 = _mm_movehl_ps(a, a);
   __m128 t2 = _mm_add_ps(a, t1);
   __m128 t3 = _mm_shuffle_ps(t2, t2, 1);
   __m128 t4 = _mm_add_ss(t2, t3);
   return _mm_cvtss_f32(t4);
#endif
}
#define v_load_f32 _mm_loadu_ps
#define v_zero_f32 _mm_setzero_ps
#endif

#ifndef V_SIMD
#define V_SIMD 0
#endif

template<size_t N> [[nodiscard]] float dotProductFma(const float* x, const float* y) {
   size_t i  = 0;
   float dot = 0.0f;
   if constexpr (N <= 0) return dot;

#if V_SIMD
   constexpr int vstep    = v_nlanes_f32;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   v_f32 vsum0 = v_zero_f32();
   v_f32 vsum1 = v_zero_f32();
   v_f32 vsum2 = v_zero_f32();
   v_f32 vsum3 = v_zero_f32();
   while (i < unrollx4) {
      vsum0 = v_muladd_f32(v_load_f32(x + i), v_load_f32(y + i), vsum0);
      vsum1 = v_muladd_f32(v_load_f32(x + i + vstep), v_load_f32(y + i + vstep), vsum1);
      vsum2 = v_muladd_f32(v_load_f32(x + i + vstep * 2), v_load_f32(y + i + vstep * 2), vsum2);
      vsum3 = v_muladd_f32(v_load_f32(x + i + vstep * 3), v_load_f32(y + i + vstep * 3), vsum3);
      i += vstep * 4;
   }
   vsum0 = v_add_f32(v_add_f32(vsum0, vsum1), v_add_f32(vsum2, vsum3));
   while (i < unrollx) {
      vsum0 = v_muladd_f32(v_load_f32(x + i), v_load_f32(y + i), vsum0);
      i += vstep;
   }
   dot = v_sum_f32(vsum0);
#else
   constexpr int n1 = N & -4;
   for (; i < n1; i += 4) { dot += y[i] * x[i] + y[i + 1] * x[i + 1] + y[i + 2] * x[i + 2] + y[i + 3] * x[i + 3]; }
#endif
   while (i < N) {
      dot += y[i] * x[i];
      ++i;
   }
   return dot;
}
