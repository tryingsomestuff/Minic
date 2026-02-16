#!/bin/bash

if [ -z $CXX ]; then
   export CXX=g++
fi

if [ -z $CC ]; then
   # If using Intel oneAPI C++ compiler, use corresponding C compiler
   if [[ "$CXX" == *"icpx"* ]] || [[ "$CXX" == *"mpiicpx"* ]]; then
      export CC=mpiicx
   else
      export CC=gcc
   fi
fi

source $(dirname $0)/common
cd_root

do_title "Building Fathom"

cd $dir/Fathom/src

lib=fathom_${v}_linux_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   lib=${lib}_${tname}
fi
lib=${lib}.o
echo "Building $lib"

# Disable LTO for Intel compilers to avoid profiling issues
if [[ "$CC" == *"mpiicx"* ]] || [[ "$CC" == *"icx"* ]]; then
   OPT="-Wall $d -DNDEBUG -O3 $t -I."
else
   OPT="-Wall $d -DNDEBUG -O3 -flto=auto $t -I."
fi
#-Wno-char-subscripts
$CC -c -std=gnu99 $OPT tbprobe.c -o $lib

cd - > /dev/null
