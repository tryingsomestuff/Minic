from subprocess import Popen, PIPE
import multiprocessing
import os
import argparse
import numpy as np
import matplotlib.pyplot as plt

class Engine():

    def __init__(self, filename, debug=False):
        self.engine = Popen(
            [filename], stdin=PIPE, stdout=PIPE, encoding="utf8", shell=True)

        self.debug = debug
        self.uci_isready()

    def writeline(self, line):
        self.engine.stdin.write(line)
        self.engine.stdin.flush()
        if self.debug:
            print ('--> [{}] {}'.format(self.engine.pid, line.rstrip()), flush=True)

    def readline(self):
        line = self.engine.stdout.readline()
        if self.debug:
            print ('<-- [{}] {}'.format(self.engine.pid, line.rstrip()), flush=True)
        return line.rstrip()

    def uci_isready(self):
        self.writeline('isready\n')
        while 'readyok' not in self.readline():
            pass

    def uci_set_option(self, name, value):
        self.uci_isready()
        self.writeline('setoption name {} value {}\n'.format(name, value))
        self.uci_isready()

    def uci_ucinewgame(self):
        self.uci_isready()
        self.writeline('ucinewgame\n')
        self.uci_isready()

    def uci_search(self, fen, depth, *moves):

        if len(moves) == 0: self.writeline('position fen {}\n'.format(fen))
        else: self.writeline('position fen {} moves {}\n'.format(fen, ' '.join(moves)))
        self.writeline('go depth {}\n'.format(depth))

        outputs = []
        while True:
            line = self.readline()
            if line.startswith('bestmove'):
                return outputs
            outputs.append(line)




parser = argparse.ArgumentParser(description='Detecting scaling performance')
parser.add_argument('-a', '--plot',         action='store_true' ,
                    help='will plot using pyplot')
parser.add_argument('-m', '--maxthreads',   type=int            , default=os.cpu_count(),
                    help=f'how many threads will be used at max, default={os.cpu_count()}')
parser.add_argument('-d', '--depth',        type=int            , default=20,
                    help='the depth which should be used for each position, default=20')
parser.add_argument('-g', '--hash',         type=int            , default=128,
                    help='the hash which should be given to the engine, default=128')
parser.add_argument('-e', '--engines',      type=str            , nargs='+',
                    help='a list of engines to check')


args = parser.parse_args()


maxthreads = args.maxthreads
depth      = args.depth
hash       = args.hash
engines    = args.engines
plot       = args.plot

positions  = ["r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
              "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
              "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
              "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
              "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
              "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
              "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
              "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
              "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
              "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37"]

if not engines or engines and len(engines) == 0:
    parser.error('Atleast one engine needs to be specified')

# create thr processes and the speeds
procs = {}
speeds = {}
for engine in engines:
    procs[engine] = Engine(engine)
    procs[engine].uci_set_option('Hash', hash)
    speeds[engine] = []


for threads in range(1, maxthreads+1):

    for engine in engines:

        # get the process
        proc = procs[engine]
        proc.uci_set_option('Threads', threads)

        # clear hash
        proc.uci_ucinewgame()
        proc.uci_isready()

        # add up total nps
        total_nps = 0

        # search on all the positions
        for position in positions:

            outputs = proc.uci_search(fen=position, depth=depth)
            if len(outputs) > 0 and 'nps' in outputs[-1]:
                s = outputs[-1].split(' ')
                nps = int(s[s.index('nps') + 1])
            # increment total nps
            total_nps += nps

            # wait for the engine
            proc.uci_isready()
        print(total_nps / len(positions), flush=True)
        speeds[engine] += [total_nps / len(positions)]

x_range = range(1, maxthreads+1)
plt.grid()
plt.plot(x_range, x_range)
for engine in engines:
    plt.plot(x_range, [x / speeds[engine][0] for x in speeds[engine]])

#plt.show()
plt.savefig("scaling.png")