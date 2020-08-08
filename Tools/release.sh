#!/bin/bash
dir=$(readlink -f $(dirname $0)/)
v=$(cat Source/definition.hpp | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"

$dir/build.sh $v "-march=x86-64" "-DUSE_SSE3"
$dir/build.sh $v "-march=nehalem" "-DUSE_SSE42"
$dir/build.sh $v "-march=skylake" "-DUSE_AVX2"

$dir/buildGW.sh $v "-march=x86-64" "-DUSE_SSE3"
$dir/buildGW.sh $v "-march=nehalem" "-DUSE_SSE42"
$dir/buildGW.sh $v "-march=skylake" "-DUSE_AVX2"

$dir/buildGW32.sh $v "-march=i686" "-DUSE_SSE3"
#$dir/buildGW32.sh $v "-msse4.2" "-DUSE_SSE42"
#$dir/buildGW32.sh $v "-mavx2 -mbmi2" "-DUSE_AVX2"

$dir/buildArm.sh $v "-DUSE_NEON"
