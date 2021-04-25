#!/bin/bash
dir=$(readlink -f $(dirname $0)/../..)
cd $dir

export CXX=arm-linux-gnueabihf-g++
export CC=arm-linux-gnueabihf-gcc

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomRPi32.sh "$@"
fi

mkdir -p $dir/Dist/Minic3

v="dev"
n=""

if [ -n "$1" ] ; then
   v=$1
   shift
fi

if [ -n "$1" ] ; then
   n=$1
   shift
fi

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n -Wno-unknown-pragmas -DWITHOUT_FILESYSTEM"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x32_armv7.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

$CXX -v
echo "version $v"
exe=minic_${v}_linux_x32_armv7
echo "Building $exe"
echo $OPT

STANDARDSOURCE="Source/*.cpp Source/nnue/learn/*.cpp"

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $dir/Dist/Minic3/$exe -static-libgcc -static-libstdc++ -lpthread

