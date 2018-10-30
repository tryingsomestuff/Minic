#/bin/bash
dir=$(readlink -f $(dirname $0)/..)
if [ -n "$1" ] ; then
   v=$1
else
   v="dev"
fi
g++ -s -fprofile-generate -DNDEBUG -O3 -flto -msse4.2 --std=c++11 minic.cc -o $dir/Dist/minic_${v}_linux_x64_see4.2
$dir/Dist/minic_${v}_linux_x64_see4.2 -analyze "r2q1rk1/p4ppp/1pb1pn2/8/5P2/1PBB3P/P1PPQ1P1/2KR3R b - - 1 14" 15
g++ -s -fprofile-use -DNDEBUG -O3 -flto -msse4.2 --std=c++11 minic.cc -o $dir/Dist/minic_${v}_linux_x64_see4.2
