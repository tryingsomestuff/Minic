#!/bin/bash

export CXX=aarch64-linux-gnu-g++
export CC=aarch64-linux-gnu-gcc

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomRPi64.sh "$@"
fi

do_title "Building Minic for RPi64"

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $STDVERSION $n -Wno-unknown-pragmas -DWITHOUT_FILESYSTEM"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64_armv8.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_linux_x64_armv8
echo "Building $exe"
echo $OPT

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $buildDir/$exe -static-libgcc -static-libstdc++ -lpthread
