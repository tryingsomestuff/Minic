import sys

s = sys.argv[1].split(';')[1:-1]

#print s

o = []

np = 1

for p in range(np):
    o.append('{\n   ')
    for k in range(64):
        if k != 63:
            o[p] += '{{{0:>4},{1:>4}}}, '.format(s[k+p*64],s[np*64+k+p*64])
        else:
            o[p] += '{{{0:>4},{1:>4}}} '.format(s[k+p*64],s[np*64+k+p*64])
        if (k+1)%8 == 0:
            o[p] += '\n'
            if k != 63:
                o[p] += '   '
    if p != np-1:
        o[p]+=('},')
    else:
        o[p]+=('}')
    print(o[p])
