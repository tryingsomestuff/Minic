import matplotlib.pyplot as plt
import numpy as np
import math

with open("output") as file:
    lines = [line.rstrip() for line in file]

    data = []
    errors = []
    names = []
    elo = 0
    error = 0
    param = None

    count = 0

    for line in lines:
        #print(line)
        if "Estimated" in line:
            if param is None:
                raise "error beurk"
            elo = line.split(':')[3]
            error = elo.split('+')[1]
            elo = elo.split('+')[0]
            error = error.split('-')[1]
            
            #print(param)
            #print(elo)
            #print(error)
            names.append(param)
            param = None
            data.append(elo)
            errors.append(error)
            count += 1
        else:
            param = line

    X = range(count)

    acc = []
    acce = []
    p = 0
    for i in range(count):
        acc.append(p + float(data[i]))
        p = acc[-1]

    for i in range(count):
        s = 0
        for j in range(i):
            s += float(errors[j])*float(errors[j])
            for k in range(j):
                s += float(errors[j])*float(errors[k])*0.
        acce.append(math.sqrt(s))

    for i in range(count):
        print("{: >20} {: >20} {: >20} {: >20}".format(data[i], errors[i], acc[i], acce[i]))


    plt.errorbar(X, [float(d) for d in acc], yerr=[float(e) for e in acce], fmt='.', color='gray', marker=None, capsize=2)

    plt.show()
    plt.savefig("trace.png")
