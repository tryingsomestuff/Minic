import matplotlib.pyplot as plt
from datetime import datetime
import time
import argparse

class TargetBoundCheck(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if not 0 < values < 1000:
            raise argparse.ArgumentError(self, "target must be between 0 and 1000")
        setattr(namespace, self.dest, values)

def str2bool(v):
    if isinstance(v, bool):
       return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')

parser = argparse.ArgumentParser(description='NNUE training plots.')
parser.add_argument('-e', type=str2bool, help='with error bar', default=False )
parser.add_argument('-x', type=str2bool, help='with exploration data', default=False )
parser.add_argument('-s', type=int,      help='skip this net at first', default=0 )
parser.add_argument('-k', type=int,      help='only keep this last nets', default=0)
parser.add_argument('-p', type=int,      help='pause between refresh in sec', default=15)
parser.add_argument('-y', type=int,      help='y axis low limit', default=-150)
parser.add_argument('-m', type=int,      help='first target', default=20, action=TargetBoundCheck)
parser.add_argument('-l', type=int,      help='second target', default=40, action=TargetBoundCheck)
parser.add_argument('-f', type=str,      help='ordo file path', default='logs/ordo.out')
parser.add_argument('-n', type=str,      help='nets directory path (with trailing /', default='logs/')
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
            # sort based on X value
            X, Y, err, names = zip(*sorted(zip(X, Y, err, names)))
            
            # filter as requested
            X = X[args.s:]
            Y = Y[args.s:]
            err = err[args.s:]
            names = names[args.s:]
            X = X[-args.k:]
            Y = Y[-args.k:]
            err = err[-args.k:]
            names = names[-args.k:]

            Yp150e = [y+1.5*e for (y,e) in zip(Y,err)]
            Yme = [y-e for (y,e) in zip(Y,err)]

            _, Ycur, Xcur = zip(*sorted(zip(Yp150e, Y, X)))
            Xcur = Xcur[-3:]
            Ycur = Ycur[-3:]

            _, Ybest, Xbest = zip(*sorted(zip(Yme, Y, X)))
            Xbest = Xbest[-3:]
            Ybest = Ybest[-3:]

            plt.subplot(121)

            # error bar and dot color
            if args.e:
                plt.errorbar(X, Y, yerr=err, fmt='.', color='gray', marker=None, capsize=2)
            plt.scatter(X, Y, c=['gold' if y-e>args.l else 'silver' if y-e>args.m else 'lime' if y-e>0 else 'cyan' if y>0 else 'black' if y+e > 0 else 'red' for y,e in zip(Y,err)], s=64, marker='o', label='Nets Elo')

            # specific markers
            if args.x:
               plt.scatter(X,     Yp150e, color='violet', marker='x', label='Best luck (150%err)')
            plt.scatter(Xcur,  Ycur,   color='blue',   marker='X', label='Current analysis'   , s=84)
            plt.scatter(Xbest, Ybest,  color='green',  marker='^', label='Current best'       , s=84)

            # h lines
            plt.axhline(y=0       , color='tomato'     , linestyle='-'      , label='Current master')
            plt.axhline(y=args.m  , color='deepskyblue', linestyle='dotted' , label='Master + {}Elo'.format(args.m))
            plt.axhline(y=args.l  , color='springgreen', linestyle='dashed' , label='Master + {}Elo'.format(args.l))
            plt.axhline(y=max(Yme), color='aqua'       , linestyle='dashdot', label='Current worst outcome')

            # axis config
            axes = plt.gca()
            axes.set_ylim([args.y,1.5*max(args.m,args.l)])

            plt.legend()

            display_max=30

            now = datetime.now()
            txt = [now.strftime("%m/%d/%Y, %H:%M:%S\n")]
            target2 = []
            target1 = []
            good = []
            probable = []
            possible = []
            for (x, y, e, n) in zip(X, Y, err, names): 
                if y - e > args.l:
                    target2.append([x,y,e,n])
                elif y - e > args.m:
                    target1.append([x,y,e,n])
                elif y - e > 0:
                    good.append([x,y,e,n])
                elif y > 0:
                    probable.append([x,y,e,n])
                elif y + e > 0:
                    possible.append([x,y,e,n])

            txt += ['target2  net {: <5} +/- {: <5} : {}'.format(y,e,n) for (x,y,e,n) in target2 ]
            txt += ['target1  net {: <5} +/- {: <5} : {}'.format(y,e,n) for (x,y,e,n) in target1 ]
            txt += ['good     net {: <5} +/- {: <5} : {}'.format(y,e,n) for (x,y,e,n) in good ]
            txt += ['probable net {: <5} +/- {: <5} : {}'.format(y,e,n) for (x,y,e,n) in probable ]
            txt += ['possible net {: <5} +/- {: <5} : {}'.format(y,e,n) for (x,y,e,n) in possible ]

            txt = txt[:display_max+2]

            plt.subplot(122)
            plt.axis('off')
            size = plt.gcf().get_size_inches()*plt.gcf().dpi # size in pixels
            plt.text(0, 0, '\n'.join(txt), fontsize=size[1]/max(2*display_max+10,2*txt.count('\n')+10))

            plt.show(block=False)
            plt.pause(args.p)
            plt.savefig('elo.png')
            plt.clf()

        except KeyboardInterrupt:
            break
        except Exception as e: 
            print(e)
            time.sleep(2)
