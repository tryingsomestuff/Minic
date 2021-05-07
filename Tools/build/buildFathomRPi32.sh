#!/bin/bash

export CXX=arm-linux-gnueabihf-g++
export CC=arm-linux-gnueabihf-gcc

source $(dirname $0)/common
cd_root

do_title "Building Fathom for RPi32"

cd $dir/Fathom/src

lib=fathom_${v}_linux_x32_armv7.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto -I."
$CC -c $OPT tbprobe.c -o $lib -static-libgcc -static-libstdc++ 

cd -
