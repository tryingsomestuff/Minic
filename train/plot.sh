#!/bin/bash

cat logs/ordo.out | grep -Ev PLAYER\|advan\|oppo | awk '{print $2 " " $4 " " $5}' > col.out

cat col.out

python3 plot.py
