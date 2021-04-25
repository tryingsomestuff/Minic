#!/bin/bash
dir=$(readlink -f $(dirname $0)/../..)

cd $dir/Fathom/src

v="dev"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

$COMPC -v

echo "version $v"

lib=fathom_${v}_android.o
echo "Building $lib"

OPT="-Wall -Wno-char-subscripts -Wno-unused-function -Wno-deprecated $d -DNDEBUG -O3 -flto -I."
$COMPC -c $OPT tbprobe.c -o $lib

cd -
