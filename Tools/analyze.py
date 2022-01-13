import queue
from subprocess import Popen, PIPE, STDOUT
from multiprocessing import Process, Queue
import os,sys
import argparse
import time

# process
procs = []

# forced flush after print
def displ(l): 
    print(l, flush=True)

class Engine():

    def __init__(self, filename, debug=False):
        self.engine = Popen([filename], stdin=PIPE, stdout=PIPE, stderr=STDOUT, encoding="utf8", shell=True)
        self.debug = debug
        self.uci_isready()
        self.searching = False;
        self.output = None

    def writeline(self, line):
        self.engine.stdin.write(line)
        self.engine.stdin.flush()
        if self.debug:
            displ('info string --> [{}] {}'.format(self.engine.pid, line.rstrip()))

    def readline(self):
        line = self.engine.stdout.readline()
        if self.debug:
            displ('info string <-- [{}] {}'.format(self.engine.pid, line.rstrip()))
        return line.rstrip()

    def uci_uci(self):
        self.writeline('uci\n')
        # wait for uciok
        while True:
            l = self.readline()
            if 'uciok' in l:
                # uci ok will be sent by the proxy
                return
            else:
                #displ(l)
                pass

    def uci_isready(self):
        self.writeline('isready\n')
        # wait for isready
        while True:
            l = self.readline()
            if 'readyok' in l:
                # readyok ok will be sent by the proxy
                return
            else:
                #displ(l)
                pass

    def uci_set_option(self, line):
        self.uci_isready()
        self.writeline(line)
        self.uci_isready()

    def uci_ucinewgame(self):
        self.uci_isready()
        self.writeline('ucinewgame\n')
        self.uci_isready()

    def uci_quit(self):
        self.uci_isready()
        self.writeline('quit\n')
        self.searching = False

    def uci_search(self, line):
        self.searching = True
        self.writeline(line)
        # wait for bestmove, save everything else before that
        self.output = []
        while True:
            l = self.readline()
            self.output.append(l)
            #displ(l)
            if l.startswith('bestmove'):
                self.searching = False
                return

parser = argparse.ArgumentParser(description='XFEN Analyzer')
parser.add_argument('-e', '--engine', type=str, help='the engine to use')
parser.add_argument('-p', '--process', type=int, default=3, help='number of concurrent process to use')
parser.add_argument('-d', '--depth', type=int, default=15, help='analysis depth')
parser.add_argument('-i', '--input', type=str, help='XFEN file')
parser.add_argument('-t', '--timeout', type=int, default=40, help='a timeout that shall allow all process to start one by one')
args = parser.parse_args()

engine = args.engine
if not engine :
    parser.error('One engine needs to be specified')
input = args.input
if not input :
    parser.error('No input file')
process = max(1,min(args.process,16))
depth = max(1,min(40,args.depth))
timeout = max(10,min(600,args.timeout))

print('{} analyzing {} at depth {} using {} process (timeout {})'.format(engine, input, depth, process, timeout))

class P(Process):
    def __init__(self, id, engine, queue, timeout, debug):
        self.id = id
        self.queue = queue
        self.engine = Engine(engine, debug)
        self.engine.uci_uci()
        self.engine.uci_isready()
        self.depth = 15
        self.stop = False
        self.timeout = timeout
        super(P, self).__init__()

    def set_depth(self, depth):
        self.depth = depth

    def run(self):
        print('Running')

        self.outfile = open('output_{}'.format(self.id), 'w', buffering=1)

        while not self.stop:
            print('Waiting for fen ...')
            fen = self.queue.get(block=True, timeout=self.timeout).rstrip('\n')
            
            print("Analyzing {}".format(fen))
            self.engine.uci_isready()
            self.engine.uci_ucinewgame()
            self.engine.writeline('position fen {}\n'.format(fen))
            self.engine.uci_search('go depth {}\n'.format(self.depth))
            time.sleep(0.1)
            # synchronous wait on engine current search
            while self.engine.searching:
                time.sleep(0.1)
                pass
            out_str = '{} {}'.format(fen,self.engine.output[-2])
            print(out_str)
            self.outfile.write(out_str + '\n')
            print('Done')

            if self.queue.empty():
                break

        self.outfile.close()

    def quit(self):
        self.engine.uci_quit()
        self.stop = True

def run():
    # event queue
    queue = Queue()

    # create each engine process and start them
    for id in range(process):
        procs.append(P(id, engine, queue, timeout, False))
        procs[-1].set_depth(depth)
        procs[-1].start()

    # read file and fill queue
    with open(input, 'r') as fp:
        for fen in fp:
            fen = fen.replace('"','')
            print('Queuing fen {}'.format(fen))
            queue.put(fen)
            time.sleep(0.2) # not too fast ...

    # wait for queue to be empty
    while not queue.empty():
        print('Still working ...({})'.format(queue.qsize()))
        time.sleep(5)

    # quit the engine
    print('Quitting')
    for p in procs:
       p.quit()
       time.sleep(0.5)
       p.kill()

    print('Done')


if __name__ == '__main__':
    run()
