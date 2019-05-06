#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"
#t="-mpopcnt -msse4.2 -mavx -mavx2 -mbmi -mbmi2"

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
echo "Building $exe"
g++ -s -fprofile-generate -Wall -Wno-char-subscripts -Wno-reorder $d -DNDEBUG -O3 -flto $t --std=c++11 minic.cc -o $dir/Dist/$exe -lpthread -IFathom/src
$dir/Dist/$exe -analyze "r2q1rk1/p4ppp/1pb1pn2/8/5P2/1PBB3P/P1PPQ1P1/2KR3R b - - 1 14" 20
g++ -s -fprofile-use $d -DNDEBUG -O3 -flto $t --std=c++11 minic.cc -o $dir/Dist/$exe -lpthread -IFathom/src

