#!/bin/bash

exe=/ssd/Minic/minic.perf

if [ -n "$1" ]; then
	exe=$1
fi

echo "***************************************************"
echo Benching $exe  
echo "***************************************************"
$exe -uci -minOutputLevel 0 <<EOF
uci
ucinewgame
setoption name Hash value 256
setoption name Threads value 8
position fen r1bqkb1r/pp3ppp/1np1pn2/6N1/2BP4/8/PPP1QPPP/R1B1K1NR w KQkq - 1 1
go depth 22
wait
quit
EOF
