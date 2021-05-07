#!/bin/bash

source "/ssd/emsdk/emsdk_env.sh"

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

OPT="-Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n -Wno-unknown-pragmas -DWITHOUT_FILESYSTEM"
OPT="$OPT -s MODULARIZE=1 -s EXPORT_NAME="Minic" -s ENVIRONMENT=web,worker,node -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=1"
OPT="$OPT -s EXIT_RUNTIME=0 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']" --pre-js wasm/pre.js -s EXPORT_ALL=1"
OPT="$OPT -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=71303168 -s MAXIMUM_MEMORY=2147483648"
OPT="$OPT -s FILESYSTEM=0 --closure 1"
OPT="$OPT -s STRICT=0 -s ASSERTIONS=0"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64_wasm.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

exe=${e}_${v}_linux_x64_wasm.js
echo "Building $exe"
echo $OPT

STANDARDSOURCE="Source/*.cpp Source/nnue/learn/*.cpp"

$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $dir/Dist/Minic3/$exe -lpthread
