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

//----------------------------------
// AVX512
//----------------------------------
#if defined(__AVX512VL__NONONO)
#define V_SIMD_512 512
using v_f32_512 = __m512;
inline constexpr auto v_nlanes_f32_512 = 16;
#define v_add_f32_512    _mm512_add_ps
#define v_mul_f32_512    _mm512_mul_ps
#define v_muladd_f32_512 _mm512_fmadd_ps

FORCE_FINLINE float v_sum_f32_256(__m256 a) {
   __m256 sum_halves = _mm256_hadd_ps(a, a);
   sum_halves        = _mm256_hadd_ps(sum_halves, sum_halves);
   const __m128 lo   = _mm256_castps256_ps128(sum_halves);
   const __m128 hi   = _mm256_extractf128_ps(sum_halves, 1);
   const __m128 sum  = _mm_add_ps(lo, hi);
   return _mm_cvtss_f32(sum);
}

FORCE_FINLINE float v_sum_f32_512(__m512 a) {
   const __m256 low  = _mm512_castps512_ps256(a);
   const __m256 high = _mm512_extractf32x8_ps(a,1);
   return v_sum_f32_256(low+high);
}

#define v_load_f32_512(PTR) _mm512_load_ps((const __m512*)(PTR))
#define v_store_f32_512     _mm512_store_ps
#define v_zero_f32_512      _mm512_setzero_ps
#define v_set_f32_512       _mm512_set1_ps
#define v_max_f32_512       _mm512_max_ps
#define v_min_f32_512       _mm512_min_ps

template<bool Q>
FORCE_FINLINE void simdReLU512Helper(float * RESTRICT x, const float * RESTRICT a, const __m512 & zero){
   v_store_f32_512(x, v_max_f32_512(zero, v_load_f32_512(x)));
}

template<bool Q>
FORCE_FINLINE void simdClippedReLU512Helper(float * RESTRICT x, const float * RESTRICT a, const __m512 & zero, const __m512 & un){
   v_store_f32_512(x, v_max_f32_512(zero, v_min_f32_512(un, v_mul_f32_512(v_load_f32_512(x), v_load_f32_512(a)))));
}

template<bool Q>
FORCE_FINLINE void simdPReLU512Helper(float * RESTRICT x, const float * RESTRICT a, const __m512 & zero){
   const auto v = v_load_f32_512(x);
   v_store_f32_512(x, v_max_f32_512(zero, v) + v_mul_f32_512(v_load_f32_512(a), v_min_f32_512(zero, v)));
}

template<bool Q>
FORCE_FINLINE void simdClippedPReLU512Helper(float * RESTRICT x, const float * RESTRICT a, const __m512 & zero, const __m512 & un){
   const auto v = v_load_f32_512(x);
   v_store_f32_512(x, v_min_f32_512((un, v_max_f32_512(zero, v) + v_mul_f32_512(v_load_f32_512(a), v_min_f32_512(zero, v))));
}

FORCE_FINLINE void simdSigmoid512Helper(float * RESTRICT x, const float * RESTRICT a) {
    const __m256 two = v_set_f32_512(2.0f);
    const __m256 point_five = v_set_f32_512(0.5f);
    const __m256 ax = v_mul_f32_512(v_load_f32_512(a), v_load_f32_512(x));
    const __m256 abs_ax = _mm128_andnot_ps(v_set_f32_512(-0.0f), ax);
    const __m256 numerator = v_muladd_f32_512(abs_ax, two, two);
    const __m256 result = _mm128_div_ps(ax, numerator);
    v_store_f32_512(x, v_add_f32_512(result, point_five));
}

template<size_t N, bool Q>
void simdActivation512(float * RESTRICT x, const float * RESTRICT a, [[maybe_unused]] const __m512 & zero, [[maybe_unused]] const __m512 & un){
   constexpr int vstep    = v_nlanes_f32_512;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
         if constexpr (activationType == eReLU){
            simdReLU512Helper<Q>(x + i            , a + i            , zero);
            simdReLU512Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdReLU512Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdReLU512Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }
         if constexpr (activationType == eClippedReLU){
            simdClippedReLU512Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }
         if constexpr (activationType == ePReLU){
            simdPReLU512Helper<Q>(x + i            , a + i            , zero);
            simdPReLU512Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdPReLU512Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdPReLU512Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }
         if constexpr (activationType == eClippedPReLU){
            simdClippedPReLU512Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedPReLU512Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedPReLU512Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedPReLU512Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }
         if constexpr (activationType == eApproxSigmoid){
            simdSigmoid512Helper(x + i            , a + i            );
            simdSigmoid512Helper(x + i + vstep    , a + i + vstep    );
            simdSigmoid512Helper(x + i + vstep * 2, a + i + vstep * 2);
            simdSigmoid512Helper(x + i + vstep * 3, a + i + vstep * 3);
         }
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
      if constexpr (activationType == eReLU){
         simdReLU512Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedReLU){
         simdClippedReLU512Helper<Q>(x + i, a + i, zero, un);
      }
      if constexpr (activationType == ePReLU){
         simdClippedPReLU512Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedPReLU){
         simdPReLU512Helper<Q>(x + i, a + i, zero, un);
      }      
      if constexpr (activationType == eApproxSigmoid){
         simdSigmoid512Helper(x + i, a + i);
      }
      i += vstep;
   }
}

