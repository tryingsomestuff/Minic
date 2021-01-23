#!/bin/bash

dir=$(dirname $0)

for i in $(seq 1 $1) ; do
   $dir/gen.sh &
   echo "pid: $!"
done

wait
