import chess
import chess.pgn
import sys

pgn = open(sys.argv[1])
game = chess.pgn.read_game(pgn)
board = game.board()

print(board.fen())
moves=[]
for move in game.mainline_moves():
    board.push(move)
    moves.append(move.uci())
print(board.fen())
print(" ".join(moves))