template<size_t N, bool Q>
[[nodiscard]] float simdDotProduct512(const float* RESTRICT x, const float* RESTRICT y) {
   constexpr int vstep    = v_nlanes_f32_512;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   v_f32_512 vsum0 = v_zero_f32_512();
   if constexpr(unrollx4){
      v_f32_512 vsum1 = v_zero_f32_512();
      v_f32_512 vsum2 = v_zero_f32_512();
      v_f32_512 vsum3 = v_zero_f32_512();
      while (i < unrollx4) {
         vsum0 = v_muladd_f32_512(v_load_f32_512(x + i            ), v_load_f32_512(y + i            ), vsum0);
         vsum1 = v_muladd_f32_512(v_load_f32_512(x + i + vstep    ), v_load_f32_512(y + i + vstep    ), vsum1);
         vsum2 = v_muladd_f32_512(v_load_f32_512(x + i + vstep * 2), v_load_f32_512(y + i + vstep * 2), vsum2);
         vsum3 = v_muladd_f32_512(v_load_f32_512(x + i + vstep * 3), v_load_f32_512(y + i + vstep * 3), vsum3);
         i += vstep * 4;
      }
      vsum0 = v_add_f32_512(v_add_f32_512(vsum0, vsum1), v_add_f32_512(vsum2, vsum3));
   }
   while (i < unrollx) {
      vsum0 = v_muladd_f32_512(v_load_f32_512(x + i), v_load_f32_512(y + i), vsum0);
      i += vstep;
   }
   return v_sum_f32_512(vsum0);
}
#endif

//----------------------------------
// AVX
//----------------------------------
#if defined(__AVX2__)
#define V_SIMD_256 256
using v_f32_256 = __m256;
inline constexpr auto v_nlanes_f32_256 = 8;
#define v_add_f32_256    _mm256_add_ps
#define v_mul_f32_256    _mm256_mul_ps
#ifdef __FMA__
#define v_muladd_f32_256 _mm256_fmadd_ps
#else
FORCE_FINLINE __m256 v_muladd_f32_256(__m256 a, __m256 b, __m256 c) { return v_add_f32_256(v_mul_f32_256(a, b), c); }
#endif
#ifndef V_SIMD_512
FORCE_FINLINE float v_sum_f32_256(__m256 a) {
   __m256 sum_halves = _mm256_hadd_ps(a, a);
   sum_halves        = _mm256_hadd_ps(sum_halves, sum_halves);
   const __m128 lo   = _mm256_castps256_ps128(sum_halves);
   const __m128 hi   = _mm256_extractf128_ps(sum_halves, 1);
   const __m128 sum  = _mm_add_ps(lo, hi);
   return _mm_cvtss_f32(sum);
}
#endif
#define v_load_f32_256  _mm256_load_ps
#define v_store_f32_256 _mm256_store_ps
#define v_zero_f32_256  _mm256_setzero_ps
#define v_set_f32_256   _mm256_set1_ps
#define v_max_f32_256   _mm256_max_ps
#define v_min_f32_256   _mm256_min_ps

