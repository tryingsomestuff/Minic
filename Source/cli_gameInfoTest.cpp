#include "com.hpp"
#include "searcher.hpp"
#include "uci.hpp"
#include "xboard.hpp"

bool TestGameInfo() {
   int        errors = 0;
   const auto check  = [&](bool condition, const char* name) {
      if (!condition) {
         Logging::LogIt(Logging::logError) << "GameInfo test failed: " << name;
         ++errors;
      }
   };
   COM::init(COM::p_uci);
   auto& history  = COM::GetGameInfo();
   auto& searcher = ThreadPool::instance().main();
   UCI::processCommand("position startpos");
   const uint16_t firstPly = COM::position.halfmoves;
   const Position initial  = COM::position;
   check(history.getPosition(firstPly).has_value(), "initial position without moves");
   check(history.getHash(firstPly) == computeHash(initial), "initial hash without moves");
   check(history.getMove(firstPly) == INVALIDMOVE, "initial move without moves");
   check(!history.getSearchScore(firstPly), "unsearched initial position");
   check(!history.getPosition(firstPly - 1), "position before initial ply");
   history.setSearchScore(initial, 0);
   check(history.getSearchScore(firstPly) == 0, "zero is a valid score");

   UCI::processCommand("position startpos moves e2e4 e7e5");
   const Position secondRoot = COM::position;
   check(history.getSearchScore(firstPly) == 0, "initial score survives extension");
   check(!history.getSearchScore(firstPly + 1), "opponent position remains unscored");
   check(history.getPosition(firstPly + 2).has_value(), "latest position");
   check(history.getHash(firstPly + 2) == computeHash(secondRoot), "latest hash");
   check(history.getMove(firstPly + 2) == secondRoot.lastMove, "latest move");
   check(!history.getPosition(firstPly + 3), "position beyond history");
   history.setSearchScore(secondRoot, 79);
   UCI::processCommand("position startpos moves e2e4 e7e5 g1f3 b8c6");
   const uint16_t currentPly = COM::position.halfmoves;
   check(history.getSearchScore(firstPly + 2) == 79, "root score survives replay");
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "below margin");
   history.setSearchScore(secondRoot, 80);
   check(searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "positive margin");
   history.setSearchScore(secondRoot, -79);
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "negative below margin");
   history.setSearchScore(secondRoot, -80);
   check(!searcher.isBooming(currentPly) && searcher.isMoobing(currentPly), "negative margin");
   history.setSearchScore(secondRoot, 0);
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "stable score");
   history.setSearchScore(secondRoot, matingScore(0));
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "mate score excluded");
   history.setSearchScore(secondRoot, matedScore(0));
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "mated score excluded");
   history.setSearchScore(secondRoot, 120);
   history.setSearchScore(initial, matingScore(0));
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "former mate score excluded");
   history.setSearchScore(initial, 0);
   check(!searcher.isBooming(firstPly + 2) && !searcher.isMoobing(firstPly + 2), "missing former score");

   UCI::processCommand("position startpos moves e2e4 e7e5 g1f3 b8c6");
   check(history.getSearchScore(firstPly + 2) == 120, "identical replay");
   history.setSearchScore(COM::position, 150);
   check(history.getSearchScore(currentPly) == 150, "latest root score");
   check(XBoard::replay(2), "XBoard rewind");
   check(history.getSearchScore(firstPly + 2) == 120, "XBoard preserves prefix scores");
   check(!history.getSearchScore(currentPly), "XBoard discards future scores");
   UCI::processCommand("position startpos moves e2e4 e7e5 g1f3 b8c6");
   check(!history.getSearchScore(currentPly), "discarded scores do not return");

   UCI::processCommand("position startpos moves e2e4 c7c5");
   check(history.getSearchScore(firstPly) == 0, "branch preserves initial score");
   check(!history.getSearchScore(firstPly + 2), "branch discards divergent root score");
   UCI::processCommand("position startpos moves e2e4 e7e5 g1f3 b8c6");
   check(!history.getSearchScore(firstPly + 2), "old branch score stays discarded");
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "missing previous score");

   const std::string fen = GetFEN(COM::position);
   UCI::processCommand("position fen " + fen);
   check(!history.getSearchScore(firstPly), "different initial FEN clears scores");
   check(!searcher.isBooming(currentPly) && !searcher.isMoobing(currentPly), "FEN without history");
   history.setSearchScore(COM::position, -42);
   UCI::processCommand("position fen " + fen);
   check(history.getSearchScore(currentPly) == -42, "identical initial FEN preserves score");
   Position differentClock = COM::position;
   ++differentClock.fifty;
   history.setSearchScore(differentClock, 999);
   check(history.getSearchScore(currentPly) == -42, "mismatched clock rejected");
   UCI::processCommand("ucinewgame");
   UCI::processCommand("position fen " + fen);
   check(!history.getSearchScore(currentPly), "new game clears scores");

   Logging::LogIt(Logging::logInfo) << "GameInfo tests: " << errors << " errors";
   return errors == 0;
}
