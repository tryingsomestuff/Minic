#!/bin/bash

ANDROID_NDK_PATH=/data/vivien/SRC/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/

export CXX=$ANDROID_NDK_PATH/armv7a-linux-androideabi34-clang++
export CC=$ANDROID_NDK_PATH/armv7a-linux-androideabi34-clang

source $(dirname $0)/common
cd_root

do_title "Building Fathom for Android"

cd $dir/Fathom/src

lib=fathom_${v}_android.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts -Wno-unused-function -Wno-deprecated $d -DNDEBUG -O3 -flto -I."
$CC -c -std=gnu99 $OPT tbprobe.c -o $lib

cd -