template<bool Q>
FORCE_FINLINE void simdReLU256Helper(float * RESTRICT x, const float * RESTRICT a, const __m256 & zero){
   v_store_f32_256(x, v_max_f32_256(zero, v_load_f32_256(x)));
}

template<bool Q>
FORCE_FINLINE void simdClippedReLU256Helper(float * RESTRICT x, const float * RESTRICT a, const __m256 & zero, const __m256 & un){
   v_store_f32_256(x, v_max_f32_256(zero, v_min_f32_256(un, v_mul_f32_256(v_load_f32_256(x), v_load_f32_256(a)))));
}

template<bool Q>
FORCE_FINLINE void simdPReLU256Helper(float * RESTRICT x, const float * RESTRICT a, const __m256 & zero){
   const auto v = v_load_f32_256(x);
   v_store_f32_256(x, v_max_f32_256(zero, v) + v_mul_f32_256(v_load_f32_256(a), v_min_f32_256(zero, v)));
}

template<bool Q>
FORCE_FINLINE void simdClippedPReLU256Helper(float * RESTRICT x, const float * RESTRICT a, const __m256 & zero, const __m256 & un){
   const auto v = v_load_f32_256(x);
   v_store_f32_256(x, v_min_f32_256(un, v_max_f32_256(zero, v) + v_mul_f32_256(v_load_f32_256(a), v_min_f32_256(zero, v))));
}

FORCE_FINLINE void simdSigmoid256Helper(float * RESTRICT x, const float * RESTRICT a) {
    const v_f32_256 two = v_set_f32_256(2.0f);
    const v_f32_256 point_five = v_set_f32_256(0.5f);
    const v_f32_256 ax = v_mul_f32_256(v_load_f32_256(a), v_load_f32_256(x));
    const v_f32_256 abs_ax = _mm256_andnot_ps(v_set_f32_256(-0.0f), ax);
    const v_f32_256 numerator = v_muladd_f32_256(abs_ax, two, two);
    const v_f32_256 result = _mm256_div_ps(ax, numerator);
    v_store_f32_256(x, v_add_f32_256(result, point_five));
}

template<size_t N, bool Q>
void simdActivation256(float * RESTRICT x, const float * RESTRICT a, [[maybe_unused]] const __m256 & zero, [[maybe_unused]] const __m256 & un){
   constexpr int vstep    = v_nlanes_f32_256;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
         if constexpr (activationType == eReLU){
            simdReLU256Helper<Q>(x + i            , a + i            , zero);
            simdReLU256Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdReLU256Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdReLU256Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }
         if constexpr (activationType == eClippedReLU){
            simdClippedReLU256Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }
         if constexpr (activationType == ePReLU){
            simdPReLU256Helper<Q>(x + i            , a + i            , zero);
            simdPReLU256Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdPReLU256Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdPReLU256Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }        
         if constexpr (activationType == eClippedPReLU){
            simdClippedPReLU256Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedPReLU256Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedPReLU256Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedPReLU256Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }             
         if constexpr (activationType == eApproxSigmoid){
            simdSigmoid256Helper(x + i            , a + i            );
            simdSigmoid256Helper(x + i + vstep    , a + i + vstep    );
            simdSigmoid256Helper(x + i + vstep * 2, a + i + vstep * 2);
            simdSigmoid256Helper(x + i + vstep * 3, a + i + vstep * 3);
         }
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
      if constexpr (activationType == eReLU){
         simdReLU256Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedReLU){
         simdClippedReLU256Helper<Q>(x + i, a + i, zero, un);
      }
      if constexpr (activationType == ePReLU){
         simdPReLU256Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedPReLU){
         simdClippedPReLU256Helper<Q>(x + i, a + i, zero, un);
      }
      if constexpr (activationType == eApproxSigmoid){
         simdSigmoid256Helper(x + i, a + i);
      }
      i += vstep;
   }
}

