#!/bin/bash

export CXX=$dir/android/bin/arm-linux-androideabi-clang++
export CC=$dir/android/bin/arm-linux-androideabi-clang++

source $(dirname $0)/common
cd_root

do_title "Building Fathom for Android"

cd $dir/Fathom/src

lib=fathom_${v}_android.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts -Wno-unused-function -Wno-deprecated $d -DNDEBUG -O3 -flto -I."
$CC -c $OPT tbprobe.c -o $lib

cd -
