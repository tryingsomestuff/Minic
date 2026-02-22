import subprocess
import os
import sys
import pytest

DIR = os.path.dirname(os.path.realpath(__file__))
EXE = DIR + '/../Tourney/minic_dev_linux_x64'

def run_program(input, *args):
    print("Running {} with args: {}".format(EXE, args))
    return subprocess.run(
        [EXE] + list(*args),
        input=input,
        capture_output=True,
    )

# TODO : TBprobe, KPKprobe eval/evalHCE 

@pytest.mark.parametrize("input,args,want", [
    pytest.param(
        b"", 
        ["-perft_test"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="perft_test"),
    pytest.param(
        b"", 
        ["-see_test"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="see_test"),
    pytest.param(
        b"uci\nisready\ngo depth 10\nwait\nquit\n", 
        ["-uci"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="uci_test"),
    pytest.param(
        b"", 
        ["-spsa"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="spsa_test"),
    pytest.param(
        b"", 
        ["-timeTest", "1000", "0", "20", "0"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="time_test"),
    pytest.param(
        b"", 
        ["-qsearch", "start"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="qsearch_test"),
    pytest.param(
        b"", 
        ["-eval", "start"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="eval_test"),
    pytest.param(
        b"", 
        ["-gen", "start"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="gen_test"),
    pytest.param(
        b"", 
        ["-see", "start", "e2e4", "0"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="see_cli_test"),
    pytest.param(
        b"", 
        ["-kpk", "8/8/8/8/8/8/4k1P1/6K1 w - - 0 1"], 
        {
           "returncode": 0,
           "stdout": "",
           "stderr": "",
        }, 
        id="kpk_test")
])
def test_program(input, args, want):
    completed_process = run_program(input, args)
    got = {
        "returncode": completed_process.returncode,
        "stdout": completed_process.stdout,
        "stderr": completed_process.stderr,
    }
    print(got["stdout"].decode('utf-8'))
    #print(got["stderr"].decode('utf-8'))
    assert got["returncode"] == want["returncode"]
