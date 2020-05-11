import sys,os,glob

files = glob.glob('tuning*.csv')
files.sort();
print(files)

for f in files:
    print(f)
    ff = open(f, "r")
    ll = ff.readlines()[-1]
    ff.close()
    if "PST" in f:
        os.system("python ./Tools/PST.py \"" + ll + "\"")
    elif "imbalance" in f:
        os.system("python ./Tools/imbalance.py \"" + ll + "\"")
    elif "mobility" in f:
        os.system("python ./Tools/MOB.py \"" + ll + "\"")
    elif "pawnStruture1" in f:
        os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
    elif "pawnStruture2" in f:
        os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
    elif "pawnStruture3" in f:
        os.system("python ./Tools/PawnStr1.py \"" + ll + "\"")
    elif "att" in f:
        print(ll)
    else:
        v = ll.split(';')[1:-1]
        i = len(v)/2
        p = ""
        for i in range(i):
            p += '{{{0:>4},{1:>4}}}, '.format(v[2*i],v[2*i+1])
        print(p)
        
        
        

