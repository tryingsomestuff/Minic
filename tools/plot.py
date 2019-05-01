import matplotlib as mpl
#mpl.use('Agg')
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cbook as cbook


def read_datafile(file_name):
    data = np.genfromtxt(file_name, delimiter=';', names=['tmp','it','p','n','b','r','q','pe','ne','be','re','qe','e'])
    #data = np.genfromtxt(file_name, delimiter=';', names=['it','it2','n','b','r','q','pe','ne','be','re','qe','err'])
    #data = np.genfromtxt(file_name, delimiter=';', names=['it','n','b','r','q','pe','ne','be','re','qe'])
    return data

data = read_datafile('tuning.csv')

p = data['p']
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

x = range(0,len(e))

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.plot(x,p,linestyle=":")
ax1.plot(x,n,linestyle=":")
ax1.plot(x,b,linestyle=":")
ax1.plot(x,r,linestyle=":")
ax1.plot(x,q,linestyle=":")
ax1.plot(x,pe)
ax1.plot(x,ne)
ax1.plot(x,be)
ax1.plot(x,re)
ax1.plot(x,qe)
ax1.plot(x,e)
leg = ax1.legend(['p','n','b','r','q','pe','ne','be','re','qe','e'])
#leg = ax1.legend(['n','b','r','q','pe','ne','be','re','qe'])
plt.savefig("tuning.png")
plt.show()
