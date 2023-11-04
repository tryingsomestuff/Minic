#!/bin/bash

ANDROID_NDK_PATH=$(dirname $0)/../../../android/toolchains/llvm/prebuilt/linux-x86_64/bin/

export CXX=$ANDROID_NDK_PATH/armv7a-linux-androideabi34-clang++
export CC=$ANDROID_NDK_PATH/armv7a-linux-androideabi34-clang

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomAndroid.sh "$@"
fi

do_title "Building Minic for Android"

# -flto is leading to some bugs ...
OPT="-s -Wall -Wno-char-subscripts -Wno-reorder -Wno-missing-braces $d -DNDEBUG -O3 $STDVERSION $n -Wno-unknown-pragmas -fconstexpr-steps=1000000000"
#OPT="-s -Wall -Wno-char-subscripts -Wno-reorder -Wno-missing-braces $d -ggdb3 $STDVERSION $n -Wno-unknown-pragmas -fconstexpr-steps=1000000000"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_android.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_android
echo "Building $exe"
echo $OPT

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $buildDir/$exe -static -static-libgcc -static-libstdc++ 
