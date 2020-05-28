#!/bin/bash

if [ "$#" -ne 3 ]; then
   echo "Illegal number of parameters"
   echo "\$1 shall be your username"
   echo "\$2 shall be your password"
   echo "\$3 shall be your available number of threads"
   exit 1
fi

echo "HELLO ! this is Minic OpenBench client docker"
echo 42 > machine.txt
echo "*************************************"
ls
echo "*************************************"
python3 Client.py -U $1 -P $2 -S http://canada.x-ray.fr:8000 -T $3