template<size_t N, bool Q> 
[[nodiscard]] float simdDotProduct256(const float* RESTRICT x, const float* RESTRICT y) {
   constexpr int vstep    = v_nlanes_f32_256;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   v_f32_256 vsum0 = v_zero_f32_256();
   if constexpr(unrollx4){
      v_f32_256 vsum1 = v_zero_f32_256();
      v_f32_256 vsum2 = v_zero_f32_256();
      v_f32_256 vsum3 = v_zero_f32_256();
      while (i < unrollx4) {
         vsum0 = v_muladd_f32_256(v_load_f32_256(x + i            ), v_load_f32_256(y + i            ), vsum0);
         vsum1 = v_muladd_f32_256(v_load_f32_256(x + i + vstep    ), v_load_f32_256(y + i + vstep    ), vsum1);
         vsum2 = v_muladd_f32_256(v_load_f32_256(x + i + vstep * 2), v_load_f32_256(y + i + vstep * 2), vsum2);
         vsum3 = v_muladd_f32_256(v_load_f32_256(x + i + vstep * 3), v_load_f32_256(y + i + vstep * 3), vsum3);
         i += vstep * 4;
      }
      vsum0 = v_add_f32_256(v_add_f32_256(vsum0, vsum1), v_add_f32_256(vsum2, vsum3));
   }
   while (i < unrollx) {
      vsum0 = v_muladd_f32_256(v_load_f32_256(x + i), v_load_f32_256(y + i), vsum0);
      i += vstep;
   }
   return v_sum_f32_256(vsum0);
}
#endif

//----------------------------------
// SSE
//----------------------------------
#if defined(__SSE2__)
#define V_SIMD_128 128
using v_f32_128 = __m128;
inline constexpr auto v_nlanes_f32_128 = 4;
#define v_add_f32_128    _mm_add_ps
#define v_mul_f32_128    _mm_mul_ps
#ifdef __FMA__
#define v_muladd_f32_128 _mm_fmadd_ps
//#elif defined(__FMA4__)
//#define v_muladd_f32_128 _mm_macc_ps
#else
FORCE_FINLINE __m128 v_muladd_f32_128(__m128 a, __m128 b, __m128 c) { return v_add_f32_128(v_mul_f32_128(a, b), c); }
#endif
FORCE_FINLINE float v_sum_f32_128(__m128 a) {
#ifdef __SSE3__
   const __m128 sum_halves = _mm_hadd_ps(a, a);
   return _mm_cvtss_f32(_mm_hadd_ps(sum_halves, sum_halves));
#else
   const __m128 t1 = _mm_movehl_ps(a, a);
   const __m128 t2 = _mm_add_ps(a, t1);
   const __m128 t3 = _mm_shuffle_ps(t2, t2, 1);
   const __m128 t4 = _mm_add_ss(t2, t3);
   return _mm_cvtss_f32(t4);
#endif
}
#define v_load_f32_128  _mm_load_ps
#define v_store_f32_128 _mm_store_ps
#define v_zero_f32_128  _mm_setzero_ps
#define v_set_f32_128   _mm_set1_ps
#define v_max_f32_128   _mm_max_ps
#define v_min_f32_128   _mm_min_ps

template<bool Q>
FORCE_FINLINE void simdReLU128Helper(float * RESTRICT x, const float * RESTRICT a, const v_f32_128 & zero){
   v_store_f32_128(x, v_max_f32_128(zero, v_load_f32_128(x)));
}

template<bool Q>
FORCE_FINLINE void simdClippedReLU128Helper(float * RESTRICT x, const float * RESTRICT a, const v_f32_128 & zero, const v_f32_128 & un){
   v_store_f32_128(x, v_max_f32_128(zero, v_min_f32_128(un, v_mul_f32_128(v_load_f32_128(x),v_load_f32_128(a)))));
}

template<bool Q>
FORCE_FINLINE void simdPReLU128Helper(float * RESTRICT x, const float * RESTRICT a, const v_f32_128 & zero){
   const auto v = v_load_f32_128(x);
   v_store_f32_128(x, v_max_f32_128(zero, v) + v_mul_f32_128(v_load_f32_128(a), v_min_f32_128(zero, v)));
}

template<bool Q>
FORCE_FINLINE void simdClippedPReLU128Helper(float * RESTRICT x, const float * RESTRICT a, const v_f32_128 & zero, const v_f32_128 & un){
   const auto v = v_load_f32_128(x);
   v_store_f32_128(x, v_min_f32_128(un, v_max_f32_128(zero, v) + v_mul_f32_128(v_load_f32_128(a), v_min_f32_128(zero, v))));
}


