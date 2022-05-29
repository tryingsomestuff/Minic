import chess.pgn
import matplotlib.pyplot as plt

incr = 0.1
initialTC = 10

def getData(engine,data):

   print("Engine : " + engine)

   pgn = open("test.pgn")
   f = chess.pgn.read_game(pgn)
   a=[]
   c = 0
   found = False
   while f:
      if c%100 == 0:
          print('.')
      c+=1
      #print(f.headers["White"])
      #print(f.headers["Black"])
      if engine in f.headers["White"]:
         #print("white")
         found = True
         n = f.variations[0]
         tt=[]
         p=0
         while n.variations:
            n = n.variations[0]
            p+=1
            if p%2 == 1:
                continue
            l = n.comment.split(' ')
            if len(l) > 1:
               try:
                  tt.append(float(l[1].replace('s','').replace(',','')))
               except:
                  print("error treating " + ' '.join(l))
         #print(tt)
         a.append(tt)
      if engine in f.headers["Black"]:
         #print("black")
         found = True
         n = f.variations[0]
         tt=[]
         p=0
         while n.variations:
            n = n.variations[0]
            p+=1
            if p%2 == 0:
                continue
            l = n.comment.split(' ')
            if len(l) > 1:
               try:
                  tt.append(float(l[1].replace('s','').replace(',','')))
               except:
                  print("error treating " + ' '.join(l))
         #print(tt)
         a.append(tt)         
      f = chess.pgn.read_game(pgn)

   #print(a)

   data[engine] = {}
   data[engine]['times'] = [0] * 1000
   counts = [0] * 1000
   for g in a:
      for n,t in enumerate(g):
         #print(n)
         #print(t)
         data[engine]['times'][n] += t
         counts[n] += 1

   #print(data[engine]['times'])
   #print(counts)

   for n,t in enumerate(data[engine]['times']):
      if counts[n] != 0:
         data[engine]['times'][n] /= counts[n]

   data[engine]['remaining'] = [0] * 1000
   data[engine]['remaining'][0] = initialTC 

   for n,t in enumerate(data[engine]['times']):
      if n > 0:
         data[engine]['remaining'][n] = data[engine]['remaining'][n-1] - data[engine]['times'][n] + incr
      if counts[n] == 0:
         break

   return found

data = {}
engines = ["minic_dev", "minic_dev_dev", "minic_3.21"]
#"Halogen", "Genie", "berserk", "seer", "weiss", "xiphos", "BlackMarlin", "stash", "rofChade", "Drofa", "stockfish"
cycler = plt.cycler(linestyle=['-', '--', ':', '-.']) * plt.cycler(color=['r', 'g', 'b'])
plt.rc('axes', prop_cycle=cycler)
engines_present = []
for e in engines:
   if getData(e, data):
      plt.plot([x for x in data[e]['times'] if x])
      engines_present.append(e)
plt.legend(engines_present)   
plt.savefig('time.png')
plt.show()

for e in engines_present:
   plt.plot([x for x in data[e]['remaining'][:200] if x])
plt.legend(engines_present)   
plt.savefig('rtime.png')
plt.show()
