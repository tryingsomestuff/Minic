import sys

s = sys.argv[1].split(';')[1:-1]

print s

o = []

for t in range(3):
   o.append("")
   for k in range(8):
      o[t] += '{{{{{0:>3},{1:>3}}},{{{2:>3},{3:>3}}}}}, '.format(s[k*3*4 + 4*t], s[k*3*4 + 4*t + 1], s[k*3*4 + 4*t + 2], s[k*3*4 + 4*t + 3])
   print(o[t])
