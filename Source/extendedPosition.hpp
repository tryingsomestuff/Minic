#pragma once

#include "definition.hpp"

#if defined(WITH_TEST_SUITE) || defined(WITH_PGN_PARSER) || defined(WITH_EVAL_TUNING)

#include "bitboardTools.hpp"
#include "position.hpp"

/*!
 * Those things are used for test suite to work when reading edp file
 */
struct ExtendedPosition : RootPosition {
   ExtendedPosition(const std::string& s, bool withMoveCount = true);
   [[nodiscard]] bool       shallFindBest();
   [[nodiscard]] bool       shallAvoidBad();
   [[nodiscard]] std::vector<std::string> bestMoves();
   [[nodiscard]] std::vector<std::string> badMoves();
   [[nodiscard]] std::vector<std::string> comment0();
   [[nodiscard]] std::string              id();

   static void test(const std::vector<std::string>& positions,
                    const std::vector<int>&         timeControls,
                    bool                            breakAtFirstSuccess,
                    bool                            multipleBest,
                    const std::vector<int>&         scores,
                    std::function<int(int)>         eloF,
                    bool                            withMoveCount = true);

   static void testStatic(const std::vector<std::string>& positions, bool withMoveCount = false);

   std::map<std::string, std::vector<std::string>> _extendedParams;

   [[nodiscard]] std::string epdString() const;
};

#endif
