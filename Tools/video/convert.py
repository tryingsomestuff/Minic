import sys
import gif_maker
obj = gif_maker.Gifmaker()
obj.make_gif_from_pgn_file(sys.argv[1], sys.argv[2])
