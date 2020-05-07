import random
import sys

def split_huge_file(file,out1,out2,percentage=0.75,seed=42):
   random.seed(seed)
   with open(file, 'r') as fin, \
      open(out1, 'w') as foutBig, \
      open(out2, 'w') as foutSmall:
      for line in fin:
         r = random.random() 
         if r < percentage:
             foutBig.write(line)
         else:
             foutSmall.write(line)

split_huge_file(sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]), int(sys.argv[5]))

