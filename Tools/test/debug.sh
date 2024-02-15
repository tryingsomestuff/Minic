#!/bin/bash

dir=$(readlink -f .)

/usr/games/xboard -fcp "$dir/runmpi_x.sh 6 -randomOpen 15 -threads 1" -fd "$dir/" -scp "$dir/Dist/Minic3/minic_dev_linux_x64 -xboard -threads 1 -randomOpen 15" -sd "$dir/" -tc 0:10 -mps 40 -xponder -debug
