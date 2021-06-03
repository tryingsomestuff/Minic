#!/bin/bash

ccc=/ssd/c-chess-cli/c-chess-cli

$ccc -each tc=10+0.1 -engine "cmd=/ssd/Minic/runmpi.sh" -engine "cmd=/ssd/Minic/Tourney/minic_dev_linux_x64 -uci -randomOpen 135" -games 1 -concurrency 1 -pgn out.pgn 
