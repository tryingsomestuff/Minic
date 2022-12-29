#!/bin/bash

export CXX=aarch64-linux-gnu-g++
export CC=aarch64-linux-gnu-gcc

source $(dirname $0)/common
cd_root

do_title "Building Fathom for RPi64"

cd $dir/Fathom/src

lib=fathom_${v}_linux_x64_armv8.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto -I."
$CC -c -std=gnu99 $OPT tbprobe.c -o $lib -static-libgcc -static-libstdc++ 

cd -
