import sys

s = sys.argv[1].split(';')[1:-1]

#print s

k = 0
for i in range(6):
    o=""
    for j in range(i):
      o += '{{{0:>5},{1:>5}}}, '.format(s[2*k], s[2*k+2])
      k += 2
    print(o)

k = 0
for i in range(6):
    o=""
    for j in range(i):
      o += '{{{0:>5},{1:>5}}}, '.format(s[2*k+1], s[2*k+3])
      k += 2
    print(o)
