#pragma once

#include "definition.hpp"
#include "logging.hpp"

struct Position;

#ifdef DEBUG_EVALSYM
[[nodiscard]] FORCE_FINLINE float fiftyMoveRuleScaling(const uint8_t ){
   return 1;
}
[[nodiscard]] FORCE_FINLINE float fiftyMoveRuleUnScaling(const uint8_t ){
   return 1;
}
#else
[[nodiscard]] constexpr float fiftyMoveRuleScaling(const uint8_t fifty){
   return 1.f - std::max(0, fifty*fifty) / 10000.f;
}
#endif

[[nodiscard]] FORCE_FINLINE ScoreType fiftyScale(const ScoreType s, uint8_t fifty){
   return static_cast<ScoreType>(s * fiftyMoveRuleScaling(fifty));
}

///@todo use that to store TT entry independant of fifty scaling
[[nodiscard]] FORCE_FINLINE ScoreType fiftyUnScale(const ScoreType s, uint8_t fifty){
   return static_cast<ScoreType>(s / fiftyMoveRuleScaling(fifty));
}

// Stockfish trick (two short in one int) is not compatible with evaluation tuning !
#ifndef WITH_EVALSCORE_AS_INT
struct EvalScore {
   array1d<ScoreType, GP_MAX> sc = {0};
   EvalScore(const ScoreType mg, const ScoreType eg): sc {mg, eg} {}
   EvalScore(const ScoreType s): sc {s, s} {}
   EvalScore(): sc {0, 0} {}
   EvalScore(const EvalScore& e): sc {e.sc[MG], e.sc[EG]} {}
   ~EvalScore() = default;

   FORCE_FINLINE ScoreType&       operator[](const GamePhase g) { return sc[g]; }
   FORCE_FINLINE const ScoreType& operator[](const GamePhase g) const { return sc[g]; }

   EvalScore& operator*=(const EvalScore& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] *= s[g];
      return *this;
   }
   EvalScore& operator/=(const EvalScore& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] /= s[g];
      return *this;
   }
   EvalScore& operator+=(const EvalScore& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] += s[g];
      return *this;
   }
   EvalScore& operator-=(const EvalScore& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] -= s[g];
      return *this;
   }
   EvalScore operator*(const EvalScore& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] *= s[g];
      return e;
   }
   EvalScore operator/(const EvalScore& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] /= s[g];
      return e;
   }
   EvalScore operator+(const EvalScore& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] += s[g];
      return e;
   }
   EvalScore operator-(const EvalScore& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] -= s[g];
      return e;
   }
   EvalScore & operator=(const EvalScore& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) { sc[g] = s[g]; }
      return *this;
   }

   EvalScore& operator*=(const ScoreType& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] *= s;
      return *this;
   }
   EvalScore& operator/=(const ScoreType& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] /= s;
      return *this;
   }
   EvalScore& operator+=(const ScoreType& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] += s;
      return *this;
   }
   EvalScore& operator-=(const ScoreType& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) sc[g] -= s;
      return *this;
   }
   EvalScore operator*(const ScoreType& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] *= s;
      return e;
   }
   EvalScore operator/(const ScoreType& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] /= s;
      return e;
   }
   EvalScore operator+(const ScoreType& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] += s;
      return e;
   }
   EvalScore operator-(const ScoreType& s) const {
      EvalScore e(*this);
      for (GamePhase g = MG; g < GP_MAX; ++g) e[g] -= s;
      return e;
   }
   EvalScore & operator=(const ScoreType& s) {
      for (GamePhase g = MG; g < GP_MAX; ++g) { sc[g] = s; }
      return *this;
   }

   [[nodiscard]] EvalScore scale(float s_mg, float s_eg) const {
      EvalScore e(*this);
      e[MG] = ScoreType(s_mg * e[MG]);
      e[EG] = ScoreType(s_eg * e[EG]);
      return e;
   }
};

#else

constexpr auto MakeScore(mg, eg) { return (int)((unsigned int)(eg) << 16) + (mg);}
constexpr auto MGScore(s)        { return (int16_t)((uint16_t)((unsigned)((s))));}
constexpr auto EGScore(s)        { return (int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16));}

