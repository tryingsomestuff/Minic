from itertools import islice
import os
import sys

if len(sys.argv) < 2:
    print("Usage: {} <sfen file>".format(sys.argv[0]))
    sys.exit(1)

filename = sys.argv[1]

## sfen format
#fen q1brnrkb/pppppp1p/2n3p1/8/4P2P/2N2P2/PPPP2P1/R1BKRBQN b EA h3 0 4
#move b7b6
#score 87
#ply 8
#result -1
#e

## epd format
# 4r1k1/5ppp/1q1Pp3/1p1p4/1Qr2PP1/P4R2/1P4KP/R7 w - - c1 "3 3"; c2 "0.000";

with open(filename) as file:
    fen=None
    score=None
    result=None
    while True:
        next_n_lines = list(islice(file, 6))
        if not next_n_lines:
            break    
        str_len, str_move, str_score, str_ply, str_result, str_ending = next_n_lines
        fen = ' '.join(str_len.rstrip().split()[1:])
        score = str_score.rstrip().split()[1]
        result = str_result.rstrip().split()[1]
        print("{} c1 \"{}\"; c2 \"{}\";".format(fen, result, score))
