#!/bin/bash

ccc=/ssd/c-chess-cli/c-chess-cli

$ccc -each tc=10+0.1 -engine "cmd=mpirun -np 2 /ssd/Minic/Dist/Minic3/minic_dev_linux_x64 -uci -randomOpen 35 -debugMode 1 -moveOverHead 100" -engine "cmd=/ssd/Minic/Dist/Minic3/minic_dev_linux_x64 -uci -randomOpen 35" -games 100 -concurrency 4 -pgn out.pgn &
