import matplotlib.pyplot as plt

X, Y ,err = [], [], []
for line in open('col.out', 'r'):
  if line.isspace() :
      continue
  values = [s for s in line.split()]
  #print(values)
  #logs/nn-epoch28.nnue
  l = values[0].strip().replace(' ', '').replace('logs/nn-epoch', '').replace('.nnue', '')
  #print(l)
  if l == "master":
      l='0'
      continue
  e = values[2]
  if e == "----":
      e='0'
  X.append(float(l))
  Y.append(float(values[1]))
  err.append(float(e))

X, Y, err = zip(*sorted(zip(X, Y, err)))

plt.errorbar(X, Y, yerr=err, fmt='-.k', marker=None)
plt.scatter(X, Y, c=['green' if x>0 else 'black' for x in Y], marker='o')
plt.axhline(y=0, color='r', linestyle='-')
plt.axhline(y=20, color='b', linestyle='dotted')
plt.axhline(y=40, color='g', linestyle='dashed')
plt.show(block=False)
plt.pause(30)
plt.savefig('elo.png')
plt.close()
