#pragma once

#include "definition.hpp"

/* Some classic test suites available here
 *
 * help_test : display available test suite
 * MEA
 * opening200
 * opening1000
 * middle200
 * middle1000
 * hard2020
 * BT2630
 * WAC
 * arasan
 * arasan_sym
 * CCROHT
 * BKTest
 * EETest
 * KMTest
 * LCTest
 * sbdTest
 * STS
 * ERET
 * MATE
 *
 */

#ifdef WITH_TEST_SUITE

#include "extendedPosition.hpp"

bool test(const std::string & option);

#endif
