#!/bin/bash

source "/ssd/emsdk/emsdk_env.sh"

# https://emscripten.org/docs/tools_reference/emcc.html
export CXX=emcc
export CC=emcc

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomWASM.sh "$@"
fi

do_title "Building Minic for WASM"

OPT="-fconstexpr-steps=100000000 -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n -Wno-unknown-pragmas -DWITHOUT_FILESYSTEM -fconstexpr-depth=1000000"
OPT="$OPT -s MODULARIZE=1 -s EXPORT_NAME="Minic" -s ENVIRONMENT=web,worker"
OPT="$OPT -s USE_PTHREADS=1"
#OPT="$OPT -s PTHREAD_POOL_SIZE=1"
OPT="$OPT -s PROXY_TO_PTHREAD"
OPT="$OPT --pre-js Tools/build/wasm/pre.js"
OPT="$OPT -s NO_EXIT_RUNTIME=1"
OPT="$OPT -s EXPORTED_RUNTIME_METHODS=['ccall']"
#OPT="$OPT -s EXPORT_ALL=1"
OPT="$OPT -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=71303168 -s MAXIMUM_MEMORY=2147483648"
#OPT="$OPT -s FILESYSTEM=0 --closure 0 --minify 0"
#OPT="$OPT -s STRICT=0 -s ASSERTIONS=0 -s ALLOW_UNIMPLEMENTED_SYSCALLS"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64_wasm.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_linux_x64_wasm.js
echo "Building $exe"
echo $OPT

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $buildDir/$exe -lpthread