struct EvalScore {
   int sc = 0;
   EvalScore(const ScoreType mg, const ScoreType eg): sc {MakeScore(mg, eg)} {}
   EvalScore(const ScoreType s): sc {MakeScore(s, s)} {}
   EvalScore(): sc {0} {}
   EvalScore(const EvalScore& s): sc {s.sc} {}
   ~EvalScore() = default;

   FORCE_FINLINE ScoreType operator[](GamePhase g) const { return g == MG ? MGScore(sc) : EGScore(sc); }

   EvalScore& operator*=(const EvalScore& s) {
      sc = MakeScore(MGScore(sc) * MGScore(s.sc), EGScore(sc) * EGScore(s.sc));
      return *this;
   }
   EvalScore& operator/=(const EvalScore& s) {
      sc = MakeScore(MGScore(sc) / MGScore(s.sc), EGScore(sc) / EGScore(s.sc));
      return *this;
   }
   EvalScore& operator+=(const EvalScore& s) {
      sc = MakeScore(MGScore(sc) + MGScore(s.sc), EGScore(sc) + EGScore(s.sc));
      return *this;
   }
   EvalScore& operator-=(const EvalScore& s) {
      sc = MakeScore(MGScore(sc) - MGScore(s.sc), EGScore(sc) - EGScore(s.sc));
      return *this;
   }
   EvalScore operator*(const EvalScore& s) const { return MakeScore(MGScore(sc) * MGScore(s.sc), EGScore(sc) * EGScore(s.sc)); }
   EvalScore operator/(const EvalScore& s) const { return MakeScore(MGScore(sc) / MGScore(s.sc), EGScore(sc) / EGScore(s.sc)); }
   EvalScore operator+(const EvalScore& s) const { return MakeScore(MGScore(sc) + MGScore(s.sc), EGScore(sc) + EGScore(s.sc)); }
   EvalScore operator-(const EvalScore& s) const { return MakeScore(MGScore(sc) - MGScore(s.sc), EGScore(sc) - EGScore(s.sc)); }
   EvalScore& operator=(const EvalScore& s) { sc = s.sc; return *this; }

   EvalScore& operator*=(const ScoreType& s) {
      sc = MakeScore(MGScore(sc) * s, EGScore(sc) * s);
      return *this;
   }
   EvalScore& operator/=(const ScoreType& s) {
      sc = MakeScore(MGScore(sc) / s, EGScore(sc) / s);
      return *this;
   }
   EvalScore& operator+=(const ScoreType& s) {
      sc = MakeScore(MGScore(sc) + s, EGScore(sc) + s);
      return *this;
   }
   EvalScore& operator-=(const ScoreType& s) {
      sc = MakeScore(MGScore(sc) - s, EGScore(sc) - s);
      return *this;
   }
   EvalScore operator*(const ScoreType& s) const { return MakeScore(MGScore(sc) * s, EGScore(sc) * s); }
   EvalScore operator/(const ScoreType& s) const { return MakeScore(MGScore(sc) / s, EGScore(sc) / s); }
   EvalScore operator+(const ScoreType& s) const { return MakeScore(MGScore(sc) + s, EGScore(sc) + s); }
   EvalScore operator-(const ScoreType& s) const { return MakeScore(MGScore(sc) - s, EGScore(sc) - s); }
   EvalScore& operator=(const ScoreType& s) { sc = MakeScore(s, s); return *this; }

   [[nodiscard]] EvalScore scale(float s_mg, float s_eg) const { return EvalScore(ScoreType(MGScore(sc) * s_mg), ScoreType(EGScore(sc) * s_eg)); }
};
#endif

inline std::ostream& operator<<(std::ostream& of, const EvalScore& s) {
   of << s[MG] << " " << s[EG];
   return of;
}

