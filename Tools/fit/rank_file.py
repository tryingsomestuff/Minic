import sys

s = sys.argv[1].split(';')[1:-1]

#print s

o = []

k = 0
for i in range(8):
    o=""
    for j in range(8):
      if j != 7:
        o += '{{{0:>5},{1:>5}}}, '.format(s[2*k], s[2*k+1])
      else:
        o += '{{{0:>5},{1:>5}}} '.format(s[2*k], s[2*k+1])
      k += 1
    if i != 7:
      print("{ " + o + "},")
    else:
      print("{ " + o + "}")

print(" ")

k = 0
for i in range(8):
    o=""
    for j in range(8):
      if j != 7:
        o += '{{{0:>5},{1:>5}}}, '.format(s[128+2*k], s[128+2*k+1])
      else:
        o += '{{{0:>5},{1:>5}}} '.format(s[128+2*k], s[128+2*k+1])
      k += 1
    if i != 7:
      print("{ " + o + "},")
    else:
      print("{ " + o + "}")
