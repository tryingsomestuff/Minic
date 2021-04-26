#!/bin/bash
dir=$(readlink -f $(dirname $0)/)
v=$(cat Source/definition.hpp | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"

$dir/build.sh $v "-march=core2" 
$dir/build.sh $v "-march=nehalem" 
$dir/build.sh $v "-march=sandybridge"
#$dir/build.sh $v "-march=haswell"
$dir/build.sh $v "-march=skylake" 

$dir/buildGW.sh $v "-march=core2" 
$dir/buildGW.sh $v "-march=nehalem" 
$dir/buildGW.sh $v "-march=sandybridge"
#$dir/buildGW.sh $v "-march=haswell"
$dir/buildGW.sh $v "-march=skylake" 

$dir/buildGW32.sh $v "-march=pentium2" 

$dir/buildAndroid.sh $v 
$dir/buildRPi32.sh $v 
$dir/buildRPi64.sh $v 
