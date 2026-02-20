#pragma once

#include <cstring>

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
FORCE_FINLINE void simdClippedReLU512Helper(float * RESTRICT x, const __m512 & zero, const __m512 & un){
   v_store_f32_512(x, v_max_f32_512(zero, v_min_f32_512(un, v_load_f32_512(x))));
}

template<size_t N, bool Q>
void simdActivation512(float * RESTRICT x, const __m512 & zero, const __m512 & un){
   constexpr int vstep    = v_nlanes_f32_512;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
            simdClippedReLU512Helper<Q>(x + i            , zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep    , zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep * 2, zero, un);
            simdClippedReLU512Helper<Q>(x + i + vstep * 3, zero, un);
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
         simdClippedReLU512Helper<Q>(x + i, zero, un);
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
FORCE_FINLINE void simdClippedReLU256Helper(float * RESTRICT x, const __m256 & zero, const __m256 & un){
   v_store_f32_256(x, v_max_f32_256(zero, v_min_f32_256(un, v_load_f32_256(x))));
}

template<size_t N, bool Q>
void simdActivation256(float * RESTRICT x, const __m256 & zero, const __m256 & un){
   constexpr int vstep    = v_nlanes_f32_256;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
            simdClippedReLU256Helper<Q>(x + i            , zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep    , zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep * 2, zero, un);
            simdClippedReLU256Helper<Q>(x + i + vstep * 3, zero, un);
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
         simdClippedReLU256Helper<Q>(x + i, zero, un);
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

#if defined(__SSE2__)
#if defined(__SSE4_1__)
#define v_cvtepi16_epi32_128 _mm_cvtepi16_epi32
#else
FORCE_FINLINE __m128i v_cvtepi16_epi32_128(__m128i src_i16) {
   const __m128i sign = _mm_srai_epi16(src_i16, 15);
   return _mm_unpacklo_epi16(src_i16, sign);
}
#endif
#endif

template<bool Q>
FORCE_FINLINE void simdClippedReLU128Helper(float * RESTRICT x, const v_f32_128 & zero, const v_f32_128 & un){
   v_store_f32_128(x, v_max_f32_128(zero, v_min_f32_128(un, v_load_f32_128(x))));
}


template<size_t N, bool Q>
void simdActivation128(float * RESTRICT x, const v_f32_128 & zero, const v_f32_128 & un){
   constexpr int vstep    = v_nlanes_f32_128;
   constexpr int unrollx4 = N & (-vstep * 4);
   constexpr int unrollx  = N & -vstep;   
   int i = 0;
   if constexpr(unrollx4){
      while (i < unrollx4) {
            simdClippedReLU128Helper<Q>(x + i            , zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep    , zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep * 2, zero, un);
            simdClippedReLU128Helper<Q>(x + i + vstep * 3, zero, un);
         i += vstep * 4;
      }
   }
   while (i < unrollx) {
         simdClippedReLU128Helper<Q>(x + i, zero, un);
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
FORCE_FINLINE void simdActivationDefault(float * RESTRICT x){
   constexpr int n1 = N & -4;
   for (int i = 0; i < n1; i += 4) {
         x[i]     = std::min(std::max(x[i]    , 0.f), 1.f);
         x[i + 1] = std::min(std::max(x[i + 1], 0.f), 1.f);
         x[i + 2] = std::min(std::max(x[i + 2], 0.f), 1.f);
         x[i + 3] = std::min(std::max(x[i + 3], 0.f), 1.f);
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

template<size_t N>
FORCE_FINLINE void simdAdd_i16(int16_t* RESTRICT dst, const int16_t* RESTRICT src) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 16; // 256 bits / 16 bits
   constexpr size_t unrollx4 = N & (-vstep * 4);
   constexpr size_t unrollx  = N & -vstep;
   if constexpr (unrollx4) {
      while (i < unrollx4) {
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i            ), _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i            )), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i            ))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep    ), _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep    )), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep    ))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep * 2), _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep * 2)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep * 2))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep * 3), _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep * 3)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep * 3))));
         i += vstep * 4;
      }
   }
   while (i + vstep <= unrollx) {
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i))));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 8;
   while (i + vstep128 <= N) {
      _mm_store_si128(reinterpret_cast<__m128i*>(dst + i), _mm_add_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(dst + i)), _mm_load_si128(reinterpret_cast<const __m128i*>(src + i))));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   for (size_t j = 0; j < tail; ++j) dst[i + j] += src[i + j];
}

