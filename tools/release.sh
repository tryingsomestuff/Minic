#!/bin/bash
dir=$(readlink -f $(dirname $0)/)
v=$(cat minic.cc | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"

$dir/build.sh $v

$dir/buildGW.sh $v
