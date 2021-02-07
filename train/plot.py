import matplotlib.pyplot as plt
from datetime import datetime
import time
import argparse

class TargetBoundCheck(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if not 0 < values < 1000:
            raise argparse.ArgumentError(self, "target must be between 0 and 1000")
        setattr(namespace, self.dest, values)

parser = argparse.ArgumentParser(description='NNUE training plots.')
parser.add_argument('-s', type=int, help='skip this net at first', default=0 )
parser.add_argument('-k', type=int, help='only keep this last nets', default=0)
parser.add_argument('-p', type=int, help='pause between refresh in sec', default=15)
parser.add_argument('-m', type=int, help='first target', default=20, action=TargetBoundCheck)
parser.add_argument('-l', type=int, help='second target', default=40, action=TargetBoundCheck)
parser.add_argument('-f', type=str, help='ordo file path', default='logs/ordo.out')
parser.add_argument('-n', type=str, help='nets directory path (with trailing /', default='logs/')
args = parser.parse_args()

while True:
    with open(args.f) as f:
        content = f.readlines()[2:-3]
        X, Y ,err = [], [], []
        names = []
        for line in content:
            if line.isspace():
                continue
            # ['1', 'logs/nn-epoch86.nnue', ':', '35.2', '81.2', '33.0', '60', '55', '65']
            values = [s for s in line.split()]
            l = values[1].strip().replace(' ', '').replace(args.n+'nn-epoch', '').replace('.nnue', '')
            if l == "master":
                l='0'
                continue
            e = values[4]
            if e == "----":
                e='0'
            X.append(float(l))
            Y.append(float(values[3]))
            err.append(float(e))
            names.append(values[1].replace(args.n, ''))

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

            plt.subplot(121)

            plt.errorbar(X, Y, yerr=err, fmt='-.', color='gray', marker=None)
            plt.scatter(X, Y, c=['gold' if y-e>0 else 'lime' if y>0 else 'black' if y+e > 0 else 'red' for y,e in zip(Y,err)], s=64, marker='o', label='Nets Elo')
            plt.scatter(X, [ y+1.5*e for (y,e) in zip(Y,err)], color='violet', marker='x', label='Best luck (150%err)')
            plt.axhline(y=0, color='tomato', linestyle='-', label='Current master')
            plt.axhline(y=args.m, color='deepskyblue', linestyle='dotted', label='Master + {}Elo'.format(args.m))
            plt.axhline(y=args.l, color='springgreen', linestyle='dashed', label='Master + {}Elo'.format(args.l))
            plt.axhline(y=max([y-e for (y,e) in zip(Y,err)]), color='aqua', linestyle='dashdot', label='Current worst outcome')
            axes = plt.gca()
            axes.set_ylim([-250,1.5*max(args.m,args.l)])

            plt.legend()

            display_max=30

            now = datetime.now()
            txt = now.strftime("%m/%d/%Y, %H:%M:%S") + '\n\n'
            target2 = 0
            target1 = 0
            good = 0
            probable = 0
            possible = 0
            for (x, y, e, n) in zip(X, Y, err, names): 
                if y - e > args.l:
                    txt += 'target2 net {: <5} +/- {: <5} : {}'.format(y,e,n) + '\n'
                    target2+=1
                    if target2 > display_max:
                        break         
            if target2 < display_max:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y - e > args.m:
                        txt += 'target1 net {: <5} +/- {: <5} : {}'.format(y,e,n) + '\n'
                        target1+=1            
                    if target1+target2 > display_max:
                        break         
            if target1+target2 < display_max:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y - e > 0:
                        txt += 'good net {: <5} +/- {: <5} : {}'.format(y,e,n) + '\n'
                        good+=1
                    if good+target1+target2 > display_max:
                        break         
            if target1+target2+good < display_max:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y > 0:
                        txt +=  'probable net {: <5} +/- {: <5} : {}'.format(str(y),str(e),n) + '\n'
                        probable+=1
                    if probable+good+target1+target2 > display_max:
                        break         
            if target1+target2+good+probable < display_max:
                for (x, y, e, n) in zip(X, Y, err, names): 
                    if y + e > 0:
                        txt += 'possible net {: <5} +/- {: <5} : {}'.format(str(y),str(e),n) + '\n'
                        possible+=1
                    if possible+probable+good+target1+target2 > display_max:
                        break         

            plt.subplot(122)
            plt.axis('off')
            size = plt.gcf().get_size_inches()*plt.gcf().dpi # size in pixels
            plt.text(0, 0, txt, fontsize=size[1]/max(2*display_max+10,2*txt.count('\n')+10))

            plt.show(block=False)
            plt.pause(args.p)
            plt.savefig('elo.png')
            plt.clf()

        except KeyboardInterrupt:
            break
        except Exception as e: 
            print(e)
            time.sleep(2)
