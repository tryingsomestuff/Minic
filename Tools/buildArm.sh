#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $dir/Tools/buildFathomArm.sh "$@"
fi

mkdir -p $dir/Dist/Minic2

v="dev"
n="-DUSE_NEON"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

if [ -n "$1" ] ; then
   n=$1
   shift
fi

OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto --std=c++17 $n"

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_android.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

$dir/android/bin/arm-linux-androideabi-clang++ -v
echo "version $v"
exe=minic_${v}_android
echo "Building $exe"
echo $OPT

NNUESOURCE="Source/nnue/features/half_kp.cpp Source/nnue/features/half_relative_kp.cpp Source/nnue/evaluate_nnue.cpp Source/nnue/learn/learn_tools.cpp Source/nnue/learn/convert.cpp Source/nnue/learn/multi_think.cpp Source/nnue/learn/learner.cpp Source/nnue/evaluate_nnue_learner.cpp" 

STANDARDSOURCE="Source/*.cpp"

$dir/android/bin/arm-linux-androideabi-clang++ $OPT $STANDARDSOURCE $NNUESOURCE -ISource -ISource/nnue -o $dir/Dist/Minic2/$exe -static-libgcc -static-libstdc++ 

