import concurrent.futures
from collections import Counter
import chess
import chess.pgn
import re
import sys
import random
import json
import os
import argparse
import io


class PosAnalyser:
    def __init__(self, plies):
        self.matching_plies = plies

    def ana_pos(self, filename):
        print("parsing {}".format(filename))
        pgnfilein = open(filename, "r", encoding="utf-8-sig", errors="surrogateescape")
        gameCounter = 0
        matstats = Counter()

        p = re.compile("([+-]*M*[0-9.]*)/([0-9]*)")
        mateRe = re.compile("([+-])M[0-9]*")

        while True:
            # read game
            game = chess.pgn.read_game(pgnfilein)
            
            if game == None:
                break

            white_player = game.headers["White"]
            black_player = game.headers["Black"]
            white_elo = int(game.headers["WhiteElo"])
            black_elo = int(game.headers["BlackElo"])
            
            is_minic = "minic" in white_player.lower() or "minic" in black_player.lower()
            is_good = white_elo > 3000 and black_elo > 3000
            is_ok = white_elo > 2800 and black_elo > 2800

            if not ((is_minic and is_ok) or is_good):
                continue                

            gameCounter = gameCounter + 1

            print(gameCounter)

            # get result
            result = game.headers["Result"]
            if result == "1/2-1/2":
                resultkey = "D"
            elif result == "1-0":
                resultkey = "W"
            elif result == "0-1":
                resultkey = "L"
            else:
                continue

            # look at the game,
            plies = 0
            board = game.board()
            fen = None
            try:
                for node in game.mainline():

                    plies = plies + 1
                    if plies > 300:
                        break
                    plieskey = (plies + 1) // 2

                    turn = board.turn
                    scorekey = None
                    m = p.search(node.comment)
                    if m:
                        score = m.group(1)
                        depth = int(m.group(2))
                        m = mateRe.search(score)
                        if m:
                            if m.group(1) == "+":
                                score = 1001
                            else:
                                score = -1001
                        else:
                            score = int(float(score) * 100)
                            if score > 1000:
                                score = 1000
                            elif score < -1000:
                                score = -1000
                            score = (score // 5) * 5  # reduce precision
                        if turn == chess.BLACK:
                            score = -score
                        scorekey = score

                    knights = len(board.pieces(chess.KNIGHT, chess.WHITE)) + len(
                        board.pieces(chess.KNIGHT, chess.BLACK)
                    )
                    bishops = len(board.pieces(chess.BISHOP, chess.WHITE)) + len(
                        board.pieces(chess.BISHOP, chess.BLACK)
                    )
                    rooks = len(board.pieces(chess.ROOK, chess.WHITE)) + len(
                        board.pieces(chess.ROOK, chess.BLACK)
                    )
                    queens = len(board.pieces(chess.QUEEN, chess.WHITE)) + len(
                        board.pieces(chess.QUEEN, chess.BLACK)
                    )
                    pawns = len(board.pieces(chess.PAWN, chess.WHITE)) + len(
                        board.pieces(chess.PAWN, chess.BLACK)
                    )
                    matcountkey = 9 * queens + 5 * rooks + 3 * knights + 3 * bishops + pawns

                    if scorekey:
                        matstats[(resultkey, plieskey, matcountkey, scorekey)] += 1

                    board = node.board()
            except:
                print("error in game {}".format(gameCounter))
                pass

        pgnfilein.close()
        return matstats


parser = argparse.ArgumentParser()

parser.add_argument(
    "--matching_plies",
    type=int,
    default=6,
    help="Number of plies that the material situation needs to be on the board.",
)
args = parser.parse_args()

# find the set of fishtest pgns (looking in the full file tree)
p = re.compile(".*.pgn")

pgns = []
for path, subdirs, files in os.walk(".", followlinks=True):
    print(files)
    for name in files:
        m = p.match(name)
        if m:
            pgns.append(os.path.join(path, name))

print(pgns)

# map sharp_pos to all pgn files using an executor
ana = PosAnalyser(args.matching_plies)

res = Counter()
with concurrent.futures.ProcessPoolExecutor() as e:
    results = e.map(ana.ana_pos, pgns, chunksize=100)
    for r in results:
        res.update(r)

# and print all the fens
with open("scoreWLDstat.json", "w") as outfile:
    json.dump(
        {
            str(k): v
            for k, v in sorted(res.items(), key=lambda item: item[1], reverse=True)
        },
        outfile,
        indent=1,
    )
