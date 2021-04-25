import sys

s = sys.argv[1].split(';')[1:-1]

#print s

o = ""

for k in range(15):
    o += '{{{0:>3},{1:>3}}},'.format(s[k],s[15+k])
print(o)
