#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir/Fathom/src

d=""
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

x86_64-w64-mingw32-g++ -v
echo "version $v"
echo "definition $d"
echo "target $t"

lib=fathom_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   lib=${lib}_${tname}
fi
lib=${lib}.o
echo "Building $lib"

OPT="-s -Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto $t -I."
x86_64-w64-mingw32-gcc -c $OPT tbprobe.c -o $lib

cd -
