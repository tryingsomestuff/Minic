import chess.pgn
import matplotlib.pyplot as plt
from random import sample

pgn = open("/home/vivien/Téléchargements/test.pgn")
f = chess.pgn.read_game(pgn)
moobs=[]
c = 0
while f:
   if c%100 == 0:
         print('.')
   c+=1
   #if c == 100:
   #   break
   n = f.variations[0]
   scores=[]
   p = 0
   while n.variations:
      n = n.variations[0]
      l = n.comment.split(' ')
      if len(l) > 1:
         l = l[0].split('/')
         if l[0] != "book" and not 'M' in l[0]:
            try:
               scores.append(float(l[0]) if p%2==0 else -float(l[0]))
            except:
               print("error treating " + ' '.join(l))
      p += 1
   moobs.append(scores)
   f = chess.pgn.read_game(pgn)

#print(moobs)

type_w2d = []
type_b2d = [] 
type_a2d = []
type_w2b = []
type_b2w = []

for sl in moobs:
   maxi = 0
   mini = 0
   for s in sl:
      if s > maxi:
         maxi = s
      if s < mini:
         mini = s
   last = sl[-1]
   if abs(last) < 0.5:
      if mini < -3:
         type_b2d.append(sl)
      if maxi > 3:
         type_w2d.append(sl)
      if mini < -3 and maxi > 3:
         type_a2d.append(sl)
   else:
      if last > 3:
         if mini < -3:
            type_b2w.append(sl)
      if last < -3:
         if maxi > 3:
            type_w2b.append(sl)

n = len(moobs)
missedw = len(type_w2d)
missedb = len(type_b2d)
doublemissed = len(type_a2d)
blunderw = len(type_w2b)
blunderb = len(type_b2w)

print("missed  w   {:.2f} percent ({}/{})".format((100*missedw/n),missedw,n))
print("missed  b   {:.2f} percent ({}/{})".format((100*missedb/n),missedb,n))
print("both missed {:.2f} percent ({}/{})".format((100*doublemissed/n),doublemissed,n))
print("blunder w   {:.2f} percent ({}/{})".format((100*blunderw/n),blunderw,n))
print("blunder b   {:.2f} percent ({}/{})".format((100*blunderb/n),blunderb,n))

#print("w2d")
#print(type_w2d)
#print("b2d")
#print(type_b2d)
#print("w2b")
#print(type_w2b)
#print("b2w")
#print(type_b2w)

plt.title("moobs w2d")
for y in sample(type_w2d,10):
    plt.plot(y)
plt.legend()
plt.savefig("w2d.png")
plt.clf()

plt.title("moobs b2d")
for y in sample(type_b2d,10):
    plt.plot(y)
plt.legend()
plt.savefig("b2d.png")
plt.clf()

plt.title("moobs both")
for y in sample(type_a2d,10):
    plt.plot(y)
plt.legend()
plt.savefig("a2d.png")
plt.clf()


plt.title("moobs w2b")
for y in sample(type_w2b,10):
    plt.plot(y)
plt.legend()
plt.savefig("w2b.png")
plt.clf()

plt.title("moobs b2w")
for y in sample(type_b2w,10):
    plt.plot(y)
plt.legend()
plt.savefig("b2w.png")
plt.clf()