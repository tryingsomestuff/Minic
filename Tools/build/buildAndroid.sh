#!/bin/bash

export CXX=/ssd/android/bin/arm-linux-androideabi-clang++
export CC=/ssd/android/bin/arm-linux-androideabi-clang++

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomAndroid.sh "$@"
fi

do_title "Building Minic for Android"

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n -Wno-unknown-pragmas"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_android.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_android
echo "Building $exe"
echo $OPT

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $buildDir/$exe -static-libgcc -static-libstdc++ 
