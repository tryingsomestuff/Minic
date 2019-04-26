#!/bin/bash
dir=$(readlink -f $(dirname $0)/)
v=$(cat minic.cc | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"

$dir/build.sh $v "-march=x86-64"
$dir/build.sh $v "-msse4.2"
$dir/build.sh $v "-mavx2 -mbmi2"

$dir/buildGW.sh $v "-march=i686"
$dir/buildGW.sh $v "-msse4.2"
$dir/buildGW.sh $v "-mavx2 -mbmi2"
