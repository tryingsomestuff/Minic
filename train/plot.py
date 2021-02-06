import matplotlib.pyplot as plt
from datetime import datetime
import time
import argparse

parser = argparse.ArgumentParser(description='NNUE training plots.')
parser.add_argument('-s', type=int, help='skip at first', default=0)
parser.add_argument('-k', type=int, help='keep at last', default=0)
parser.add_argument('-p', type=int, help='pause', default=15)
parser.add_argument('-m', type=int, help='pause', default=20)
parser.add_argument('-l', type=int, help='pause', default=40)
args = parser.parse_args()

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

while True:
    with open('logs/ordo.out') as f:
        content = f.readlines()[2:-3]
        X, Y ,err = [], [], []
        names = []
        for line in content:
            if line.isspace():
                continue
            # ['1', 'logs/nn-epoch86.nnue', ':', '35.2', '81.2', '33.0', '60', '55', '65']
            values = [s for s in line.split()]
            l = values[1].strip().replace(' ', '').replace('logs/nn-epoch', '').replace('.nnue', '')
            if l == "master":
                l='0'
                continue
            e = values[4]
            if e == "----":
                e='0'
            X.append(float(l))
            Y.append(float(values[3]))
            err.append(float(e))
            names.append(values[1].replace('logs', ''))

        try:
            X, Y, err, names = zip(*sorted(zip(X, Y, err, names)))
            X = X[args.s:]
            Y = Y[args.s:]
            err = err[args.s:]
            names = names[args.s:]
            X = X[-args.k:]
            Y = Y[-args.k:]
            err = err[-args.k:]
            names = names[-args.k:]

            plt.errorbar(X, Y, yerr=err, fmt='-.', color='gray', marker=None)
            plt.scatter(X, Y, c=['lime' if y>0 else 'black' if y+e > 0 else 'red' for y,e in zip(Y,err)], marker='o', label='Nets Elo')
            plt.scatter(X, [ y+1.5*e for (y,e) in zip(Y,err)], color='violet', marker='x', label='Best luck')
            plt.axhline(y=0, color='tomato', linestyle='-', label='Current master')
            plt.axhline(y=args.m, color='deepskyblue', linestyle='dotted', label='Master + {}'.format(args.m))
            plt.axhline(y=args.l, color='springgreen', linestyle='dashed', label='Master + {}'.format(args.l))
            plt.axhline(y=max([y-e for (y,e) in zip(Y,err)]), color='aqua', linestyle='dashdot', label='Current worst outcome')
            plt.legend()
            plt.show(block=False)
            plt.pause(args.p)
            plt.savefig('elo.png')
            plt.clf()

            now = datetime.now()
            current_time = now.strftime("%Y:%m:%D %H:%M:%S")
            print(current_time)
            nn = 0
            pr = 0
            po = 0
            for (x, y, e, n) in zip(X, Y, err, names): 
                if y - e > 0:
                    print(bcolors.OKGREEN + 'net at {: <5} +/- {: <5} : {}'.format(y,e,n) + bcolors.ENDC)
                    nn+=1
            if nn < 10:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y > 0:
                        print(bcolors.OKBLUE + 'probable net at {: <5} +/- {: <5} : {}'.format(str(y),str(e),n) + bcolors.ENDC)
                        pr+=1
            if nn < 10 and pr < 15:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y + e > 0:
                        print(bcolors.OKCYAN + 'possible net at {: <5} +/- {: <5} : {}'.format(str(y),str(e),n) + bcolors.ENDC)              
                        po+=1
            print('----------------------------------------------------------')
        except KeyboardInterrupt:
            break
        except Exception as e: 
            print(e)
            time.sleep(2)