FORCE_FINLINE void simdSigmoid128Helper(float * RESTRICT x, const float * RESTRICT a) {
    const v_f32_128 two = v_set_f32_128(2.0f);
    const v_f32_128 point_five = v_set_f32_128(0.5f);
    const v_f32_128 ax = v_mul_f32_128(v_load_f32_128(a), v_load_f32_128(x));
    const v_f32_128 abs_ax = _mm_andnot_ps(v_set_f32_128(-0.0f), ax);
    const v_f32_128 numerator = v_muladd_f32_128(abs_ax, two, two);
    const v_f32_128 result = _mm_div_ps(ax, numerator);
    v_store_f32_128(x, v_add_f32_128(result, point_five));
}

template<size_t N, bool Q>
void simdActivation128(float * RESTRICT x, const float * RESTRICT a, [[maybe_unused]] const v_f32_128 & zero, [[maybe_unused]] const v_f32_128 & un){
   constexpr int vstep    = v_nlanes_f32_128;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;   
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
         if constexpr (activationType == eReLU){
            simdReLU128Helper<Q>(x + i            , a + i            , zero);
            simdReLU128Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdReLU128Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdReLU128Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }
         if constexpr (activationType == eClippedReLU){
            simdClippedReLU128Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }
         if constexpr (activationType == ePReLU){
            simdPReLU128Helper<Q>(x + i            , a + i            , zero);
            simdPReLU128Helper<Q>(x + i + vstep    , a + i + vstep    , zero);
            simdPReLU128Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero);
            simdPReLU128Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero);
         }
         if constexpr (activationType == eClippedPReLU){
            simdClippedPReLU128Helper<Q>(x + i            , a + i            , zero, un);
            simdClippedPReLU128Helper<Q>(x + i + vstep    , a + i + vstep    , zero, un);
            simdClippedPReLU128Helper<Q>(x + i + vstep * 2, a + i + vstep * 2, zero, un);
            simdClippedPReLU128Helper<Q>(x + i + vstep * 3, a + i + vstep * 3, zero, un);
         }             
         if constexpr (activationType == eApproxSigmoid){
            simdSigmoid128Helper(x + i            , a + i            );
            simdSigmoid128Helper(x + i + vstep    , a + i + vstep    );
            simdSigmoid128Helper(x + i + vstep * 2, a + i + vstep * 2);
            simdSigmoid128Helper(x + i + vstep * 3, a + i + vstep * 3);
         }
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
      if constexpr (activationType == eReLU){
         simdReLU128Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedReLU){
         simdClippedReLU128Helper<Q>(x + i, a + i, zero, un);
      }
      if constexpr (activationType == ePReLU){
         simdPReLU128Helper<Q>(x + i, a + i, zero);
      }
      if constexpr (activationType == eClippedPReLU){
         simdClippedPReLU128Helper<Q>(x + i, a + i, zero, un);
      }      
      if constexpr (activationType == eApproxSigmoid){
         simdSigmoid128Helper(x + i, a + i);
      }
      i += vstep;
   }
}

template<size_t N, bool Q> 
[[nodiscard]] float simdDotProduct128(const float* RESTRICT x, const float* RESTRICT y) {
   constexpr int vstep    = v_nlanes_f32_128;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   v_f32_128 vsum0 = v_zero_f32_128();
   if constexpr(unrollx4){
      v_f32_128 vsum1 = v_zero_f32_128();
      v_f32_128 vsum2 = v_zero_f32_128();
      v_f32_128 vsum3 = v_zero_f32_128();
      while (i < unrollx4) {
         vsum0 = v_muladd_f32_128(v_load_f32_128(x + i            ), v_load_f32_128(y + i            ), vsum0);
         vsum1 = v_muladd_f32_128(v_load_f32_128(x + i + vstep    ), v_load_f32_128(y + i + vstep    ), vsum1);
         vsum2 = v_muladd_f32_128(v_load_f32_128(x + i + vstep * 2), v_load_f32_128(y + i + vstep * 2), vsum2);
         vsum3 = v_muladd_f32_128(v_load_f32_128(x + i + vstep * 3), v_load_f32_128(y + i + vstep * 3), vsum3);
         i += vstep * 4;
      }
      vsum0 = v_add_f32_128(v_add_f32_128(vsum0, vsum1), v_add_f32_128(vsum2, vsum3));
   }
   while (i < unrollx) {
      vsum0 = v_muladd_f32_128(v_load_f32_128(x + i), v_load_f32_128(y + i), vsum0);
      i += vstep;
   }
   return v_sum_f32_128(vsum0);
}

