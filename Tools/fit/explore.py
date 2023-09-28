import json
import matplotlib.pyplot as plt
import numpy as np

with open("log.txt") as file:
    lines = [line.rstrip() for line in file]

    data = []

    it = -1
    score = 0
    params = {}

    for line in lines:
        if "Starting iteration" in line:
            if it != -1:
                data.append({"it": it, "score": score, "params": params})
            it = int(line.split()[-1])
        if "Testing" in line:
            params = json.loads("{" + line.split("{")[-1].split("}")[0].replace("'","\"") + "}")
        if "Got Elo" in line:
            score = line.split(":")[-1]

    staticNullMoveSlopeDepth0 = []
    staticNullMoveSlopeGP0 = []
    staticNullMoveSlopeHeight0 = []
    staticNullMoveInit0 = []
    staticNullMoveMaxDepth1 = []
    staticNullMoveBonus0 = []

    for d in data:
        if float(d["score"].split("+")[0]) - float(d["score"].split("-")[-1]) > 0:
            print(d["params"])
            staticNullMoveSlopeDepth0 .append(d["params"]["staticNullMoveSlopeDepth0"])
            staticNullMoveSlopeGP0    .append(d["params"]["staticNullMoveSlopeGP0"])
            staticNullMoveSlopeHeight0.append(d["params"]["staticNullMoveSlopeHeight0"])
            staticNullMoveInit0       .append(d["params"]["staticNullMoveInit0"])
            staticNullMoveBonus0      .append(d["params"]["staticNullMoveBonus0"])

    for x in [staticNullMoveSlopeDepth0, staticNullMoveSlopeGP0, staticNullMoveSlopeHeight0, staticNullMoveInit0, staticNullMoveBonus0]:
        counts, bins = np.histogram(x)
        plt.stairs(counts, bins)
        plt.show()
