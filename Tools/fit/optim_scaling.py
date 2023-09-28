import re
import os
import time
import argparse
import threading
import subprocess
import signal
import json

nb_best_print = 15
nb_best_test = 15
nb_tested_config = 3

time_control='3+0.1'
to_be_tuned='probCutMaxMoves'
test_range = range(1,10,1)
fixed_options = {}

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
   
    # game option
    command = "{} -each tc={} -games 72 -rounds 2 -concurrency {}".format(
        c_chess_exe, time_control, concurrency
    )
    command = (
        command
        + " -openings file={} order=random -repeat -resign count=3 score=700 -draw number=40 count=8 score=10".format(
            book_file_name
        )
    )

    # reference engine options
    command = command + " -engine cmd=/home/vivien/Minic/Dist/Minic3/minic_dev_linux_x64 name=master"
    #for param,value in fixed_options.items():
    #    command = (command + " option.{}={}").format(param, value)

    # tuned engines options
    count = 0
    for config in best:
        print(config)
        command = command + " -engine cmd={} name={} option.{}={}".format(
            engine, config, to_be_tuned, config.split('-',1)[1]
        )
        for param,value in fixed_options.items():
            command = (command + " option.{}={}").format(param, value)        

        count +=1
        if count >= nb_tested_config:
            break

    # output option
    pgn_file_name = os.path.join("out.pgn")
    c_chess_out_file_name = os.path.join("c_chess.out")
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

    configs = [ "config-{}".format(k) for k in test_range ]
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
        default=7,
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
        default="/home/vivien/Ordo/ordo",
        help="Path to ordo, see https://github.com/michiguel/Ordo",
    )
    parser.add_argument(
        "--c_chess_exe",
        type=str,
        default="/home/vivien/c-chess-cli/c-chess-cli",
        help="Path to c-chess-cli, see https://github.com/lucasart/c-chess-cli",
    )
    parser.add_argument(
        "--engine",
        type=str,
        default="/home/vivien/Minic/Tourney/minic_dev_linux_x64",
        help="Path to engine, see https://github.com/tryingsomestuff/Minic",
    )
    parser.add_argument(
        "--book_file_name",
        type=str,
        default="/home/vivien/Minic/Book_and_Test/OpeningBook/8moves_last.epd",
        help="Path to a suitable book, see https://github.com/tryingsomestuff/Minic-Book_and_Test/tree/HEAD/OpeningBook",
    )
    parser.add_argument(
        "--time_control",
        type=str,
        default="3+0.1",
        help="time control used, in the for XX+YY in secondes",
    )
    parser.add_argument(
        "--parameter",
        type=str,
        default="probCutMaxMoves",
        help="parameter to be optimized",
    )
    parser.add_argument(
        "--range_min",
        default=-500,
        type=int,
        help="range min",
    )
    parser.add_argument(
        "--range_max",
        default=500,
        type=int,
        help="range max",
    )
    parser.add_argument(
        "--range_step",
        default=10,
        type=int,
        help="range step",
    )
    parser.add_argument(
        "--json_config",
        default="tuning.json",
        type=str,
        help="tuning json file in the format of chess tuning tool, only ['engines'][0]['fixed_parameters'] object matters",
    )

    args = parser.parse_args()

    global test_range
    global to_be_tuned
    global time_control
    global fixed_options
    
    test_range = range(args.range_min,args.range_max,args.range_step)
    to_be_tuned = args.parameter
    time_control = args.time_control
    with open(args.json_config) as jsonf:
        jsondata = json.load(jsonf)
        fixed_options = jsondata['engines'][0]['fixed_parameters']
    
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
