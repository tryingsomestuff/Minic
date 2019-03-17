#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"

if [ -n "$1" ] ; then
   v=$1
fi

g++ -v
echo "version $v"
echo "definition $d"
exe=minic_${v}_mingw_x64.exe

x86_64-w64-mingw32-g++ minic.cc $d -DNDEBUG -O3 -flto -static -static-libgcc -static-libstdc++ -std=c++11 -o $dir/Dist/$exe -lpthread
x86_64-w64-mingw32-strip $dir/Dist/$exe

#exe32=minic_${v}_mingw_x32.exe
#i686-w64-mingw32-g++ minic.cc $d -DNDEBUG -O3 -flto -static -static-libgcc -static-libstdc++ -std=c++11 -o $dir/Dist/$exe32 -lpthread
#i686-w64-mingw32-strip $dir/Dist/$exe32
