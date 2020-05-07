import chess

def file_len(fname):
    with open(fname) as f:
        for i, l in enumerate(f):
            pass
    return i + 1

filepath = "tuning/quiet-labeled.epd"
with open(filepath) as fp:
   line = fp.readline()
   c = file_len(filepath)
   i = 0
   while line:
      edp = chess.Board(' '.join(line.split(' ')[:4]))
      if not edp.is_valid():
         print( "invalid position" + line)
      line = fp.readline()
      i = i + 1
      if not i % 10000:
          print( " {} / {} ".format(i,c))
