#!/bin/bash

export CXX=x86_64-w64-mingw32-g++
export CC=x86_64-w64-mingw32-gcc

source $(dirname $0)/common

cd $dir/Fathom/src

lib=fathom_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   lib=${lib}_${tname}
fi
lib=${lib}.o
echo "Building $lib"

OPT="-s -Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto $t -I."
$CC -c $OPT tbprobe.c -o $lib

cd -
