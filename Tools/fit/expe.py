import json
import os
import copy
import threading
import subprocess
import signal
import shutil

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

best_params = {}

use_chess_tuning_tool = True

# read all param to be tuned and theirs ranges
with open("params.json") as f:
    all = json.load(f)

    # read tuning comfig model
    with open("simple_tune.json") as ff:
        exp_data = json.load(ff)

        # optim loops
        for loop in range(1, 100):

            # parameters loop
            for param,value in all['parameter_ranges'].items():

                print(param)
                print(value)

                dir=os.path.join("loop" + str(loop), param)
                if os.path.isdir(dir):
                    print("Skipping already done dir")
                else:
                    # build uci params (fixed, and the one to be tuned) and write them
                    exp_data['parameter_ranges'] = {param: value}
                    fixed = best_params
                    if param in fixed:
                        fixed.pop(param)
                    exp_data['engines'][0]['fixed_parameters'].update(fixed)
                    exp_data['engines'][1]['fixed_parameters'].update(fixed)
                    with open("tuning.json", "w") as outfile:
                        json.dump(exp_data, outfile)

                    print("Running experiment")

                    # run chess-tuning-tools
                    if use_chess_tuning_tool:
                        Command("tune local -c tuning.json").run(timeout=12000) 
                        os.makedirs(dir, exist_ok=True)
                        os.rename("log.txt", os.path.join(dir, "log.txt"))
                        os.rename("out.pgn", os.path.join(dir, "out.pgn"))
                        os.remove("model.pkl")
                        os.remove("data.npz")

                    # or use naive tuning tool
                    else:
                        rmin = int(value.split('(')[1].split(',')[0])
                        rmax = int(value.split('(')[1].split(',')[1].split(')')[0])
                        # TODO adaptative boundaries if dir already exists (refinement)
                        rstep = int(max(1, (rmax-rmin)/15))
                        print('rmin', rmin)
                        print('rmax', rmax)
                        print('rstep', rstep)
                        # TODO find TC, book and concurrency in exp_data
                        cmd = "python3 ../Tools/fit/optim_scaling.py --engine ./minic_dev_linux_x64 --time_control \"3+0.05\" --parameter \"{}\" --range_min {} --range_max {} --range_step {} --json_config tuning.json --concurrency 34 --book_file_name /home/vivien/Minic/Book_and_Test/OpeningBook/4moves_noob.epd".format(param, rmin, rmax, rstep)
                        Command(cmd).run(timeout=7200) 
                        os.makedirs(dir, exist_ok=True)
                        os.rename("ordo.out", os.path.join(dir, "ordo.out"))
                        os.rename("out.pgn", os.path.join(dir, "out.pgn"))
                        os.remove("c_chess.out")

                # get the best configuration
                optim = None # {'nullMoveMargin': 33}
                elo = None
                if use_chess_tuning_tool:
                    with open(os.path.join(dir, "log.txt")) as log:
                        lines = [line.rstrip() for line in log]
                        for l in range(len(lines)-2):
                            if "Current optimum" in lines[l]:
                                optim = lines[l+1]
                                elo = lines[l+2].split(':')[-1]
                else:
                    with open(os.path.join(dir, "ordo.out")) as log:
                        lines = [line.rstrip() for line in log]
                        for l in range(len(lines)-2):
                            if "PLAYER" in lines[l]:
                                conf_name = list(filter(None, lines[l+1].split(' ')))[1]
                                print(conf_name)
                                if not "master" in conf_name:
                                    optim = '{"' + param + '": ' + conf_name.replace('config-','') + '}'
                                    elo = list(filter(None, lines[l+1].split(' ')))[3]
                                else:
                                    conf_name = list(filter(None, lines[l+2].split(' ')))[1]
                                    print(conf_name)
                                    optim = '{"' + param + '": ' + conf_name.replace('config-','') + '}'
                                    elo = list(filter(None, lines[l+2].split(' ')))[3]

                # update best parameters
                print(optim)
                print(elo)
                best_params.update(json.loads(optim.replace('\'','\"')))
