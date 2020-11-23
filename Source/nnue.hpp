#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "logging.hpp"
#include "dynamicConfig.hpp"
#include "nnue_impl.hpp"

#ifndef NDEBUG
   #define SCALINGCOUNT 5000
#else
   #define SCALINGCOUNT 50000
#endif

// Internal wrapper to the NNUE things
namespace NNUEWrapper{

  typedef float nnueType;

  // NNUE eval scaling factor
  extern int NNUEscaling;

  void compute_scaling(int count = SCALINGCOUNT);

  inline void init(){
     if ( !DynamicConfig::NNUEFile.empty() ){
        Logging::LogIt(Logging::logInfoPrio) << "Loading NNUE net " << DynamicConfig::NNUEFile;
        if ( nnue::half_kp_weights<nnueType>{}.load(DynamicConfig::NNUEFile, nnue::half_kp_eval<nnueType>::weights)){
           DynamicConfig::useNNUE = true;
           compute_scaling();
        }
        else{
           Logging::LogIt(Logging::logInfoPrio) << "Fail to load NNUE net, using standard evaluation";
           DynamicConfig::useNNUE = false;
        }
     }
     else{
        Logging::LogIt(Logging::logInfoPrio) << "No NNUE net loaded, using standard evaluation";
        DynamicConfig::useNNUE = false;
     }
  }

} // NNUEWrapper

using NNUEEvaluator = nnue::half_kp_eval<NNUEWrapper::nnueType>;

namespace feature_idx{

    constexpr size_t major = 64 * 12;
    constexpr size_t minor = 64;

    constexpr size_t us_pawn_offset     = 0;
    constexpr size_t us_knight_offset   = us_pawn_offset   + minor;
    constexpr size_t us_bishop_offset   = us_knight_offset + minor;
    constexpr size_t us_rook_offset     = us_bishop_offset + minor;
    constexpr size_t us_queen_offset    = us_rook_offset   + minor;
    constexpr size_t us_king_offset     = us_queen_offset  + minor;

    constexpr size_t them_pawn_offset   = us_king_offset     + minor;
    constexpr size_t them_knight_offset = them_pawn_offset   + minor;
    constexpr size_t them_bishop_offset = them_knight_offset + minor;
    constexpr size_t them_rook_offset   = them_bishop_offset + minor;
    constexpr size_t them_queen_offset  = them_rook_offset   + minor;
    constexpr size_t them_king_offset   = them_queen_offset  + minor;

    constexpr size_t us_offset(Piece pt){
    switch(pt){
        case P_wp: return us_pawn_offset;
        case P_wn: return us_knight_offset;
        case P_wb: return us_bishop_offset;
        case P_wr: return us_rook_offset;
        case P_wq: return us_queen_offset;
        case P_wk: return us_king_offset;
        default: return us_pawn_offset;
    }
    }

    constexpr size_t them_offset(Piece pt){
    switch(pt){
        case P_wp: return them_pawn_offset;
        case P_wn: return them_knight_offset;
        case P_wb: return them_bishop_offset;
        case P_wr: return them_rook_offset;
        case P_wq: return them_queen_offset;
        case P_wk: return them_king_offset;
        default: return them_pawn_offset;
    }
    }

} // feature_idx

#ifdef WITH_DATA2BIN
#include "learn/convert.hpp"
#endif

#endif // WITH_NNUE