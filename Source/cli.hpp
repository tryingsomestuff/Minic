#pragma once

#include "definition.hpp"

#include "position.hpp"

/* This header is making some tooks available from command line
 * perft, test suite, analysis, static evaluation, ...
 *
 * Available options are :
 *
 * -uci : starts UCI mode
 * -xboard : starts XBOARD move
 * -perft_test : run small perf test
 * -perft_test_long_fisher : run a long perf test for FRC
 * -perft_test_long : run a long perf test
 * -see_test : run a SEE test (position talen from Vajolet by Marco Belli a.k.a elcabesa)
 * bench : used for OpenBench ( by Andrew Grant)
 * -buildBook : convert ascii book to binary book
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

int cliManagement(std::string cli, int argc, char ** argv);
