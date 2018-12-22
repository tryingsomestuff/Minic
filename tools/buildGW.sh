#/bin/bash
dir=$(readlink -f $(dirname $0)/..)
if [ -n "$1" ] ; then
   v=$1
else
   v="dev"
fi
export MINIC_NUM_THREADS=1
x86_64-w64-mingw32-g++ minic.cc -DNDEBUG -O3 -flto -static -static-libgcc -static-libstdc++ -std=c++11 -o $dir/Dist/minic_${v}_mingw_x64.exe -lpthread
x86_64-w64-mingw32-strip $dir/Dist/minic_${v}_mingw_x64.exe
#i686-w64-mingw32-g++ minic.cc -DNDEBUG -O3 -flto -static -static-libgcc -static-libstdc++ -std=c++11 -o $dir/Dist/minic_${v}_mingw_x86.exe -lpthread
#i686-w64-mingw32-strip $dir/Dist/minic_${v}_mingw_x86.exe
