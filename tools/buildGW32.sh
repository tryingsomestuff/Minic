#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

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

exe32=minic_${v}_mingw_x32
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe32=${exe32}_${tname}
fi
exe32=${exe32}.exe
echo "Building $exe32"

i686-w64-mingw32-g++-posix minic.cc $d -DNDEBUG -O3 -flto $t -static -static-libgcc -static-libstdc++ -std=c++11 -o $dir/Dist/$exe32 -Wl,-Bstatic -lpthread
i686-w64-mingw32-strip $dir/Dist/$exe32
