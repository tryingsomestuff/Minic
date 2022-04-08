#pragma once

#include "material.hpp"

template<bool withRep, bool isPV, bool INR> 
[[nodiscard]] inline MaterialHash::Terminaison Searcher::interiorNodeRecognizer(const Position& p) const{
   if ( (p.occupancy() & ~p.allKing()) == emptyBitBoard)  return MaterialHash::Ter_Draw;
   if (withRep && isRep(p, isPV)) return MaterialHash::Ter_Draw;
   //if (p.fifty >= 100) return MaterialHash::Ter_Draw; // cannot check 50MR here because it might be reset by checkmate
   if (INR && (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) < 2) return MaterialHash::probeMaterialHashTable(p.mat);
   return MaterialHash::Ter_Unknown;
}
