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

// header used in NNUE evaluation function

#ifndef NNUE_EVALUATE_NNUE_H_INCLUDED
#define NNUE_EVALUATE_NNUE_H_INCLUDED

#include "nnue_feature_transformer.h"

#include <memory>

namespace Eval::NNUE {

  // Evaluation function file name
  extern std::string fileName;

  // Hash value of evaluation function structure
  constexpr std::uint32_t kHashValue =
      FeatureTransformer::GetHashValue() ^ Network::GetHashValue();

  // Deleter for automating release of memory area
  template <typename T>
  struct AlignedDeleter {
    void operator()(T* ptr) const {
      ptr->~T();
      std_aligned_free(ptr);
    }
  };

  template <typename T>
  using AlignedPtr = std::unique_ptr<T, AlignedDeleter<T>>;

  ScoreType ComputeScore(const Position& pos, bool refresh);
  void Initialize();
  bool ReadParameters(std::istream& stream);
  void UpdateAccumulatorIfPossible(const Position& pos);
}  // namespace Eval::NNUE

#endif // #ifndef NNUE_EVALUATE_NNUE_H_INCLUDED