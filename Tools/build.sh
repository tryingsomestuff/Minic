#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

ulimit -s unlimited

cd $dir

export CXX=g++
export CC=gcc

#export CXX=clang++-10
#export CC=clang-10

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $dir/Tools/buildFathom.sh "$@"
fi

TINYDNN_PRESENT=0
if [ -e tiny-dnn/tiny-dnn/tiny-dnn.h ]; then
   TINYDNN_PRESENT=1
   echo "found tiny-dnn lib, trying to use it"
fi

mkdir -p $dir/Dist/Minic2

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"
n="-DUSE_AVX2"

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

$CXX -v
echo "version $v"
echo "definition $d"
echo "target $t"

exe=minic_${v}_linux_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe=${exe}_${tname}
fi

echo "Building $exe"

WARN="-Wall -Wcast-qual -Wno-char-subscripts -Wno-reorder -Wmaybe-uninitialized -Wuninitialized -pedantic -Wextra -Wshadow -Wfatal-errors -Wno-unknown-warning-option"

OPT="-s $WARN $d -DNDEBUG -O3 $t --std=c++17 $n" ; DEPTH=20
#OPT="-s $WARN $d -ffunction-sections -fdata-sections -Os -s -DNDEBUG -Wl,--gc-sections $t --std=c++17" ; DEPTH=20
#OPT="$WARN $d -DNDEBUG -O3 -g -ggdb -fno-omit-frame-pointer $t --std=c++17" ; DEPTH=20
#OPT="$WARN $d -DNDEBUG -g $t --std=c++17" ; DEPTH=10
#OPT="$WARN $d -g $t --std=c++17" ; DEPTH=10

LIBS="-lpthread"

if [ $TINYDNN_PRESENT = "1" ]; then
   OPT="$OPT -DCNN_USE_AVX2 -DCNN_USE_TBB -Itiny-dnn/ "
   LIBS="$LIBS -ltbb"
   DEPTH=15
else
   OPT="$OPT -fno-exceptions"
fi

# -flto is making g++ 9.3.0 under cygwin segfault
uname -a | grep CYGWIN
if [ $? == 1 ]; then
   OPT="$OPT -flto"
fi

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      lib=${lib}_${tname}
   fi
   lib=${lib}.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

echo $OPT $LIBS

rm -f *.gcda

$CXX -fprofile-generate $OPT Source/*.cpp -ISource -ISource/nnue -ISource/nnue/architectures -ISource/nnue/features -ISource/nnue/layers -o $dir/Dist/Minic2/$exe $LIBS 
echo "end of first compilation"
if [ $? = "0" ]; then
   echo "running Minic for profiling : $dir/Dist/Minic2/$exe"
   $dir/Dist/Minic2/$exe bench $DEPTH -quiet 0 
   echo "starting optimized compilation"
   $CXX -fprofile-use $OPT Source/*.cpp -ISource -ISource/nnue -ISource/nnue/architectures -ISource/nnue/features -ISource/nnue/layers -o $dir/Dist/Minic2/$exe $LIBS
   echo "done "
else
   echo "some error"
fi

rm -f *.gcda
