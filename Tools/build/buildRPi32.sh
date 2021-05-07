#!/bin/bash

export CXX=arm-linux-gnueabihf-g++
export CC=arm-linux-gnueabihf-gcc

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomRPi32.sh "$@"
fi

do_title "Building Minic for RPi32"

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n -Wno-unknown-pragmas -DWITHOUT_FILESYSTEM"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x32_armv7.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_linux_x32_armv7
echo "Building $exe"
echo $OPT

STANDARDSOURCE="Source/*.cpp Source/nnue/learn/*.cpp"

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $dir/Dist/Minic3/$exe -static-libgcc -static-libstdc++ -lpthread
