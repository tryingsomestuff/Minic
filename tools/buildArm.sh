#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

d="-DWITH_UCI"
v="dev"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

android/bin/arm-linux-androideabi-clang++ -v
echo "version $v"
echo "definition $d"
exe=minic_${v}_android
echo "Building $exe"
OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++14 -IFathom/src"
android/bin/arm-linux-androideabi-clang++ $OPT minic.cc -o $dir/Dist/$exe 

