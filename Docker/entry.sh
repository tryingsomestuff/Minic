#!/bin/bash

if [ "$#" -ne 4 ]; then
   echo "Illegal number of parameters"
   echo "\$1 shall be your username"
   echo "\$2 shall be your password"
   echo "\$3 shall be your available number of threads"
   echo "\$4 shall be the instance name"
   exit 1
fi

echo "HELLO ! this is Minic OpenBench client docker"
echo "*************************************"
python3 client.py -U $1 -P $2 -N 1 -T $3 -I $4 -S http://home.x-ray.fr:8000
