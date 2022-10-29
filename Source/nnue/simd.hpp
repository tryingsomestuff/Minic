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
typedef __m512 v_f32_512;
#define v_nlanes_f32_512 16
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
#define v_zero_f32_512      _mm512_setzero_ps

FORCE_FINLINE void Log512(const __m512 & value){
    constexpr size_t n = sizeof(__m512) / sizeof(float);
    float buffer[n];
    _mm512_store_ps((void*)buffer, value);
    for (size_t i = 0; i < n; i++)
        std::cout << buffer[i] << " ";
    std::cout << std::endl;
}

template<size_t N> [[nodiscard]] float simdDotProduct512(const float* x, const float* y) {
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
         vsum0 = v_muladd_f32_512(v_load_f32_512(x + i), v_load_f32_512(y + i), vsum0);
         vsum1 = v_muladd_f32_512(v_load_f32_512(x + i + vstep), v_load_f32_512(y + i + vstep), vsum1);
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
   //Log512(vsum0);
   return v_sum_f32_512(vsum0);
}
#endif

//----------------------------------
// AVX
//----------------------------------
#if defined(__AVX2__)
#define V_SIMD_256 256
typedef __m256 v_f32_256;
typedef __m256i v_i32_256;
#define v_nlanes_f32_256 8
#define v_nlanes_i16_256 16
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
#define v_load_f32_256 _mm256_load_ps
#define v_zero_f32_256 _mm256_setzero_ps

FORCE_FINLINE void Log256(const __m256 & value){
    constexpr size_t n = sizeof(__m256) / sizeof(float);
    float buffer[n];
    _mm256_store_ps(buffer, value);
    for (size_t i = 0; i < n; i++)
        std::cout << buffer[i] << " ";
    std::cout << std::endl;
}

template<size_t N> [[nodiscard]] float simdDotProduct256(const float* x, const float* y) {
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
         vsum0 = v_muladd_f32_256(v_load_f32_256(x + i), v_load_f32_256(y + i), vsum0);
         vsum1 = v_muladd_f32_256(v_load_f32_256(x + i + vstep), v_load_f32_256(y + i + vstep), vsum1);
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
   //Log256(vsum0);
   return v_sum_f32_256(vsum0);
}
#endif

//----------------------------------
// SSE
//----------------------------------
#if defined(__SSE2__)
#define V_SIMD_128 128
typedef __m128 v_f32_128;
#define v_nlanes_f32_128 4
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
#define v_load_f32_128 _mm_load_ps
#define v_zero_f32_128 _mm_setzero_ps

FORCE_FINLINE void Log128(const __m128 & value){
    constexpr size_t n = sizeof(__m128) / sizeof(float);
    float buffer[n];
    _mm_store_ps(buffer, value);
    for (size_t i = 0; i < n; i++)
        std::cout << buffer[i] << " ";
    std::cout << std::endl;
}

template<size_t N> [[nodiscard]] float simdDotProduct128(const float* x, const float* y) {
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
         vsum0 = v_muladd_f32_128(v_load_f32_128(x + i), v_load_f32_128(y + i), vsum0);
         vsum1 = v_muladd_f32_128(v_load_f32_128(x + i + vstep), v_load_f32_128(y + i + vstep), vsum1);
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
   //Log128(vsum0);
   return v_sum_f32_128(vsum0);
}

#endif

template<size_t N> [[nodiscard]] float simdDotProductDefault(const float* x, const float* y) {
   constexpr int n1 = N & -4;
   float dot = 0.f;
   for (int i = 0; i < n1; i += 4) { dot += y[i] * x[i] + y[i + 1] * x[i + 1] + y[i + 2] * x[i + 2] + y[i + 3] * x[i + 3]; }
   return dot;
}

template<size_t N> [[nodiscard]] float simdDotProduct(const float* x, const float* y) {
   size_t i  = 0;
   float dot = 0.0f;
   if constexpr (N <= 0) return dot;

#if V_SIMD_512
   if (N-i >= 16){
      dot += simdDotProduct512<N>(x+i,y+i);
      i += ((N-i) & -16);
   }
#endif

#if V_SIMD_256
   if (N-i >= 8){
      dot += simdDotProduct256<N>(x+i,y+i);
      i += ((N-i) & -8);
   }
#endif

#if V_SIMD_128
   if (N-i >= 4){
      dot += simdDotProduct128<N>(x+i,y+i);
      i += ((N-i) & -4);
   }
#endif

   if (N-i >= 4){
      dot += simdDotProductDefault<N>(x+i,y+i);
      i += ((N-i) & -4);
   }

   while (i < N) {
      dot += y[i] * x[i];
      ++i;
   }
   return dot;
}

/*
int main(int, char**){
   constexpr int n = 768;
   alignas(64) float a[n];
   alignas(64) float b[n];
   for (int i = 0; i < n; ++i){
      a[i] = i/1000.f/(n-1);
      b[i] = 1000.f/(n-1)*i;
   }
   std::cout << simdDotProduct512<n>(a,b) << std::endl;
   std::cout << simdDotProduct256<n>(a,b) << std::endl;
   std::cout << simdDotProduct128<n>(a,b) << std::endl;
   std::cout << simdDotProductDefault<n>(a,b) << std::endl;
   return 0;
}
*/
