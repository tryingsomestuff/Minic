#include "com.hpp"
#include "moveGen.hpp"
#include "moveSort.hpp"
#include "searcher.hpp"
#include "uci.hpp"

bool TestPawnHistory() {
   int errors = 0;
   const auto check = [&](const bool condition, const char* name) {
      if (!condition) {
         Logging::LogIt(Logging::logError) << "Pawn history test failed: " << name;
         ++errors;
      }
   };

   constexpr array1d<int, 17> bonuses = {
       std::numeric_limits<int>::min(), -65536, -16384, -4096, -1024, -64, -4, -1,
       0, 1, 4, 64, 1024, 4096, 16384, 65536, std::numeric_limits<int>::max()};
   bool bounded = true;
   bool monotonic = true;
   bool zeroUnchanged = true;
   for (int item = -HISTORY_MAX; item <= HISTORY_MAX; ++item) {
      for (const int bonus : bonuses) {
         const ScoreType updated = HISTORY_UPDATE(static_cast<ScoreType>(item), bonus);
         bounded &= updated >= -HISTORY_MAX && updated <= HISTORY_MAX;
         monotonic &= bonus > 0 ? updated >= item : bonus < 0 ? updated <= item : true;
         if (bonus == 0) zeroUnchanged &= updated == item;
      }
   }
   check(bounded, "gravity is bounded for all initial entries and large signed bonuses");
   check(monotonic, "gravity moves toward the bonus target without overshooting");
   check(zeroUnchanged, "zero bonus preserves history");
   check(HSCORE(4) == 64 && HSCORE(16) == HISTORY_MAX && HSCORE(MAX_DEPTH) == HISTORY_MAX,
         "depth bonus preserves factor four and saturates at depth sixteen");
   check(HISTORY_UPDATE(100, 64) == 100 + 64 - HISTORY_DIV(100 * 64),
         "unscaled shallow updates preserve the original gravity");

   COM::init(COM::p_uci);
   UCI::processCommand("position startpos");
   const Position& p = COM::position;

   MoveList moves;
   MoveGen::generate<MoveGen::GP_quiet>(p, moves);
   const auto moveIt = std::ranges::find_if(moves, [](const Move m) {
      return Move2From(m) == Sq_e2 && Move2To(m) == Sq_e3;
   });
   check(moveIt != moves.end(), "find legal quiet move");
   if (moveIt == moves.end()) return false;

   Searcher& searcher = ThreadPool::instance().main();
   HistoryT& history = searcher.historyT;
   history.initHistory();
   const HistoryT& constHistory = history;
   check(&history.getFromToHistory(Co_White, Sq_e2, Sq_e3) == &history.history[Co_White][Sq_e2][Sq_e3] &&
         &constHistory.getFromToHistory(Co_Black, Sq_e7, Sq_e6) == &history.history[Co_Black][Sq_e7][Sq_e6],
         "from/to accessors reference the matching color and squares");
   check(&history.getPieceToHistory(P_wp, Sq_e3) == &history.historyP[PieceIdx(P_wp)][Sq_e3] &&
         &constHistory.getPieceToHistory(P_bp, Sq_e6) == &history.historyP[PieceIdx(P_bp)][Sq_e6],
         "piece/to accessors preserve piece color");
   check(&history.getCaptureHistory(P_wn, Sq_e4, P_bp) == &history.historyCap[PieceIdx(P_wn)][Sq_e4][0] &&
         &constHistory.getCaptureHistory(P_bn, Sq_e5, P_wq) == &history.historyCap[PieceIdx(P_bn)][Sq_e5][4] &&
         &constHistory.getCaptureHistory(P_bn, Sq_e5, P_bq) == &history.historyCap[PieceIdx(P_bn)][Sq_e5][4],
         "capture accessors preserve mover color and ignore victim color");
   searcher.killerT.initKillers();
   searcher.counterT.initCounter();
   searcher.previousBest = INVALIDMOVE;
   CMHPtrArray cmhPtr{};
   const Piece piece = p.board_const(Move2From(*moveIt));
   const Square to = Move2To(*moveIt);

   check(history.getPawnHistory(p, piece, to) == 0, "initial bucket is zero");
   for (int i = 0; i < 128; ++i) history.update<1>(32, *moveIt, p, cmhPtr);
   const ScoreType positive = history.getPawnHistory(p, piece, to);
   check(positive > 0, "positive quiet update is learned");
   check(positive <= HISTORY_MAX, "positive value is bounded");

   for (int i = 0; i < 128; ++i) history.update<-1>(32, *moveIt, p, cmhPtr);
   const ScoreType negative = history.getPawnHistory(p, piece, to);
   check(negative < 0, "negative quiet updates reverse the score");
   check(negative >= -HISTORY_MAX, "negative value is bounded");

   history.initHistory();
   check(history.getPawnHistory(p, piece, to) == 0, "history reset clears pawn history");

   MoveList baselineMoves;
   MoveGen::generate<MoveGen::GP_quiet>(p, baselineMoves);
   MoveSorter::score(searcher, baselineMoves, p, 0.f, 1, cmhPtr, false, false);
   const auto baselineIt = std::ranges::find_if(baselineMoves, [&](const Move m) {
      return sameMove(m, *moveIt);
   });
   check(baselineIt != baselineMoves.end(), "find scored quiet move");
   if (baselineIt == baselineMoves.end()) return false;
   const ScoreType baselineScore = Move2Score(*baselineIt);

   history.getPawnHistory(p, piece, to) = HISTORY_MAX;
   MoveList pawnHistoryMoves;
   MoveGen::generate<MoveGen::GP_quiet>(p, pawnHistoryMoves);
   MoveSorter::score(searcher, pawnHistoryMoves, p, 0.f, 1, cmhPtr, false, false);
   const auto pawnHistoryIt = std::ranges::find_if(pawnHistoryMoves, [&](const Move m) {
      return sameMove(m, *moveIt);
   });
   check(pawnHistoryIt != pawnHistoryMoves.end(), "find rescored quiet move");
   if (pawnHistoryIt == pawnHistoryMoves.end()) return false;
   check(Move2Score(*pawnHistoryIt) - baselineScore == HISTORY_MAX / SearchConfig::pawnHistoryDivisor,
         "pawn history contributes to quiet ordering");

   history.initHistory();
   Logging::LogIt(Logging::logInfo) << "Pawn history tests: " << errors << " errors";
   return errors == 0;
}
