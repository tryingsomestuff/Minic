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

plt.errorbar(X, Y, yerr=err, fmt='-.k')
plt.show()

