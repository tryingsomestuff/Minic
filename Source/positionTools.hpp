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
   PerftAccumulator(): pseudoNodes(0), validNodes(0), captureNodes(0), epNodes(0), checkNode(0), checkMateNode(0), castling(0), promotion(0) {}
   Counter           pseudoNodes, validNodes, captureNodes, epNodes, checkNode, checkMateNode, castling, promotion;
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