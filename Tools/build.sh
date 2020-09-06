#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

ulimit -s unlimited

cd $dir

export CXX=g++
export CC=gcc

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

avx512support=$(grep -c avx512 /proc/cpuinfo)
avx2support=$(grep -c avx2 /proc/cpuinfo)
if (test $avx512support -ne 0); then
   n="-DUSE_AVX512 -DUSE_AVX2 -DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
elif (test $avx2support -ne 0); then
   n="-DUSE_AVX2 -DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
else
   n="-DUSE_SSE41 -DUSE_SSSE3 -DUSE_SSE2"
fi

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

WARN="-Wall -Wcast-qual -Wno-char-subscripts -Wno-reorder -Wmaybe-uninitialized -Wuninitialized -pedantic -Wextra -Wshadow -Wno-unknown-warning-option"

OPT="-s $WARN $d -DNDEBUG -O3 $t --std=c++17 $n" ; DEPTH=16
#OPT="-s $WARN $d -ffunction-sections -fdata-sections -Os -s -DNDEBUG -Wl,--gc-sections $t --std=c++17" ; DEPTH=16
#OPT="$WARN $d -DNDEBUG -O3 -g -ggdb -fno-omit-frame-pointer $t --std=c++17" ; DEPTH=16
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

NNUESOURCE="Source/nnue/features/half_kp.cpp Source/nnue/features/half_relative_kp.cpp Source/nnue/evaluate_nnue.cpp Source/nnue/learn/learn_tools.cpp Source/nnue/learn/convert.cpp Source/nnue/learn/multi_think.cpp Source/nnue/learn/learner.cpp Source/nnue/evaluate_nnue_learner.cpp" 

$CXX -fprofile-generate $OPT Source/*.cpp $NNUESOURCE -ISource -ISource/nnue -ISource/nnue/architectures -ISource/nnue/features -ISource/nnue/layers -o $dir/Dist/Minic2/$exe $LIBS 
ret=$?
echo "end of first compilation"
if [ $ret = "0" ]; then
   echo "running Minic for profiling : $dir/Dist/Minic2/$exe"
   $dir/Dist/Minic2/$exe bench $DEPTH -quiet 0 
   #$dir/Dist/Minic2/$exe bench $DEPTH -quiet 0 -NNUEFile Tourney/nn.bin
   echo "starting optimized compilation"
   $CXX -fprofile-use $OPT Source/*.cpp $NNUESOURCE -ISource -ISource/nnue -ISource/nnue/architectures -ISource/nnue/features -ISource/nnue/layers -o $dir/Dist/Minic2/$exe $LIBS
   echo "done "
else
   echo "some error"
fi

rm -f *.gcda