template<size_t N>
FORCE_FINLINE void simdSub_i16(int16_t* RESTRICT dst, const int16_t* RESTRICT src) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 16;
   constexpr size_t unrollx4 = N & (-vstep * 4);
   constexpr size_t unrollx  = N & -vstep;
   if constexpr (unrollx4) {
      while (i < unrollx4) {
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i            ), _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i            )), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i            ))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep    ), _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep    )), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep    ))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep * 2), _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep * 2)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep * 2))));
         _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + vstep * 3), _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i + vstep * 3)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + vstep * 3))));
         i += vstep * 4;
      }
   }
   while (i + vstep <= unrollx) {
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(dst + i)), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i))));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 8;
   while (i + vstep128 <= N) {
      _mm_store_si128(reinterpret_cast<__m128i*>(dst + i), _mm_sub_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(dst + i)), _mm_load_si128(reinterpret_cast<const __m128i*>(src + i))));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   for (size_t j = 0; j < tail; ++j) dst[i + j] -= src[i + j];
}

template<size_t N>
FORCE_FINLINE void simdCopy_i16(int16_t* RESTRICT dst, const int16_t* RESTRICT src) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 16;
   while (i + vstep <= N) {
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i)));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 8;
   while (i + vstep128 <= N) {
      _mm_store_si128(reinterpret_cast<__m128i*>(dst + i), _mm_load_si128(reinterpret_cast<const __m128i*>(src + i)));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   if (tail) std::memcpy(dst + i, src + i, tail * sizeof(int16_t));
}

template<size_t N>
FORCE_FINLINE void simdCopy_f32(float* RESTRICT dst, const float* RESTRICT src) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 8;
   while (i + vstep <= N) {
      _mm256_store_ps(dst + i, _mm256_load_ps(src + i));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 4;
   while (i + vstep128 <= N) {
      _mm_store_ps(dst + i, _mm_load_ps(src + i));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   if (tail) std::memcpy(dst + i, src + i, tail * sizeof(float));
}

template<size_t N>
FORCE_FINLINE void simdDequantize_i16_f32(float* RESTRICT dst, const int16_t* RESTRICT src, const float scale) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 8; // 8 floats per __m256
   const __m256 vscale = _mm256_set1_ps(scale);
   while (i + vstep <= N) {
      // Load 8 x int16, sign-extend to 8 x int32, convert to 8 x float, multiply by scale
      const __m128i src_i16 = _mm_load_si128(reinterpret_cast<const __m128i*>(src + i));
      const __m256i src_i32 = _mm256_cvtepi16_epi32(src_i16);
      const __m256  src_f32 = _mm256_cvtepi32_ps(src_i32);
      _mm256_store_ps(dst + i, _mm256_mul_ps(src_f32, vscale));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 4;
   const __m128 vscale128 = _mm_set1_ps(scale);
   while (i + vstep128 <= N) {
      // Load 4 x int16 (as 64-bit), sign-extend to 4 x int32, convert to 4 x float
      const __m128i src_i16 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src + i));
      const __m128i src_i32 = v_cvtepi16_epi32_128(src_i16);
      const __m128  src_f32 = _mm_cvtepi32_ps(src_i32);
      _mm_store_ps(dst + i, _mm_mul_ps(src_f32, vscale128));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   for (size_t j = 0; j < tail; ++j) dst[i + j] = scale * static_cast<float>(src[i + j]);
}

