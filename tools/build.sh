#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $dir/tools/buildFathom.sh "$@"
fi

mkdir -p $dir/Dist

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

if [ -n "$1" ] ; then
   t=$1
   shift
fi

g++ -v
echo "version $v"
echo "definition $d"
echo "target $t"

exe=minic_${v}_linux_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe=${exe}_${tname}
fi

rm -f minic.gcda

echo "Building $exe"
OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $t --std=c++14"
#OPT="-s -Wall -Wno-char-subscripts -Wno-reorder $d -O1 -g -flto $t --std=c++14"
echo $OPT

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      lib=${lib}_${tname}
   fi
   lib=${lib}.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

if [ -e "$dir/tiny-dnn" ]; then
   echo "found tiny-dnn lib"
   OPT="$OPT -I$dir/tiny-dnn"
fi

g++ -fprofile-generate $OPT minic.cc -o $dir/Dist/$exe -lpthread 
$dir/Dist/$exe -analyze "r2q1rk1/p4ppp/1pb1pn2/8/5P2/1PBB3P/P1PPQ1P1/2KR3R b - - 1 14" 20 -quiet 0 
#$dir/Dist/$exe -analyze "shirov" 20 
g++ -fprofile-use $OPT minic.cc -o $dir/Dist/$exe -lpthread 

