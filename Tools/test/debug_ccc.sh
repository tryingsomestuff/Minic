#!/bin/bash

ccc=/ssd/c-chess-cli/c-chess-cli

$ccc -each tc=10+0.1 -engine "cmd=/ssd/Minic/runmpi.sh" -engine "cmd=/ssd/Minic/Dist/Minic3/minic_dev_linux_x64 -uci -threads 1" -games 100 -concurrency 1 -pgn out.pgn
