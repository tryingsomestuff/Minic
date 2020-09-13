#!/bin/bash
dir=$(readlink -f $(dirname $0)/)
v=$(cat Source/definition.hpp | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"

$dir/build.sh $v "-march=core2" "-DUSE_SSSE3 -DUSE_SSE2"
$dir/build.sh $v "-march=nehalem" "-DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
$dir/build.sh $v "-march=skylake" "-DUSE_AVX2 -DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"

$dir/buildGW.sh $v "-march=core2" "-DUSE_SSSE3 -DUSE_SSE2"
$dir/buildGW.sh $v "-march=nehalem" "-DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
$dir/buildGW.sh $v "-march=skylake" "-DUSE_AVX2 -DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"

$dir/buildGW32.sh $v "-march=pentium2" "-DUSE_MMX"
#$dir/buildGW32.sh $v "-march=nehalem" "-DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
#$dir/buildGW32.sh $v "-march=skylake" "-DUSE_AVX2 -DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"

$dir/buildArm.sh $v "-DUSE_NEON"
