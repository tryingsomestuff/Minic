#/bin/bash
dir=$(readlink -f $(dirname $0)/..)
if [ -n "$1" ] ; then
   v=$1
   d="-DDEBUG_TOOL"
else
   v="dev"
   d="-DDEBUG_TOOL"
fi
export MINIC_NUM_THREADS=1
g++ -s -fprofile-generate $d -DNDEBUG -O3 -flto -msse4.2 --std=c++11 minic.cc -o $dir/Dist/minic_${v}_linux_x64_see4.2 -lpthread
$dir/Dist/minic_${v}_linux_x64_see4.2 -analyze "r2q1rk1/p4ppp/1pb1pn2/8/5P2/1PBB3P/P1PPQ1P1/2KR3R b - - 1 14" 15
g++ -s -fprofile-use $d -DNDEBUG -O3 -flto -msse4.2 --std=c++11 minic.cc -o $dir/Dist/minic_${v}_linux_x64_see4.2 -lpthread