template<size_t N0, size_t N1>
FORCE_FINLINE void simdSplice_f32(float* RESTRICT dst, const float* RESTRICT a, const float* RESTRICT b) {
   simdCopy_f32<N0>(dst, a);
   simdCopy_f32<N1>(dst + N0, b);
}

template<size_t N, bool Q>
FORCE_FINLINE void simdDequantizeActivate_i16_f32(float* RESTRICT dst, const int16_t* RESTRICT src, const float scale) {
   size_t i = 0;
#if V_SIMD_256
   constexpr size_t vstep = 8;
   const __m256 vscale = _mm256_set1_ps(scale);
   const __m256 vzero  = _mm256_setzero_ps();
   const __m256 vone   = _mm256_set1_ps(1.0f);
   while (i + vstep <= N) {
      const __m128i src_i16 = _mm_load_si128(reinterpret_cast<const __m128i*>(src + i));
      const __m256i src_i32 = _mm256_cvtepi16_epi32(src_i16);
      const __m256  deq     = _mm256_mul_ps(_mm256_cvtepi32_ps(src_i32), vscale);
      _mm256_store_ps(dst + i, _mm256_max_ps(vzero, _mm256_min_ps(vone, deq)));
      i += vstep;
   }
#endif
#if V_SIMD_128
   constexpr size_t vstep128 = 4;
   const __m128 vscale128 = _mm_set1_ps(scale);
   const __m128 vzero128  = _mm_setzero_ps();
   const __m128 vone128   = _mm_set1_ps(1.0f);
   while (i + vstep128 <= N) {
      const __m128i src_i16 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src + i));
      const __m128i src_i32 = v_cvtepi16_epi32_128(src_i16);
      const __m128  deq     = _mm_mul_ps(_mm_cvtepi32_ps(src_i32), vscale128);
      _mm_store_ps(dst + i, _mm_max_ps(vzero128, _mm_min_ps(vone128, deq)));
      i += vstep128;
   }
#endif
   const size_t tail = N - i;
   for (size_t j = 0; j < tail; ++j) {
      const float deq = scale * static_cast<float>(src[i + j]);
      dst[i + j] = std::max(0.f, std::min(1.f, deq));
   }
}

template<size_t N, bool Q> 
void simdActivation(float* RESTRICT x) {
   size_t i = 0;
   if constexpr (N <= 0) return;

#if V_SIMD_512
   if (N-i >= 16){
      const v_f32_512 zero = v_zero_f32_512();
      const v_f32_512 un   = v_set_f32_512(1.f);      
      simdActivation512<N,Q>(x+i, zero, un);
      i += ((N-i) & -16);
   }
#endif

#if V_SIMD_256
   if (N-i >= 8){
      const v_f32_256 zero = v_zero_f32_256();
      const v_f32_256 un   = v_set_f32_256(1.f);
      simdActivation256<N,Q>(x+i, zero, un);
      i += ((N-i) & -8);
   }
#endif

#if V_SIMD_128
   if (N-i >= 4){
      const v_f32_128 zero = v_zero_f32_128();
      const v_f32_128 un   = v_set_f32_128(1.f);      
      simdActivation128<N,Q>(x+i, zero, un);
      i += ((N-i) & -4);
   }
#endif

   if (N-i >= 4){
      simdActivationDefault<N,Q>(x+i);
      i += ((N-i) & -4);
   }

   while (i < N) {
         x[i] = std::min(std::max(x[i], 0.f), 1.f);
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
   simdActivation256<n,true>(a, zero, un);
   reset();
   }
   {
   const v_f32_128 zero = v_zero_f32_128();
   const v_f32_128 un   = v_set_f32_128(1.f);      
   simdActivation128<n,true>(a, zero, un);
   reset();
   }
   {
   simdActivationDefault<n,true>(a);
   reset();
   }

   return 0;
}
#endif