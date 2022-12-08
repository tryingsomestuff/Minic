#!/bin/bash

export CXX=/ssd2/android/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi33-clang++
export CC=/ssd2/android/toolchains/llvm/prebuilt/linux-x86_64/bin//armv7a-linux-androideabi33-clang++

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomAndroid.sh "$@"
fi

do_title "Building Minic for Android"

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $STDVERSION $n -Wno-unknown-pragmas -fconstexpr-steps=1000000000"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_android.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_android
echo "Building $exe"
echo $OPT

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $buildDir/$exe -static-libgcc -static-libstdc++ 
