#!/bin/bash
dir=$(readlink -f $(dirname $0)/)

e=minic
v=$(cat Source/definition.hpp | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
#t=... => inlined
n=""
d=""
echo "====================================="
echo "  Releasing $e version $v"
echo "====================================="

# all build script takes arguments in this order (if missing => default)
#   exe_name version arch_target special_options special_definitions

$dir/build.sh $e $v "-march=core2" $n $d
$dir/build.sh $e $v "-march=nehalem"  $n $d
$dir/build.sh $e $v "-march=sandybridge" $n $d
#$dir/build.sh $e $v "-march=haswell" $n $d
$dir/build.sh $e $v "-march=skylake" $n $d

$dir/buildGW.sh $e $v "-march=core2" $n $d
$dir/buildGW.sh $e $v "-march=nehalem" $n $d
$dir/buildGW.sh $e $v "-march=sandybridge" $n $d
#$dir/buildGW.sh $e $v "-march=haswell" $n $d
$dir/buildGW.sh $e $v "-march=skylake" $n $d

$dir/buildGW32.sh $e $v "-march=pentium2" $n $d

$dir/buildAndroid.sh $e $v $n $d
$dir/buildRPi32.sh $e $v $n $d
$dir/buildRPi64.sh $e $v $n $d

#TODO WASM

rootdir=$(readlink -f $dir/../../)

$rootdir/Dist/Minic3/minic_${v}_linux_x64_core2 bench 16 -NNUEFile $rootdir/Tourney/nn.bin 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_nehalem bench 16 -NNUEFile $rootdir/Tourney/nn.bin 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_sandybridge bench 16 -NNUEFile $rootdir/Tourney/nn.bin 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_skylake bench 16 -NNUEFile $rootdir/Tourney/nn.bin 2>&1 | grep NODES

echo '---------------------------------'

$rootdir/Dist/Minic3/minic_${v}_linux_x64_core2 bench 16 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_nehalem bench 16 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_sandybridge bench 16 2>&1 | grep NODES
echo '-------'
$rootdir/Dist/Minic3/minic_${v}_linux_x64_skylake bench 16 2>&1 | grep NODES