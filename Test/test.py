import subprocess
import os
import sys
import pytest

DIR = os.getcwd()
EXE = DIR + '/../Tourney/minic_dev_linux_x64'

def run_program(input, *args):
    print("Running {} with args: {}".format(EXE, args))
    return subprocess.run(
        [EXE] + list(*args),
        input=input,
        capture_output=True,
    )

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
        id="uci_test")
    
])
def test_program(input, args, want):
    completed_process = run_program(input, args)
    got = {
        "returncode": completed_process.returncode,
        "stdout": completed_process.stdout,
        "stderr": completed_process.stderr,
    }
    #print(got["stdout"])
    #print(got["stderr"])
    assert got["returncode"] == want["returncode"]
