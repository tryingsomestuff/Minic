#include "skill.hpp"

#include "tools.hpp"

Move Skill::pick(std::vector<RootScores>& multiPVMoves) {
   const size_t    multiPV    = multiPVMoves.size();
   const ScoreType topScore   = multiPVMoves[0].s;
   const ScoreType worstScore = multiPVMoves[multiPV - 1].s;
   const ScoreType delta      = std::min(ScoreType(topScore - worstScore), absValue(P_wp));
   const ScoreType weakness   = 128 - DynamicConfig::level; // so from 28 (minimum) to 128 (maximum)
   ScoreType       maxScore   = -MATE;

   Logging::LogIt(Logging::logInfo) << "Picking suboptimal move from " << multiPV << " best move(s)";

   // shuffling of available moves thanks to both a deterministic value and a random one
   Move m = INVALIDMOVE;
   for (size_t i = 0; i < multiPV; ++i) {
      if (!isValidMove(multiPVMoves[i].m)) continue;
      const ScoreType currentMoveScore = multiPVMoves[i].s;
      // magic formula from Stockfish
      const ScoreType add = (weakness * (topScore - currentMoveScore) + randomInt<int, 1234>(0, weakness) * delta) / 128;
      Logging::LogIt(Logging::logInfo) << ToString(multiPVMoves[i].m) << " " << currentMoveScore << " " << add << " " << maxScore;
      const ScoreType score = currentMoveScore + (isMateScore(currentMoveScore) ? 0 : add);
      if (score >= maxScore) {
         maxScore = score;
         m        = multiPVMoves[i].m;
      }
   }
   Logging::LogIt(Logging::logInfo) << "Picked move " << ToString(m) << " " << maxScore;
   return m;
}