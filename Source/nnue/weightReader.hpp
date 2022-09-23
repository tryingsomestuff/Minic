#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

// NT is the network type as written inside the binary file
template<typename NT> struct WeightsReader {
   std::istream* file = nullptr;

   WeightsReader<NT>& readVersion(uint32_t& version) {
      assert(file);
      file->read((char*)&version, sizeof(uint32_t));
      return *this;
   }

   template<typename T> WeightsReader<NT>& streamW(T* dst, const size_t request, [[maybe_unused]] size_t dim0, [[maybe_unused]] size_t dim1) {
      assert(file);
      Logging::LogIt(Logging::logInfo) << "Reading inner weight";
      // we will get min and max weight for display purpose
      NT minW = std::numeric_limits<NT>::max();
      NT maxW = std::numeric_limits<NT>::lowest();
      array1d<char, sizeof(NT)> singleElement {};
      
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         // update min/max
         minW = std::min(minW, tmp);
         maxW = std::max(maxW, tmp);
#if (defined USE_SIMD_INTRIN)
         // transpose data ///@todo as this is default now, do this in trainer ...
         const size_t j = (i % dim1) * dim0 + i / dim1;
#else
         const size_t j = i;
#endif
         dst[j] = static_cast<T>(tmp);
      }
      Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
      return *this;
   }

   template<typename T, bool Q> WeightsReader<NT>& streamWI(T* dst, const size_t request) {
      assert(file);
      Logging::LogIt(Logging::logInfo) << "Reading input weight";
      // we will get min and max weight for display purpose
      NT minW = std::numeric_limits<NT>::max();
      NT maxW = std::numeric_limits<NT>::lowest();
      array1d<char, sizeof(NT)> singleElement {};
      const NT Wscale = Quantization<Q>::scale;
      // read each weight one by one, and scale them if quantization is active
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         // update min/max
         minW = std::min(minW, tmp);
         maxW = std::max(maxW, tmp);
         // if quantization is active and we overflow, just clamp and warn
         if (Q && std::abs(tmp * Wscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Wscale, (NT)std::numeric_limits<T>::lowest(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow weight " << tmp2 << " -> " << tmp;
         }
         else {
            tmp = tmp * Wscale;
         }
         dst[i] = static_cast<T>(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Weight in [" << minW << ", " << maxW << "]";
      return *this;
   }

   template<typename T> WeightsReader<NT>& streamB(T* dst, const size_t request) {
      assert(file);
      Logging::LogIt(Logging::logInfo) << "Reading inner bias";
      // we will get min and max bias for display purpose
      NT minB = std::numeric_limits<NT>::max();
      NT maxB = std::numeric_limits<NT>::lowest();
      array1d<char, sizeof(NT)> singleElement {};
      // read each bias one by one, and scale them if quantization is active
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         // update min/max
         minB = std::min(minB, tmp);
         maxB = std::max(maxB, tmp);
         dst[i] = static_cast<T>(tmp);
      }
      Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
      return *this;
   }

   template<typename T, bool Q> WeightsReader<NT>& streamBI(T* dst, const size_t request) {
      assert(file);
      Logging::LogIt(Logging::logInfo) << "Reading input bias";
      // we will get min and max bias for display purpose
      NT minB = std::numeric_limits<NT>::max();
      NT maxB = std::numeric_limits<NT>::lowest();
      array1d<char, sizeof(NT)> singleElement {};
      const NT Bscale = Quantization<Q>::scale;
      // read each bias one by one, and scale them if quantization is active
      for (size_t i(0); i < request; ++i) {
         file->read(singleElement.data(), singleElement.size());
         NT tmp {0};
         std::memcpy(&tmp, singleElement.data(), singleElement.size());
         // update min/max
         minB = std::min(minB, tmp);
         maxB = std::max(maxB, tmp);
         // if quantization is active and we overflow, just clamp and warn
         if (Q && std::abs(tmp * Bscale) > (NT)std::numeric_limits<T>::max()) {
            NT tmp2 = tmp;
            tmp = std::clamp(tmp2 * Bscale, (NT)std::numeric_limits<T>::lowest(), (NT)std::numeric_limits<T>::max());
            Logging::LogIt(Logging::logWarn) << "Overflow bias " << tmp2 << " -> " << tmp;
         }
         else {
            tmp = tmp * Bscale;
         }
         dst[i] = static_cast<T>(Quantization<Q>::round(tmp));
      }
      Logging::LogIt(Logging::logInfo) << "Bias in [" << minB << ", " << maxB << "]";
      return *this;
   }

   WeightsReader(std::istream& stream): file(&stream) {}
};

#endif // WITH_NNUE