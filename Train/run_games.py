import re
import os
import time
import argparse
import threading
import subprocess
import signal


nb_best_print = 15
nb_best_test = 15
nb_tested_net = 3

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

def convert_ckpt(root_dir):
    """ Find the list of checkpoints that are available, and convert those that have no matching .nnue """
    # lightning_logs/version_0/checkpoints/epoch=3.ckpt
    p = re.compile("epoch=[0-9]*.*.ckpt")
    ckpts = []
    for path, subdirs, files in os.walk(root_dir, followlinks=False):
        for filename in files:
            #print(filename)
            m = p.match(filename)
            if m:
                ckpts.append(os.path.join(path, filename))

    # lets move the .nnue files a bit up in the tree, and get rid of the = sign.
    # lightning_logs/version_0/checkpoints/epoch=3.ckpt -> nn-epoch3.nnue
    for ckpt in ckpts:
        nnue_file_name = re.sub("lightning_logs/version_[0-9]+/checkpoints/", "", ckpt)
        nnue_file_name = re.sub("epoch=", "nn-epoch", nnue_file_name)
        nnue_file_name = re.sub("-step=[0-9]*", "", nnue_file_name)
        nnue_file_name = re.sub(".ckpt", ".nnue", nnue_file_name)
        if not os.path.exists(nnue_file_name):
            command = "python3 serialize.py {} {} ".format(ckpt, nnue_file_name)
            ret = Command(command).run(timeout=600)
            if ret != 0:
                raise ValueError("Error serializing!")


def find_nnue(root_dir):
    """ Find the set of nnue nets that are available for testing, going through the full subtree """
    p = re.compile("nn-epoch[0-9]*.nnue")
    nnues = []
    for path, subdirs, files in os.walk(root_dir, followlinks=False):
        for filename in files:
            m = p.match(filename)
            if m:
                nnues.append(os.path.join(path, filename))
    return nnues


def parse_ordo(root_dir, nnues):
    """ Parse an ordo output file for rating and error """
    ordo_file_name = os.path.join(root_dir, "ordo.out")
    ordo_scores = {}
    for name in nnues:
        ordo_scores[name] = (-1000, 3000)

    if os.path.exists(ordo_file_name):
        ordo_file = open(ordo_file_name, "r")
        lines = ordo_file.readlines()
        for line in lines:
            if "nn-epoch" in line:
                fields = line.split()
                net = fields[1]
                rating = float(fields[3])
                error = float(fields[4])
                ordo_scores[net] = (rating, error)

    return ordo_scores


def run_match(best, root_dir, c_chess_exe, concurrency, book_file_name, engine):
    """ Run a match using c-chess-cli adding pgns to a file to be analysed with ordo """
    pgn_file_name = os.path.join(root_dir, "out.pgn")
    c_chess_out_file_name = os.path.join(root_dir, "c_chess.out")
    command = "{} -each tc=3+0.1 -games 10 -rounds 2 -concurrency {}".format(
        c_chess_exe, concurrency
    )
    command = (
        command
        + " -openings file={} order=random -repeat -resign count=6 score=900 -draw number=40 count=8 score=10".format(
            book_file_name
        )
    )
    command = command + " -engine cmd=/ssd2/Minic/Dist/Minic3/3.42/minic_3.42_linux_x64_skylake name=master"

    count = 0
    for net in best:
        if os.path.exists(net) :
            command = command + " -engine cmd={} name={} option.NNUEFile={}".format(
                engine, net, os.path.join(os.getcwd(), net)
            )
            count +=1
            if count >= nb_tested_net:
                break
        else:
            print("skip absent net file {}".format(net))

    command = command + " -pgn {} 0 > {} 2>&1".format(
        pgn_file_name, c_chess_out_file_name
    )

    print("Running match ... {}".format(command), flush=True)
    ret = os.system(command)
    if ret != 0:
        #raise ValueError("Error running match!")
        print("Error running match!")


def run_ordo(root_dir, ordo_exe, concurrency):
    """ run an ordo calculation on an existing pgn file """
    pgn_file_name = os.path.join(root_dir, "out.pgn")
    ordo_file_name = os.path.join(root_dir, "ordo.out")
    command = "{} -q -G -J -p {} -a 0.0 --anchor=master --draw-auto --white-auto -s 10 --cpus={} -o {}".format(
        ordo_exe, pgn_file_name, concurrency, ordo_file_name
    )

    print("Running ordo ranking ... {}".format(ordo_file_name), flush=True)
    ret = Command(command).run(timeout=20)
    if ret != 0:
        #raise ValueError("Error running ordo!")
        print("Error running ordo!")


def run_round(
    root_dir,
    explore_factor,
    ordo_exe,
    c_chess_exe,
    engine,
    book_file_name,
    concurrency,
):
    """ run a round of games, finding existing nets, analyze an ordo file to pick most suitable ones, run a round, and run ordo """

    # find and convert checkpoints to .nnue
    convert_ckpt(root_dir)

    # find a list of networks to test
    nnues = find_nnue(root_dir)
    if len(nnues) == 0:
        print("No .nnue files found in {}".format(root_dir))
        time.sleep(10)
        return
    else:
        print("Found {} nn-epoch*.nnue files".format(len(nnues)))

    # Get info from ordo data if that is around
    ordo_scores = parse_ordo(root_dir, nnues)

    # provide the top nets
    print("Best nets so far:")
    ordo_scores = dict(
        sorted(ordo_scores.items(), key=lambda item: item[1][0], reverse=True)
    )
    count = 0
    for net in ordo_scores:
        print("   {} : {} +- {}".format(net, ordo_scores[net][0], ordo_scores[net][1]))
        count += 1
        if count == nb_best_print:
            break

    # get top with error bar added, for further investigation
    print("Trying to measure {} nets in:".format(nb_tested_net))
    ordo_scores = dict(
        sorted(
            ordo_scores.items(),
            key=lambda item: item[1][0] + explore_factor * item[1][1],
            reverse=True,
        )
    )
    best = []
    for net in ordo_scores:
        print("   {} : {} +- {}".format(net, ordo_scores[net][0], ordo_scores[net][1]))
        best.append(net)
        if len(best) == nb_best_test:
            break

    # run these nets against master...
    run_match(best, root_dir, c_chess_exe, concurrency, book_file_name, engine)

    # and run a new ordo ranking for the games played so far
    run_ordo(root_dir, ordo_exe, concurrency)


def main():
    # basic setup
    parser = argparse.ArgumentParser(
        description="Finds the strongest .nnue / .ckpt in tree, playing games.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "root_dir",
        type=str,
        help="""The directory where to look, recursively, for .nnue or .ckpt.
                 This directory will be used to store additional files,
                 in particular the ranking (ordo.out)
                 and game results (out.pgn and c_chess.out).""",
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
        help="`expected_improvement = rating + explore_factor * error` is used to pick select the next networks to run.",
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
            args.root_dir,
            args.explore_factor,
            args.ordo_exe,
            args.c_chess_exe,
            args.engine,
            args.book_file_name,
            args.concurrency,
        )


if __name__ == "__main__":
    main()