#endif

template<size_t N, bool Q> 
FORCE_FINLINE void simdActivationDefault(float * RESTRICT x, const float * RESTRICT a){
   constexpr int n1 = N & -4;
   for (int i = 0; i < n1; i += 4) {
      if constexpr (activationType == eReLU){
         x[i]     = std::max(x[i    ] * a[i    ], 0.f);
         x[i + 1] = std::max(x[i + 1] * a[i + 1], 0.f);
         x[i + 2] = std::max(x[i + 2] * a[i + 2], 0.f);
         x[i + 3] = std::max(x[i + 3] * a[i + 3], 0.f);
      }
      if constexpr (activationType == eClippedReLU){
         x[i]     = std::min(std::max(x[i    ] * a[i    ], 0.f), 1.f);
         x[i + 1] = std::min(std::max(x[i + 1] * a[i + 1], 0.f), 1.f);
         x[i + 2] = std::min(std::max(x[i + 2] * a[i + 2], 0.f), 1.f);
         x[i + 3] = std::min(std::max(x[i + 3] * a[i + 3], 0.f), 1.f);
      }
      if constexpr (activationType == ePReLU){
         x[i]     = std::max(0.f, x[i    ]) + a[i    ] * std::min( 0.f, x[i    ]);
         x[i + 1] = std::max(0.f, x[i + 1]) + a[i + 1] * std::min( 0.f, x[i + 1]);
         x[i + 2] = std::max(0.f, x[i + 2]) + a[i + 2] * std::min( 0.f, x[i + 2]);
         x[i + 3] = std::max(0.f, x[i + 3]) + a[i + 3] * std::min( 0.f, x[i + 3]);
      }
      if constexpr (activationType == eClippedPReLU){
         x[i]     = std::min(1.f, std::max(0.f, x[i    ]) + a[i    ] * std::min( 0.f, x[i    ]));
         x[i + 1] = std::min(1.f, std::max(0.f, x[i + 1]) + a[i + 1] * std::min( 0.f, x[i + 1]));
         x[i + 2] = std::min(1.f, std::max(0.f, x[i + 2]) + a[i + 2] * std::min( 0.f, x[i + 2]));
         x[i + 3] = std::min(1.f, std::max(0.f, x[i + 3]) + a[i + 3] * std::min( 0.f, x[i + 3]));
      }
      if constexpr (activationType == eApproxSigmoid){
         x[i]     = a[i    ]*x[i    ] / ( 2 * std::fabs(a[i    ]*x[i    ]) + 2) + 0.5;
         x[i + 1] = a[i + 1]*x[i + 1] / ( 2 * std::fabs(a[i + 1]*x[i + 1]) + 2) + 0.5;
         x[i + 2] = a[i + 2]*x[i + 2] / ( 2 * std::fabs(a[i + 2]*x[i + 2]) + 2) + 0.5;
         x[i + 3] = a[i + 3]*x[i + 3] / ( 2 * std::fabs(a[i + 3]*x[i + 3]) + 2) + 0.5;
      }
   }   
}

template<size_t N, bool Q> 
[[nodiscard]] float simdDotProductDefault(const float* RESTRICT x, const float* RESTRICT y) {
   constexpr int n1 = N & -4;
   float dot = 0.f;
   for (int i = 0; i < n1; i += 4) { 
      dot += y[i    ] * x[i    ]
           + y[i + 1] * x[i + 1] 
           + y[i + 2] * x[i + 2] 
           + y[i + 3] * x[i + 3]; 
   }
   return dot;
}

