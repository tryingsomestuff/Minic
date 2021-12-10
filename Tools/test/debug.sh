#!/bin/bash

dir=$(readlink -f .)

/usr/games/xboard -fcp "$dir/runmpi.sh -randomOpen 30 -threads 1" -fd "$dir/" -scp "$dir/Dist/Minic3/minic_dev_linux_x64 -xboard -threads 1 -randomOpen 30" -sd "$dir/" -tc 0:10 -mps 40 -xponder -debug
