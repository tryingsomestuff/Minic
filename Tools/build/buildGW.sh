#!/bin/bash
dir=$(readlink -f $(dirname $0)/../..)
cd $dir

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathomGW.sh "$@"
fi

mkdir -p $dir/Dist/Minic3

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"
n="-fopenmp-simd"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

if [ -n "$1" ] ; then
   t=$1
   shift
fi

if [ -n "$1" ] ; then
   n=$1
   shift
fi

x86_64-w64-mingw32-g++ -v
echo "version $v"
echo "definition $d"
echo "target $t"

exe=minic_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe=${exe}_${tname}
fi
exe=${exe}.exe

echo "Building $exe"
OPT="-Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $t --std=c++17 $n -Wno-unknown-pragmas"
echo $OPT

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_mingw_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      lib=${lib}_${tname}
   fi
   lib=${lib}.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

STANDARDSOURCE="Source/*.cpp Source/nnue/learn/*.cpp"

x86_64-w64-mingw32-g++ $OPT $STANDARDSOURCE -ISource -ISource/nnue -static -static-libgcc -static-libstdc++ -o $dir/Dist/Minic3/$exe -Wl,-Bstatic -lpthread
x86_64-w64-mingw32-strip $dir/Dist/Minic3/$exe