template<size_t N, bool Q> 
void simdActivation(float* RESTRICT x, const float* a) {
   size_t i = 0;
   if constexpr (N <= 0) return;

#if V_SIMD_512
   if (N-i >= 16){
      const v_f32_512 zero = v_zero_f32_512();
      const v_f32_512 un   = v_set_f32_512(1.f);      
      simdActivation512<N,Q>(x+i, a+i, zero, un);
      i += ((N-i) & -16);
   }
#endif

#if V_SIMD_256
   if (N-i >= 8){
      const v_f32_256 zero = v_zero_f32_256();
      const v_f32_256 un   = v_set_f32_256(1.f);
      simdActivation256<N,Q>(x+i, a+i, zero, un);
      i += ((N-i) & -8);
   }
#endif

#if V_SIMD_128
   if (N-i >= 4){
      const v_f32_128 zero = v_zero_f32_128();
      const v_f32_128 un   = v_set_f32_128(1.f);      
      simdActivation128<N,Q>(x+i, a+i, zero, un);
      i += ((N-i) & -4);
   }
#endif

   if (N-i >= 4){
      simdActivationDefault<N,Q>(x+i, a+i);
      i += ((N-i) & -4);
   }

   while (i < N) {
      if constexpr (activationType == eReLU){
         x[i] = std::max(x[i] * a[i], 0.f);
      }
      if constexpr (activationType == eClippedReLU){
         x[i] = std::min(std::max(x[i] * a[i], 0.f), 1.f);
      }
      if constexpr (activationType == ePReLU){
         x[i] = std::max(0.f, x[i]) + a[i] * std::min( 0.f, x[i]);
      }
      if constexpr (activationType == eClippedPReLU){
         x[i] = std::min(1.f, std::max(0.f, x[i]) + a[i] * std::min( 0.f, x[i]));
      }      
      if constexpr (activationType == eApproxSigmoid){
         x[i] = a[i]*x[i] / ( 2 * std::fabs(a[i]*x[i]) + 2) + 0.5;
      }
      ++i;
   }
}

template<size_t N, bool Q> 
[[nodiscard]] float simdDotProduct(const float* RESTRICT x, const float* RESTRICT y) {
   size_t i = 0;
   float dot = 0.0f;
   if constexpr (N <= 0) return dot;

#if V_SIMD_512
   if (N-i >= 16){
      dot += simdDotProduct512<N,Q>(x+i,y+i);
      i += ((N-i) & -16);
   }
#endif

#if V_SIMD_256
   if (N-i >= 8){
      dot += simdDotProduct256<N,Q>(x+i,y+i);
      i += ((N-i) & -8);
   }
#endif

#if V_SIMD_128
   if (N-i >= 4){
      dot += simdDotProduct128<N,Q>(x+i,y+i);
      i += ((N-i) & -4);
   }
#endif

   if (N-i >= 4){
      dot += simdDotProductDefault<N,Q>(x+i,y+i);
      i += ((N-i) & -4);
   }

   while (i < N) {
      dot += y[i] * x[i];
      ++i;
   }
   return dot;
}



#ifdef TESTING
int main(int, char**){
   constexpr int n = 768;
   alignas(64) float a[n];
   alignas(64) float b[n];

   for (int i = 0; i < n; ++i){
      a[i] = (i+1)/1000.f/(n-1);
      b[i] = 1.f/a[i];
   }
   //std::cout << simdDotProduct512<n,true>(a,b) << std::endl;
   std::cout << simdDotProduct256<n,true>(a,b) << std::endl;
   std::cout << simdDotProduct128<n,true>(a,b) << std::endl;
   std::cout << simdDotProductDefault<n,true>(a,b) << std::endl;

   for (int i = 0; i < n; ++i){
      a[i] = -5.f + 10.f*i/(n-1);
      b[i] = 2.f;
   }

   auto reset = [&](){
      for (int i = 0; i < n; ++i){
         std::cout << a[i] << " ";
         a[i] = -5.f + 10.f*i/(n-1);
      }  
      std::cout << std::endl;    
   };

   {
   const v_f32_256 zero = v_zero_f32_256();
   const v_f32_256 un   = v_set_f32_256(1.f);
   simdActivation256<n,true>(a,b, zero, un);
   reset();
   }
   {
   const v_f32_128 zero = v_zero_f32_128();
   const v_f32_128 un   = v_set_f32_128(1.f);      
   simdActivation128<n,true>(a,b, zero, un);
   reset();
   }
   {
   simdActivationDefault<n,true>(a,b);
   reset();
   }

   return 0;
}
#endif