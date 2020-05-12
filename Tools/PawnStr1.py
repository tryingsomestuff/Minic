import sys

s = sys.argv[1].split(';')[1:-1]

#print s

o = ""

for k in range(8):
   o += '{{{{{0:>3},{1:>3}}},{{{2:>3},{3:>3}}}}}, '.format(s[k*4 ], s[k*4 + 1], s[k*4 + 2], s[k*4 + 3])
print("{ " + o + " },")
