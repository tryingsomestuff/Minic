/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2020 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Code for calculating NNUE evaluation function

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

#include "definition.hpp"

#ifdef WITH_NNUE

#include "evaluate_nnue.h"
#include "nnue_accumulator.h"

namespace NNUE {

  // Input feature converter
  LargePagePtr<FeatureTransformer> __feature_transformer;

  // Evaluation function
  AlignedPtr<Network> __network;

  // Get a string that represents the structure of the evaluation function
  std::string GetArchitectureString() {
    return "Features=" + FeatureTransformer::GetStructureString() +
      ",Network=" + Network::GetStructureString();
  }

  namespace Detail {

  // Initialize the evaluation function parameters
  template <typename T>
  void Initialize(AlignedPtr<T>& pointer) {

    pointer.reset(reinterpret_cast<T*>(std_aligned_alloc(alignof(T), sizeof(T))));
    std::memset(pointer.get(), 0, sizeof(T));
  }

  template <typename T>
  void Initialize(LargePagePtr<T>& pointer) {

    //static_assert(alignof(T) <= 4096, "aligned_large_pages_alloc() may fail for such a big alignment requirement of T");
    pointer.reset(reinterpret_cast<T*>(std_aligned_alloc(alignof(T), sizeof(T))));
    std::memset(pointer.get(), 0, sizeof(T));
  }

  // Read evaluation function parameters
  template <typename T>
  bool ReadParameters(std::istream& stream, T& reference) {

    std::uint32_t header;
    header = read_little_endian<std::uint32_t>(stream);
    if (!stream || header != T::GetHashValue()) return false;
    return reference.ReadParameters(stream);
  }

  // write evaluation function parameters
  template <typename T>
  bool WriteParameters(std::ostream& stream, const AlignedPtr<T>& pointer) {
    constexpr std::uint32_t header = T::GetHashValue();
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return pointer->WriteParameters(stream);
  }

  template <typename T>
  bool WriteParameters(std::ostream& stream, const LargePagePtr<T>& pointer) {
    constexpr std::uint32_t header = T::GetHashValue();
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return pointer->WriteParameters(stream);
  }

  }  // namespace Detail

  // Initialize the evaluation function parameters
  void Initialize() {

    Detail::Initialize(__feature_transformer);
    Detail::Initialize(__network);
  }

  // Read network header
  bool ReadHeader(std::istream& stream, std::uint32_t* hash_value, std::string* architecture)
  {
    std::uint32_t version, size;

    version     = read_little_endian<std::uint32_t>(stream);
    *hash_value = read_little_endian<std::uint32_t>(stream);
    size        = read_little_endian<std::uint32_t>(stream);
    if (!stream || version != kVersion) return false;
    architecture->resize(size);
    stream.read(&(*architecture)[0], size);
    return !stream.fail();
  }

  // write the header
  bool WriteHeader(std::ostream& stream,
    std::uint32_t hash_value, const std::string& architecture) {
    stream.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
    stream.write(reinterpret_cast<const char*>(&hash_value), sizeof(hash_value));
    const std::uint32_t size = static_cast<std::uint32_t>(architecture.size());
    stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    stream.write(architecture.data(), size);
    return !stream.fail();
  }

  // Read network parameters
  bool ReadParameters(std::istream& stream) {

    std::uint32_t hash_value;
    std::string architecture;
    if (!ReadHeader(stream, &hash_value, &architecture)) return false;
    if (hash_value != kHashValue) return false;
    if (!Detail::ReadParameters(stream, *__feature_transformer)) return false;
    if (!Detail::ReadParameters(stream, *__network)) return false;
    return stream && stream.peek() == std::ios::traits_type::eof();
  }
  // write evaluation function parameters
  bool WriteParameters(std::ostream& stream) {
    if (!WriteHeader(stream, kHashValue, GetArchitectureString())) return false;
    if (!Detail::WriteParameters(stream, __feature_transformer)) return false;
    if (!Detail::WriteParameters(stream, __network)) return false;
    return !stream.fail();
  }
  // Evaluation function. Perform differential calculation.
  NNUEValue ComputeScore(const Position& pos) {

    alignas(kCacheLineSize) TransformedFeatureType
        transformed_features[FeatureTransformer::kBufferSize];
    __feature_transformer->Transform(pos, transformed_features);
    alignas(kCacheLineSize) char buffer[Network::kBufferSize];
    const auto output = __network->Propagate(transformed_features, buffer);

    return static_cast<NNUEValue>(output[0] / FV_SCALE);
  }

} // namespace NNUE

#endif
