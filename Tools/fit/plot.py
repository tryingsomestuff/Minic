import matplotlib as mpl
#mpl.use('Agg')
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cbook as cbook

n=11

names=[ str(i) for i in range(0,n) ]

print(names)

def read_datafile(file_name):
    data = np.genfromtxt(file_name, delimiter=';', names=names)
    return data

data = read_datafile('tuning.csv')

x = range(0,len(data['0']))

fig = plt.figure()
ax1 = fig.add_subplot(111)
for i in range(1,n):
   ax1.plot(x,data[str(i)],linestyle=":")
plt.savefig("tuning.png")
plt.show()
