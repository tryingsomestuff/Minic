#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

cd $dir/Fathom/src

v="dev"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

$dir/android/bin/arm-linux-androideabi-clang++ -v

echo "version $v"

lib=fathom_${v}_android.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts $d -DNDEBUG -O3 -flto -I."
$dir/android/bin/arm-linux-androideabi-clang++ -c $OPT tbprobe.c -o $lib -static-libgcc -static-libstdc++ 

cd -
