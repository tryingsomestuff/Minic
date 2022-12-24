#pragma once

#include "definition.hpp"
#include "position.hpp"

[[nodiscard]] std::string GetFENShort(const Position &p);

[[nodiscard]] std::string GetFENShort2(const Position &p);

[[nodiscard]] std::string GetFEN(const Position &p);

[[nodiscard]] std::string SanitizeCastling(const Position &p, const std::string &str);

[[nodiscard]] Square kingSquare(const Position &p);

///@todo std::optional to avoid output parameter
bool readMove(const Position &p, const std::string &ss, Square &from, Square &to, MType &moveType, bool forbidCastling = false);

[[nodiscard]] float gamePhase(const Position::Material &mat, ScoreType &matScoreW, ScoreType &matScoreB);

[[nodiscard]] bool readEPDFile(const std::string &fileName, std::vector<std::string> &positions);

struct PerftAccumulator {
   Counter pseudoNodes {0};
   Counter validNodes {0};
   Counter captureNodes {0};
   Counter epNodes {0};
   Counter checkNode {0};
   Counter checkMateNode {0};
   Counter castling {0};
   Counter promotion {0};
   void              Display() const;
   PerftAccumulator& operator+=(const PerftAccumulator& acc) {
      pseudoNodes += acc.pseudoNodes;
      validNodes += acc.validNodes;
      captureNodes += acc.captureNodes;
      epNodes += acc.epNodes;
      checkNode += acc.checkNode;
      checkMateNode += acc.checkMateNode;
      castling += acc.castling;
      promotion += acc.promotion;
      return *this;
   }
};

#if !defined(WITH_SMALL_MEMORY)
namespace chess960 {
extern const array1d<std::string,960> positions;
std::string getDFRCXFEN();
}
#endif