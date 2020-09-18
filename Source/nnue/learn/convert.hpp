#pragma once

#include "definition.hpp"

#ifdef WITH_DATA2BIN

#include <cstdint>
#include <string>
#include <vector>

bool convert_bin(const std::vector<std::string>& filenames, const std::string& output_file_name, 
                 const int ply_minimum, const int ply_maximum, const int interpolate_eval);

bool convert_bin_from_pgn_extract(const std::vector<std::string>& filenames, const std::string& output_file_name, 
                                  const bool pgn_eval_side_to_move, const bool convert_no_eval_fens_as_score_zero);

bool convert_plain(const std::vector<std::string>& filenames, const std::string& output_file_name);

#endif // WITH_DATA2BIN