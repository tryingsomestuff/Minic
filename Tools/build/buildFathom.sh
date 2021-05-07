#!/bin/bash

export CXX=g++
export CC=gcc

source $(dirname $0)/common
cd_root

do_title "Building Fathom"

cd $dir/Fathom/src

lib=fathom_${v}_linux_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   lib=${lib}_${tname}
fi
lib=${lib}.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto $t -I."
$CC -c $OPT tbprobe.c -o $lib

cd -
