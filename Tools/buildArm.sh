#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $dir/Tools/buildFathomArm.sh "$@"
fi

mkdir -p $dir/Dist/Minic2

v="dev"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++14 -IFathom/src"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_android.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

$dir/android/bin/arm-linux-androideabi-clang++ -v
echo "version $v"
exe=minic_${v}_android
echo "Building $exe"

$dir/android/bin/arm-linux-androideabi-clang++ $OPT Source/*.cpp -ISource -o $dir/Dist/Minic2/$exe -static-libgcc -static-libstdc++ 

