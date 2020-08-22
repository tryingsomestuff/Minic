import sys

s = sys.argv[1].split(';')[1:-1]

#print s

k = 0
for i in range(1,7):
    o=""
    for j in range(i):
      o += '{{{0:>5},{1:>5}}}, '.format(s[k], s[k+1])
      k += 2
    print("{ " + o + "},")

