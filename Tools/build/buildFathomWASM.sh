#!/bin/bash

source "/ssd/emsdk/emsdk_env.sh"

export CXX=emcc
export CC=emcc

source $(dirname $0)/common
cd_root

do_title "Building Fathom for WASM"

cd $dir/Fathom/src

lib=fathom_${v}_linux_x64_wasm.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto -I."
$CC -c $OPT tbprobe.c -o $lib 

cd -
