#pragma once

#include "material.hpp"

template<bool withRep, bool isPV, bool INR> inline MaterialHash::Terminaison Searcher::interiorNodeRecognizer(const Position& p) const {
   if (withRep && isRep(p, isPV)) return MaterialHash::Ter_Draw;
   if (p.fifty >= 100) return MaterialHash::Ter_Draw;
   if (INR && (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) < 2) return MaterialHash::probeMaterialHashTable(p.mat);
   return MaterialHash::Ter_Unknown;
}
