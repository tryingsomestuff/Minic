import sys,os,glob

files = glob.glob('TuningOutput/tuning*.csv')
files.sort();
print(files)

# same order as evalConfig.cpp ...
features = [ 'imbalance', 'PST0', 'PST1', 'PST2', 'PST3', 'PST4', 'PST5', 
             'shield', 'Fawn', 'passer', 'rookBehindPassed', 'kingNearPassed', 'pawnStructure1', 'pawnStructure2', 'pawnStructure3', 'pawnStructure4',
             'holes', 'pieceBlocking', 'pawnFrontMinor', 'center', 'knightTooFar', 'candidate', 'protectedPasser', 'freePasser', 'pawnMob', 'pawnAtt', 'pawnlessFlank', 'storm',
             'rookOpen', 'rookQueenFile', 'rookFrontQueen', 'rookFrontKing', 'minorOnOpen', 'pinned', 'hanging', 'minorThreat', 'rookThreat', 'queenThreat', 'kingThreat',
             'adjustN', 'adjustR', 'adjustB', 'badBishop', 'pairAdjust', 'queenNearKing', 
             'mobility', 'initiative', 
             'attDefKing', 'attFunction', 'attOpenFile', 'attNoqueen', 'secondorder' ]

if len(sys.argv) > 1:
    features = sys.argv[1:]

for f in features:
    try:
        with open("TuningOutput/tuning_"+f+".csv", "r") as ff:
            print(f)
            ll = ff.readlines()[-1]
            ff.close()
            if "PST" in f:
                os.system("python ./Tools/fit/PST.py \"" + ll + "\"")
            elif "imbalance" in f:
                os.system("python ./Tools/fit/imbalance.py \"" + ll + "\"")
            elif "secondorder" in f:
                os.system("python ./Tools/fit/secondOrder.py \"" + ll + "\"")
            elif "mobility" in f:
                os.system("python ./Tools/fit/MOB.py \"" + ll + "\"")
            elif "pawnStructure1" in f:
                os.system("python ./Tools/fit/PawnStr1.py \"" + ll + "\"")
            elif "pawnStructure2" in f:
                os.system("python ./Tools/fit/PawnStr1.py \"" + ll + "\"")
            elif "pawnStructure3" in f:
                os.system("python ./Tools/fit/PawnStr1.py \"" + ll + "\"")
            elif "pawnStructure4" in f:
                os.system("python ./Tools/fit/PawnStr1.py \"" + ll + "\"")
            elif "kingNearPassed" in f:
                os.system("python ./Tools/fit/rank_file.py \"" + ll + "\"")
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
                s = int(len(v)/2)
                p = ""
                for i in range(s):
                    p += '{{{0:>3},{1:>3}}}, '.format(v[2*i],v[2*i+1])
                print(p)
    except:
        print("In file {}".format(f))
        print("Unexpected error:", sys.exc_info())
        pass
