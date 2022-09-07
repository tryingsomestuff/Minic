#!/bin/bash
dir=$(readlink -f $(dirname $0)/)

e=minic
v=$(cat Source/config.hpp | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
#t=... => inlined
n=""
d=""
echo "====================================="
echo "  Releasing $e version $v"
echo "====================================="

# all build script takes arguments in this order (if missing => default)
#   exe_name version arch_target special_options special_definitions

# Intel main arch
for m in -march=core2 -march=nehalem -march=sandybridge -march=skylake; do
   $dir/build.sh $e $v $m $n $d
   $dir/buildGW.sh $e $v $m $n $d
done

# AMD main arch
export NOPROFILE=1
for m in -march=athlon64-sse3 -march=barcelona -march=bdver1 -march=znver1 -march=znver3; do
   $dir/build.sh $e $v $m $n $d
   $dir/buildGW.sh $e $v $m $n $d
done
export NOPROFILE

# an old win32 build
$dir/buildGW32.sh $e $v "-march=pentium2" $n $d

# some fun stuff
$dir/buildAndroid.sh $e $v $n $d
$dir/buildRPi32.sh $e $v $n $d
$dir/buildRPi64.sh $e $v $n $d

#TODO WASM

#TODO M1

rootdir=$(readlink -f $dir/../../)
buildDir=$rootdir/Dist/Minic3/$v
net=$rootdir/Tourney/nn.bin

$buildDir/minic_${v}_linux_x64_core2 bench 16 -NNUEFile $net 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_nehalem bench 16 -NNUEFile $net 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_sandybridge bench 16 -NNUEFile $net 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_skylake bench 16 -NNUEFile $net 2>&1 | grep NODES

echo '---------------------------------'

$buildDir/minic_${v}_linux_x64_core2 bench 16 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_nehalem bench 16 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_sandybridge bench 16 2>&1 | grep NODES
echo '-------'
$buildDir/minic_${v}_linux_x64_skylake bench 16 2>&1 | grep NODES
