import matplotlib as mpl
mpl.use('Agg')
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cbook as cbook


def read_datafile(file_name):
    #data = np.genfromtxt(file_name, delimiter=';', names=['it','p','n','b','r','q','pe','ne','be','re','qe','e'])
    data = np.genfromtxt(file_name, delimiter=';', names=['it','n','b','r','q','pe','ne','be','re','qe','e'])
    return data

data = read_datafile('tuning.csv')

x = data['it']
#p = data['p']
n = data['n']
b = data['b']
r = data['r']
q = data['q']
pe = data['pe']
ne = data['ne']
be = data['be']
re = data['re']
qe = data['qe']
e = data['e']

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(x,e)
leg = ax1.legend(['e'])
plt.savefig("tuning_e.png")
#plt.show()
