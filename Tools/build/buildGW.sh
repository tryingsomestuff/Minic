#!/bin/bash

export CXX=x86_64-w64-mingw32-g++
export CC=x86_64-w64-mingw32-gcc

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomGW.sh "$@"
fi

do_title "Building Minic for Win64"

exe=${e}_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe=${exe}_${tname}
fi
exe=${exe}.exe

echo "Building $exe"
OPT="-Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $t --std=c++17 $n -Wno-unknown-pragmas"
echo $OPT

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_mingw_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      lib=${lib}_${tname}
   fi
   lib=${lib}.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -static -static-libgcc -static-libstdc++ -o $buildDir/$exe -Wl,-Bstatic -lpthread
x86_64-w64-mingw32-strip $buildDir/$exe