enum Feature : uint8_t { F_material = 0, F_positional, F_development, F_mobility, F_attack, F_pawnStruct, F_complexity, F_max };
FORCE_FINLINE Feature operator++(Feature& f) {
   f = Feature(f + 1);
   return f;
}
struct EvalFeatures {
   array1d<EvalScore, F_max> scores; // EvalScore is 0 initialized
   float       scalingFactor = 1.f; // only used in end-game !
   EvalScore   SumUp() const;
   static void callBack();
};

/*
inline std::ostream & operator<<(std::ostream & of, const EvalFeatures & features){
    for (size_t k = 0 ; k < F_max; ++k) of << features.scores[k] << " ";
    of << features.scalingFactor << " ";
    return of;
}
*/

[[nodiscard]] FORCE_FINLINE ScoreType ScaleScore(const EvalScore s, const float gp, const float scalingFactorEG = 1.f) {
   return static_cast<ScoreType>(gp * s[MG] + (1.f - gp) * scalingFactorEG * s[EG]);
}

/* Evaluation is returning the score of course, but also fill this little structure to provide
 * additionnal usefull information, such as game phase and current danger. Things that are
 * possibly used in search later.
 */
struct EvalData {
   float gp = 0;
   colored<ScoreType> danger   = {0, 0};
   colored<uint16_t>  mobility = {0, 0};
   colored<bool>      haveThreats = {false, false};
   colored<bool>      goodThreats = {false, false};
   bool evalDone = false; // will tell if phase, danger and mobility are filleds or not
};

// used for easy move detection
struct RootScores {
   Move      m;
   ScoreType s;
};

// used for multiPV stuff
struct MultiPVScores {
   Move      m;
   ScoreType s;
   PVList    pv;
   DepthType seldepth;
};

void displayEval(const EvalData& data, const EvalFeatures& features);

// idea from Stockfish
namespace WDL {
// Coefficients of a 3rd order polynomial fit based on Minic test data (from CCRL, CEGT, FGRL)
constexpr array1d<double, 4> as {-13.65744616, 94.04894005, -95.05180396, 84.853482690};
constexpr array1d<double, 4> bs {-10.78187987, 77.22626799, -132.72201029, 122.54185402};
} // namespace WDL

[[nodiscard]] FORCE_FINLINE ScoreType shiftArmageddon(const ScoreType v, const unsigned int ply, const Color c) {
   // limit input ply and rescale
   const double m = std::min(256u, ply) / 64.0; // care! here ply not move
   const double a = (((WDL::as[0] * m + WDL::as[1]) * m + WDL::as[2]) * m) + WDL::as[3];
   if (c == Co_White) return static_cast<ScoreType>(v - 2 * a);
   else
      return static_cast<ScoreType>(v + 2 * a);
}

[[nodiscard]] FORCE_FINLINE double toWDLModel(const ScoreType v, const unsigned int ply) {
   // limit input ply and rescale
   const double m = std::min(256u, ply) / 64.0; // care! here ply not move
   const double a = (((WDL::as[0] * m + WDL::as[1]) * m + WDL::as[2]) * m) + WDL::as[3];
   const double b = (((WDL::bs[0] * m + WDL::bs[1]) * m + WDL::bs[2]) * m) + WDL::bs[3];
   // clamp score
   const double x = std::clamp(double((a - v) / b), -600.0, 600.0);
   // win probability from 0 to 1000
   return 1000. / (1. + std::exp(x));
}

/*
FORCE_FINLINE ScoreType fromWDLModel(const double w, const unsigned int ply) {
    // limit input ply and rescale
    const double m = std::min(256u, ply) / 32.0;
    const double a = (((WDL::as[0] * m + WDL::as[1]) * m + WDL::as[2]) * m) + WDL::as[3];
    const double b = (((WDL::bs[0] * m + WDL::bs[1]) * m + WDL::bs[2]) * m) + WDL::bs[3];
    const double s = a - b * std::log(1000./(std::max(w,std::numeric_limits<double>::epsilon())) - 1. + std::numeric_limits<double>::epsilon());
    return static_cast<ScoreType>(std::clamp(s, static_cast<double>matedScore(ply) , static_cast<double>(matingScore(ply - 1)) );
}
*/