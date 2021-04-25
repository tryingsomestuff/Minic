import sys

s = sys.argv[1].split(';')[1:-1]

#print s

o = []

np = 1

for p in range(np):
    o.append('{\n   ')
    for k in range(64):
        o[p] += '{{{0:>4},{1:>4}}},'.format(s[k+p*64],s[np*64+k+p*64])
        if (k+1)%8 == 0:
            o[p] += '\n'
            if k != 63:
                o[p] += '   '
    o[p]+=('},')
    print(o[p])
