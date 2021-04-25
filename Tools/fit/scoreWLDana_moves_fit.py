from collections import Counter
import sys
import json
import os
import argparse
import io
from ast import literal_eval
import matplotlib.pyplot as plt
import numpy as np
from scipy.interpolate import griddata
from scipy.optimize import curve_fit
import numpy as np

#
# read score stats as obtained from fishtest games
#
with open("scoreWLDstat.json", "r") as infile:
    inputdata = json.load(infile)

print("read data")

#
# transform to just score, move data
#
inpdict = {literal_eval(k): v for k, v in inputdata.items()}

win = Counter()
loss = Counter()
draw = Counter()
for k, v in inpdict.items():
    (result, move, material, score) = k
    if score < -400 or score > 400:
        continue
    if move < 0 or move > 128:
        continue
    if result == "W":
        win[(score, move)] += v
    elif result == "L":
        loss[(score, move)] += v
    elif result == "D":
        draw[(score, move)] += v

print("counted")

#
# make score, move -> win ratio data
#
coords = list(
    set(k for k in win).union(set(k for k in loss)).union(set(k for k in draw))
)
coords.sort()
xs = []
ys = []
zs = []
for coord in coords:
    total = float(win[coord] + loss[coord] + draw[coord])
    x, y = coord
    xs.append(x)
    ys.append(y)
    zs.append(win[coord] / total)

#
# fit a model to predict winrate from score and move
# define model functions
#

def winmodel(x, a, b):
    return 1.0 / (1.0 + np.exp(-(x - a) / b))

def poly3(x, a, b, c, d):
    xnp = np.asarray(x) / 32
    return ((a * xnp + b) * xnp + c) * xnp + d

def wdl(score, move, popt_as, popt_bs):
    a = poly3(move, *popt_as)
    b = poly3(move, *popt_bs)
    w = int(1000 * winmodel(score, a, b))
    l = int(1000 * winmodel(-score, a, b))
    d = 1000 - w - l
    return (w, d, l)

#
# convert to model
#
scores, moves, winrate = xs, ys, zs

print(scores)
print("#################")
print(moves)
print("#################")
print(winrate)

model_ms = []
model_as = []
model_bs = []

grouping = 1
for m in range(10, 120, grouping):
    mmin = m
    mmax = m + grouping
    xdata = []
    ydata = []
    for i in range(0, len(moves)):
        if moves[i] < mmin or moves[i] >= mmax:
            continue
        xdata.append(scores[i])
        ydata.append(winrate[i])

    if not ydata:
        continue

    popt, pcov = curve_fit(winmodel, xdata, ydata, p0=[125, 55])
    model_ms.append(m)
    model_as.append(popt[0])
    model_bs.append(popt[1])

    if m == 30:
        plt.figure(figsize=(24, 16))
        plt.plot(xdata, ydata, "b.", label="Measured winrate")
        ymodel = []
        for x in xdata:
            ymodel.append(winmodel(x, popt[0], popt[1]))
        plt.plot(xdata, ymodel, "r-", label="Model winrate ")
        plt.xlabel("score")
        plt.ylabel("outcome")
        plt.legend()
        plt.show()
        plt.close()


popt_as, pcov = curve_fit(poly3, model_ms, model_as)
label_as = "as = ((%5.3f * x / 32 + %5.3f) * x / 32 + %5.3f) * x / 32 + %5.3f" % tuple(
    popt_as
)
print(label_as)
print(popt_as)

popt_bs, pcov = curve_fit(poly3, model_ms, model_bs)
label_bs = "bs = ((%5.3f * x / 32 + %5.3f) * x / 32 + %5.3f) * x / 32 + %5.3f" % tuple(
    popt_bs
)
print(label_bs)
print(popt_bs)
plt.figure(figsize=(24, 16))
plt.plot(model_ms, model_as, "b.", label="as")
plt.plot(model_ms, poly3(model_ms, *popt_as), "r-", label="fit: " + label_as)
plt.plot(model_ms, model_bs, "g.", label="bs")
plt.plot(model_ms, poly3(model_ms, *popt_bs), "m-", label="fit: " + label_bs)

plt.xlabel("move")
plt.ylabel("parameter")
plt.legend()
plt.show()
plt.close()

print(wdl(330, 21, popt_as, popt_bs))

#
# now generate contour plot of raw data
#
print("processing done, plotting data")
font = {"family": "DejaVu Sans", "weight": "normal", "size": 20}
grid_x, grid_y = np.mgrid[-200:400:30j, 10:120:22j]
points = np.array(list(zip(xs, ys)))
zz = griddata(points, zs, (grid_x, grid_y), method="cubic")
fig = plt.figure(figsize=(24, 16))
plt.rc("font", **font)
left, bottom, width, height = 0.1, 0.1, 0.8, 0.8
ax = fig.add_axes([left, bottom, width, height])
cp = plt.contourf(grid_x, grid_y, zz, [0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0])
plt.colorbar(cp)
CS = plt.contour(
    grid_x, grid_y, zz, [0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0], colors="black"
)
ax.clabel(CS, inline=1, colors="black")
ax.set_title(
    "Fraction of positions, with a given move number and score, leading to a win"
)
ax.set_xlabel("score")
ax.set_ylabel("move")
ax.yaxis.grid(True)
ax.xaxis.grid(True)
plt.show()
plt.close()

#
# now model
#

for i in range(0, len(xs)):
    zs[i] = wdl(xs[i], ys[i], popt_as, popt_bs)[0] / 1000.0

print("processing done, plotting model")
font = {"family": "DejaVu Sans", "weight": "normal", "size": 20}
grid_x, grid_y = np.mgrid[-200:400:30j, 10:120:22j]
points = np.array(list(zip(xs, ys)))
zz = griddata(points, zs, (grid_x, grid_y), method="cubic")
fig = plt.figure(figsize=(24, 16))
plt.rc("font", **font)
left, bottom, width, height = 0.1, 0.1, 0.8, 0.8
ax = fig.add_axes([left, bottom, width, height])
cp = plt.contourf(grid_x, grid_y, zz, [0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0])
plt.colorbar(cp)
CS = plt.contour(
    grid_x, grid_y, zz, [0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0], colors="black"
)
ax.clabel(CS, inline=1, colors="black")
ax.set_title(
    "Fraction of positions, with a given move number and score, leading to a win"
)
ax.set_xlabel("score")
ax.set_ylabel("move")
ax.yaxis.grid(True)
ax.xaxis.grid(True)
plt.show()
plt.close()
