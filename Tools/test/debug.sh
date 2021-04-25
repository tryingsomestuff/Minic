#!/bin/bash

dir=$(readlink -f .)

/usr/games/xboard -fcp "$dir/Dist/Minic3/minic_3.05_linux_x64_skylake -xboard -randomOpen 30 -threads 1" -fd "$dir/" -scp "mpirun -np 2 $dir/Dist/Minic3/minic_dev_linux_x64 -xboard -threads 2 -quiet 1 -debugMode 1 -randomOpen 30" -sd "$dir/" -tc 0:10 -mps 40 -xponder -debug
