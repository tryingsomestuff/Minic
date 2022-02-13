/ssd/Minic/minic.perf -uci -minOutputLevel 0 <<EOF
uci
ucinewgame
setoption name Hash value 256
setoption name Threads value 8
position fen r1bqkb1r/pp3ppp/1np1pn2/6N1/2BP4/8/PPP1QPPP/R1B1K1NR w KQkq - 1 1
go depth 22
wait
quit
EOF
