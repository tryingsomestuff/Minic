#/bin/bash
dir=$(readlink -f $(dirname $0)/..)
if [ -n "$1" ] ; then
   v=$1
else
   v="dev"
fi
g++ -DNDEBUG -O3 -flto -msse4.2 --std=c++11 minic.cc -o $dir/Dist/minic_${v}_linux_x64_see4.2
