#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "inputLayer.hpp"
#include "layer.hpp"
#include "weightReader.hpp"

template<typename NT, bool Q> struct NNUEWeights {

   static constexpr int nbuckets = 4;
   static constexpr int bucketDivisor = 32/nbuckets;

   InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q> w {};
   InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q> b {};
   Layer<NT, 2 * firstInnerLayerSize, 32, Q>       fc0[nbuckets];
   Layer<NT, 32, 32, Q>                            fc1[nbuckets];
   Layer<NT, 64, 32, Q>                            fc2[nbuckets];
   Layer<NT, 96,  1, Q>                            fc3[nbuckets];

   uint32_t version {0};

   NNUEWeights<NT, Q>& load(WeightsReader<NT>& ws, bool readVersion) {
      quantizationInfo<Q>();
      if (readVersion) ws.readVersion(version);
      w.load_(ws);
      b.load_(ws);
      for(int k = 0 ; k < nbuckets; ++k) fc0[k].load_(ws);
      for(int k = 0 ; k < nbuckets; ++k) fc1[k].load_(ws);
      for(int k = 0 ; k < nbuckets; ++k) fc2[k].load_(ws);
      for(int k = 0 ; k < nbuckets; ++k) fc3[k].load_(ws);
      return *this;
   }

   static bool load(const std::string& path, NNUEWeights<NT, Q>& loadedWeights) {
      constexpr uint32_t expectedVersion {0xc0ffee01};
      constexpr int      expectedSize    {50515988}; // net size + 4 for version
      constexpr bool     withVersion     {true}; // used for backward compatiblity and debug

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
         auto         ws = WeightsReader<NT>(stream);
         loadedWeights.load(ws, withVersion);
      }
#ifdef EMBEDDEDNNUEPATH
      else {                                             // read from embedded data
         if (embedded::weightsFileSize != expectedSize) { // with or without version
            Logging::LogIt(Logging::logError) << "File " << path << " does not look like a compatible net";
            return false;
         }
         std::istringstream stream(std::string((const char*)embedded::weightsFileData, embedded::weightsFileSize), std::stringstream::binary);
         auto               ws = WeightsReader<NT>(stream);
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

#endif // WITH_NNUE