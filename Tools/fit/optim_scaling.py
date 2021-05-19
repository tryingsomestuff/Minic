import re
import os
import time
import argparse
import threading
import subprocess
import signal


nb_best_print = 15
nb_best_test = 15
nb_tested_config = 3

class Command(object):
    def __init__(self, cmd):
        self.cmd = cmd
        self.process = None

    def run(self, timeout):
        def target():
            self.process = subprocess.Popen(self.cmd, shell=True, preexec_fn=os.setsid)
            self.process.communicate()

        thread = threading.Thread(target=target)
        thread.start()

        thread.join(timeout)
        if thread.is_alive():
            os.killpg(self.process.pid, signal.SIGTERM)
            thread.join()
        return self.process.returncode

def parse_ordo(configs):
    """ Parse an ordo output file for rating and error """
    ordo_file_name = os.path.join("ordo.out")
    ordo_scores = {}
    for name in configs:
        ordo_scores[name] = (-500, 1000)

    if os.path.exists(ordo_file_name):
        ordo_file = open(ordo_file_name, "r")
        lines = ordo_file.readlines()
        for line in lines:
            if "config" in line:
                fields = line.split()
                net = fields[1]
                rating = float(fields[3])
                error = float(fields[4])
                ordo_scores[net] = (rating, error)

    return ordo_scores


def run_match(best, c_chess_exe, concurrency, book_file_name, engine):
    """ Run a match using c-chess-cli adding pgns to a file to be analysed with ordo """
    pgn_file_name = os.path.join("out.pgn")
    c_chess_out_file_name = os.path.join("c_chess.out")
    command = "{} -each tc=3+0.03 -games 10 -rounds 2 -concurrency {}".format(
        c_chess_exe, concurrency
    )
    command = (
        command
        + " -openings file={} order=random -repeat -resign 3 700 -draw 8 10".format(
            book_file_name
        )
    )
    command = command + " -engine cmd=/ssd/Minic/Dist/Minic3/minic_dev_linux_x64 name=master"
    #command = command + " -engine cmd=/ssd/engines/seer-nnue/build/seer name=master option.Hash=128"
    #command = command + " -engine cmd=/ssd/engines/Halogen/Halogen/src/Halogen name=master option.Hash=128"

    count = 0
    for config in best:
        command = command + " -engine cmd={} name={} option.NNUEscaling={}".format(
            engine, config, config.split('-')[1]
        )
        count +=1
        if count >= nb_tested_config:
            break

    command = command + " -pgn {} 0 > {} 2>&1".format(
        pgn_file_name, c_chess_out_file_name
    )

    print("Running match ... {}".format(command), flush=True)
    ret = os.system(command)
    if ret != 0:
        #raise ValueError("Error running match!")
        print("Error running match!")


def run_ordo(ordo_exe, concurrency):
    """ run an ordo calculation on an existing pgn file """
    pgn_file_name = os.path.join("out.pgn")
    ordo_file_name = os.path.join("ordo.out")
    command = "{} -q -G -J -p {} -a 0.0 --anchor=master --draw-auto --white-auto -s 100 --cpus={} -o {}".format(
        ordo_exe, pgn_file_name, concurrency, ordo_file_name
    )

    print("Running ordo ranking ... {}".format(ordo_file_name), flush=True)
    ret = Command(command).run(timeout=10)
    if ret != 0:
        #raise ValueError("Error running ordo!")
        print("Error running ordo!")


def run_round(
    explore_factor,
    ordo_exe,
    c_chess_exe,
    engine,
    book_file_name,
    concurrency,
):
    """ run a round of games, analyze an ordo file to pick most suitable ones, run a round, and run ordo """

    configs = [ "config-{}".format(k) for k in range(32,257,8) ]
    print(configs)

    # Get info from ordo data if that is around
    ordo_scores = parse_ordo(configs)

    # provide the top configs
    print("Best config so far:")
    ordo_scores = dict(
        sorted(ordo_scores.items(), key=lambda item: item[1][0], reverse=True)
    )
    count = 0
    for config in ordo_scores:
        print("   {} : {} +- {}".format(config, ordo_scores[config][0], ordo_scores[config][1]))
        count += 1
        if count == nb_best_print:
            break

    # get top with error bar added, for further investigation
    print("Trying to measure {} configs in:".format(nb_tested_config))
    ordo_scores = dict(
        sorted(
            ordo_scores.items(),
            key=lambda item: item[1][0] + explore_factor * item[1][1],
            reverse=True,
        )
    )
    best = []
    for config in ordo_scores:
        print("   {} : {} +- {}".format(config, ordo_scores[config][0], ordo_scores[config][1]))
        best.append(config)
        if len(best) == nb_best_test:
            break

    # run these configs against master...
    run_match(best, c_chess_exe, concurrency, book_file_name, engine)

    # and run a new ordo ranking for the games played so far
    run_ordo(ordo_exe, concurrency)


def main():
    # basic setup
    parser = argparse.ArgumentParser(
        description="Finds the strongest configs, playing games.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--concurrency",
        type=int,
        default=8,
        help="Number of concurrently running threads",
    )
    parser.add_argument(
        "--explore_factor",
        default=1.5,
        type=float,
        help="`expected_improvement = rating + explore_factor * error` is used to pick select the next configs to run.",
    )
    parser.add_argument(
        "--ordo_exe",
        type=str,
        default="./ordo",
        help="Path to ordo, see https://github.com/michiguel/Ordo",
    )
    parser.add_argument(
        "--c_chess_exe",
        type=str,
        default="./c-chess-cli",
        help="Path to c-chess-cli, see https://github.com/lucasart/c-chess-cli",
    )
    parser.add_argument(
        "--engine",
        type=str,
        default="./Dist/Minic3/minic_dev_linux_x64",
        help="Path to engine, see https://github.com/tryingsomestuff/Minic",
    )
    parser.add_argument(
        "--book_file_name",
        type=str,
        default="./noob_3moves.epd",
        help="Path to a suitable book, see https://github.com/tryingsomestuff/Minic-Book_and_Test/tree/HEAD/OpeningBook",
    )
    args = parser.parse_args()

    while True:
        run_round(
            args.explore_factor,
            args.ordo_exe,
            args.c_chess_exe,
            args.engine,
            args.book_file_name,
            args.concurrency,
        )


if __name__ == "__main__":
    main()
