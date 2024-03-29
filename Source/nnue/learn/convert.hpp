#pragma once

#include "definition.hpp"

#ifdef WITH_DATA2BIN

bool convert_plain_to_bin(const std::vector<std::string>& filenames,
                          const std::string&              output_file_name,
                          const int                       ply_minimum,
                          const int                       ply_maximum);

bool convert_bin_from_pgn_extract(const std::vector<std::string>& filenames,
                                  const std::string&              output_file_name,
                                  const bool                      pgn_eval_side_to_move,
                                  const bool                      convert_no_eval_fens_as_score_zero);

bool convert_bin_to_plain(const std::vector<std::string>& filenames, 
                          const std::string& output_file_name);

bool rescore(const std::vector<std::string>& filenames, 
             const std::string& output_file_name);

#endif // WITH_DATA2BIN