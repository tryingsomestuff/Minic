#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir/Fathom/src

v="dev"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

$CC -v

echo "version $v"

lib=fathom_${v}_linux_x32_armv7.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto -I."
$CC -c $OPT tbprobe.c -o $lib -static-libgcc -static-libstdc++ 

cd -
