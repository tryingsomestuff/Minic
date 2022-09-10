#pragma once

#include "definition.hpp"

/*!
 * This is making some tools available from command line
 * perft, test suite, analysis, static evaluation, ...
 *
 * Available options are :
 *
 * -uci : starts UCI mode
 * -xboard : starts XBOARD move
 * -perft_test : run small perf test
 * -perft_test_long_fischer : run a long perf test for FRC
 * -perft_test_long : run a long perf test
 * -see_test : run a SEE test (most positions taken from Vajolet by Marco Belli a.k.a elcabesa)
 * bench : used for OpenBench ( by Andrew Grant)
 * -qsearch : run a qsearch
 * -see : run a SEE
 * -attacked :
 * -cov :
 * -eval : run an evaluation
 * -gen : generate available moves
 * -testmove
 * -perft : run a perft on the given position for the given depth
 * -analyze : run an analysis for the given position to the given depth
 * -mateFinder : run an analysis in mate finder mode for the given position to the given depth
 *
 * and all test suite ...
 *
 */

int cliManagement(const std::string & cli, int argc, char** argv);

int bench(DepthType depth);
