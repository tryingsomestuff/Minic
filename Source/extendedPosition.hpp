#pragma once

#include "definition.hpp"

#if defined(WITH_TEST_SUITE) || defined(WITH_PGN_PARSER) || defined(WITH_TEXEL_TUNING)

#include "bitboardTools.hpp"
#include "position.hpp"

/* Those things are used for test suite to work when reading edp file
 */

struct ExtendedPosition : Position{
    ExtendedPosition(const std::string & s, bool withMoveCount = true);
    bool shallFindBest();
    bool shallAvoidBad();
    std::vector<std::string> bestMoves();
    std::vector<std::string> badMoves();
    std::vector<std::string> comment0();
    std::string id();

    static void test(const std::vector<std::string> & positions,
                     const std::vector<int> &         timeControls,
                     bool                             breakAtFirstSuccess,
                     bool                             multipleBest,
                     const std::vector<int> &         scores,
                     std::function< int(int) >        eloF,
                     bool                             withMoveCount = true);

    static void testStatic(const std::vector<std::string> & positions,
                           bool withMoveCount = false);

    std::map<std::string,std::vector<std::string> > _extendedParams;
};

std::string showAlgAbr(Move m, const Position & p);

#endif
