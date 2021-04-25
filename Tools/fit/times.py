import chess.pgn
import matplotlib.pyplot as plt

def getData(engine,data):

   print("Engine : " + engine)

   pgn = open("test.pgn")
   f = chess.pgn.read_game(pgn)
   a=[]
   c = 0
   while f:
      if c%100 is 0:
          print('.')
      c+=1
      #print(f.headers["White"])
      #print(f.headers["Black"])
      if e in f.headers["White"]:
         n = f.variations[0]
         tt=[]
         p=0
         while n.variations:
            n = n.variations[0]
            p+=1
            if p%2 is 0:
                continue
            l = n.comment.split(' ')
            if len(l) > 1:
               try:
                  tt.append(float(l[1].replace('s','').replace(',','')))
               except:
                  print("error treating " + ' '.join(l))
         #print(tt)
         a.append(tt)
      if e in f.headers["Black"]:
         n = f.variations[0]
         tt=[]
         p=0
         while n.variations:
            n = n.variations[0]
            p+=1
            if p%2 is 1:
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
      if counts[n]:
         data[engine]['times'][n] /= counts[n]

   data[engine]['remaining'] = [0] * 1000
   data[engine]['remaining'][0] = 30 

   for n,t in enumerate(data[engine]['times']):
      if n > 0:
         data[engine]['remaining'][n] = data[engine]['remaining'][n-1] - data[engine]['times'][n] + 0.3
      if counts[n] is 0:
         break

data = {}
engines = ["igel", "minic_2.15", "minic_2.12", "texel", "Rubi", "demolito", "Winter", "Topple", "PeSTO", "rodent", "Vajolet"]
for e in engines:
   getData(e, data)
   plt.plot([x for x in data[e]['times'] if x])
plt.legend(engines)   
plt.savefig('time.png')
plt.show()

for e in engines:
   plt.plot([x for x in data[e]['remaining'][:100] if x])
plt.legend(engines)   
plt.savefig('rtime.png')
plt.show()
