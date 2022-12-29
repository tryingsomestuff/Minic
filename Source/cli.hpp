#pragma once

#include "definition.hpp"

/*!
 * This is making some tools available from command line
 * perft, test suite, analysis, static evaluation, ...
 *
 * See help() for more detail
 */

bool cliManagement(const std::string & cli, int argc, char** argv);

bool bench(DepthType depth);
