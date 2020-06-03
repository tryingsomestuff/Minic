import sys,os,glob

files = glob.glob('tuning*.csv')
files.sort();
print(files)

# same order as evalConfig.cpp ...
features = [ 'imbalance', 'PST0', 'PST1', 'PST2', 'PST3', 'PST4', 'PST5', 
             'shield', 'Fawn', 'passer', 'rookBehindPassed', 'kingNearPassed', 'pawnStructure1', 'pawnStructure2', 'pawnStructure3', 'pawnStructure4',
             'holes', 'pieceBlocking', 'center', 'knightTooFar', 'candidate', 'protectedPasser', 'freePasser', 'pawnMob', 'pawnAtt', 'pawnlessFlank', 'storm',
             'rookOpen', 'rookQueenFile', 'rookFrontQueen', 'rookFrontKing', 'minorOnOpen', 'pinned', 'hanging', 'minorThreat', 'rookThreat', 'queenThreat', 'kingThreat',
             'adjustN', 'adjustR', 'adjustB', 'badBishop', 'pairAdjust', 'queenNearKing', 
             'mobility', 'initiative', 
             'attDefKing', 'attFunction', 'attOpenFile', 'secondOrder' ]

for f in features:
    print(f)
    with open("tuning_"+f+".csv", "r") as ff:
        ll = ff.readlines()[-1]
        ff.close()
        if "PST" in f:
            os.system("python ./Tools/PST.py \"" + ll + "\"")
        elif "imbalance" in f:
            os.system("python ./Tools/imbalance.py \"" + ll + "\"")
        elif "secondOrder" in f:
            os.system("python ./Tools/secondOrder.py \"" + ll + "\"")
        elif "mobility" in f:
            os.system("python ./Tools/MOB.py \"" + ll + "\"")
        elif "pawnStructure1" in f:
            os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
        elif "pawnStructure2" in f:
            os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
        elif "pawnStructure3" in f:
            os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
        elif "pawnStructure4" in f:
            os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
        elif "attDefKing" in f:
            o = ""
            v = ll.split(';')[1:-1]
            for i in range(6):
                o += '{0:>3}, '.format(v[i])
            o = "{ " + o + "},"
            o2 = ""
            for i in range(5):
               o2 += '{0:>3}, '.format(v[6+i])
            o2 = "{ " + o2 + " 0 }"
            print("{ " + o + o2 + " }")
        elif "att" in f:
            print(ll)
        else:
            v = ll.split(';')[1:-1]
            i = len(v)/2
            p = ""
            for i in range(i):
                p += '{{{0:>3},{1:>3}}}, '.format(v[2*i],v[2*i+1])
            print(p)
