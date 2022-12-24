#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "inputLayer.hpp"
#include "layer.hpp"
#include "weightReader.hpp"

template<typename NT, bool Q> struct NNUEWeights {

   static constexpr int nbuckets = 2;
   static constexpr int bucketDivisor = 32/nbuckets;

   InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q> w {};
   InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q> b {};

   struct InnerLayer{
     Layer<NT, 2 * firstInnerLayerSize,  8, Q> fc0;
     Layer<NT,  8,  8, Q>                      fc1;
     Layer<NT, 16,  8, Q>                      fc2;
     Layer<NT, 24,  1, Q>                      fc3;
   };

   array1d<InnerLayer, nbuckets> innerLayer;

   uint32_t version {0};

   NNUEWeights<NT, Q>& load(WeightsReader<NT>& ws, bool readVersion) {
      quantizationInfo<Q>();
      if (readVersion) ws.readVersion(version);
      w.load_(ws);
      b.load_(ws);
      for (auto & l : innerLayer) l.fc0.load_(ws);
      for (auto & l : innerLayer) l.fc1.load_(ws);
      for (auto & l : innerLayer) l.fc2.load_(ws);
      for (auto & l : innerLayer) l.fc3.load_(ws);
      return *this;
   }

   static bool load(const std::string& path, NNUEWeights<NT, Q>& loadedWeights) {
      [[maybe_unused]] constexpr uint32_t expectedVersion {0xc0ffee02};
      [[maybe_unused]] constexpr int      expectedSize    {151049100}; // net size + 4 for version
      [[maybe_unused]] constexpr bool     withVersion     {true}; // used for backward compatiblity and debug

      if (path != "embedded") { // read from disk
#ifndef __ANDROID__
#ifndef WITHOUT_FILESYSTEM
         std::error_code ec;
         const auto fsize = std::filesystem::file_size(path, ec);
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
         auto ws = WeightsReader<NT>(stream);
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