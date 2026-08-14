#!/usr/bin/env python3
"""SPRT-gated local search tuner for Minic's WITH_SEARCH_TUNING UCI options.

This tunes the search parameters that Minic exposes over UCI when compiled
with `WITH_SEARCH_TUNING` (see the commented out `#define WITH_SEARCH_TUNING`
in Source/config.hpp). It plays each candidate change against the current
best known configuration using cutechess-cli's built-in `-sprt` termination
criterion, which implements the same sequential probability ratio test (SPRT)
used by chess engine testing frameworks such as OpenBench/Fishtest
(https://github.com/AndyGrant/OpenBench): the match is stopped as soon as the
log-likelihood ratio crosses the acceptance bound for H1 ("the change is an
improvement of at least elo0") or the rejection bound for H0 ("the change is
not an improvement of at least elo1").

Two sub-commands are provided:

  discover  Query an engine binary over UCI and dump every "spin" option to a
            JSON parameter file (value/min/max/step). Edit that file to keep
            only the parameters you actually want to tune (Hash, Threads,
            MultiPV, NNUE*, Style*, ... are not search-tuning knobs and should
            usually be removed).

  tune      Repeatedly pick one parameter, propose a +/-step change, and play
            an SPRT match (single binary, two `option.<name>=<value>` engine
            instances via cutechess-cli) to decide whether to keep it. State
            is checkpointed after every test so the process can be
            interrupted (Ctrl+C) and resumed later.

Each SPRT match can be run in one of two ways, via --executor:

  local  (default) One cutechess-cli process on this machine, playing
         -concurrency games in parallel; this is cutechess-cli's own native
         `-sprt` termination criterion.

  slurm  Distributes the games across --slurm-jobs parallel `sbatch` jobs
         (each an independent cutechess-cli process with its own
         -concurrency), in batches of --slurm-batch-games games. Results are
         read directly from each job's Slurm --output file (streamed live,
         and also once the job finishes); after every batch the *aggregated*
         W/L/D across all jobs and all batches so far is turned into an LLR
         (self-computed, same Wald SPRT bounds cutechess-cli itself uses) and
         checked against elo0/elo1 -- another batch is submitted if no
         decision has been reached and --rounds-cap hasn't been hit. This
         gives "massive concurrency" = --slurm-jobs x --concurrency games
         running at once, spread across whatever nodes Slurm schedules them
         on.

Example
-------
    # Build a binary with WITH_SEARCH_TUNING enabled first, then:
    python3 Tools/fit/sprt_tune.py discover ./Tourney/minic_dev_linux_x64
    # -> edit Tools/fit/sprt_tune_params.json to select parameters
    python3 Tools/fit/sprt_tune.py tune ./Tourney/minic_dev_linux_x64 \
        --concurrency 8 --tc 10+0.1
    # ...or distributed across a Slurm cluster:
    python3 Tools/fit/sprt_tune.py tune ./Tourney/minic_dev_linux_x64 \
        --executor slurm --slurm-jobs 16 --concurrency 8 --tc 10+0.1
"""

import argparse
import json
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

DEFAULT_PARAMS_FILE = Path(__file__).resolve().parent / "sprt_tune_params.json"
DEFAULT_STATE_FILE = Path(__file__).resolve().parent / "sprt_tune_state.json"
DEFAULT_BEST_FILE = Path(__file__).resolve().parent / "sprt_tune_best.json"
DEFAULT_LOG_FILE = Path(__file__).resolve().parent / "sprt_tune.log"
DEFAULT_STATUS_FILE = Path(__file__).resolve().parent / "sprt_tune_status.json"
DEFAULT_PGN_FILE = Path(__file__).resolve().parent / "sprt_tune_last.pgn"
DEFAULT_BOOK = REPO_ROOT / "Book_and_Test" / "4moves_noob.epd"
DEFAULT_CUTECHESS = "/home/vivien/cutechess-1.4.0/cutechess-cli"
DEFAULT_SLURM_WORK_DIR = Path(__file__).resolve().parent / "slurm_jobs"

SLURM_TERMINAL_STATES = (
    "COMPLETED", "FAILED", "CANCELLED", "TIMEOUT", "OUT_OF_MEMORY",
    "NODE_FAIL", "BOOT_FAIL", "DEADLINE", "PREEMPTED",
)

OPTION_RE = re.compile(
    r"^option name (?P<name>\S+) type (?P<type>\S+)"
    r"(?: default (?P<default>\S+))?"
    r"(?: min (?P<min>-?\d+))?"
    r"(?: max (?P<max>-?\d+))?"
)
SPRT_RE = re.compile(
    r"SPRT: llr\s+([-\d.eE]+)\s*\(([-\d.]+)%\),\s*lbound\s+([-\d.eE]+),"
    r"\s*ubound\s+([-\d.eE]+)(?:\s*-\s*(H0|H1) was accepted)?"
)
SCORE_RE = re.compile(
    r"Score of (\S+) vs (\S+): (\d+) - (\d+) - (\d+)\s+\[([\d.]+)\]\s+(\d+)"
)
ELO_RE = re.compile(
    r"Elo difference:\s*([-\d.]+)\s*\+/-\s*([\d.]+),\s*LOS:\s*([\d.]+)\s*%,"
    r"\s*DrawRatio:\s*([\d.]+)\s*%"
)


def timestamp():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def atomic_write_json(path, data):
    tmp = f"{path}.tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)
    os.replace(tmp, path)


# --------------------------------------------------------------------------- 
# Elo / SPRT math, self-computed from W/L/D counts (used by the 'slurm'
# executor, which aggregates results across several sbatch jobs and therefore
# can't rely on a single cutechess-cli process's own "-sprt" bookkeeping).
# ---------------------------------------------------------------------------

def score_stats(wins, losses, draws):
    """Return (score, standard_error_of_score) from W/L/D counts, using the
    per-game result variance (trinomial: win=1, draw=0.5, loss=0)."""
    n = wins + losses + draws
    if n == 0:
        return None, None
    w, l, d = wins / n, losses / n, draws / n
    score = w + d / 2.0
    if n <= 1:
        return score, None
    var = w * (1.0 - score) ** 2 + l * (0.0 - score) ** 2 + d * (0.5 - score) ** 2
    return score, math.sqrt(var / n)


def elo_and_sem(wins, losses, draws):
    """Convert W/L/D counts to (elo_diff, standard_error_in_elo). Either
    element may be None if it can't be estimated (e.g. 0 games, or a 0%/100%
    score for which the logistic Elo model is undefined)."""
    score, se_score = score_stats(wins, losses, draws)
    if score is None:
        return None, None
    if score <= 0.0:
        return -1000.0, None
    if score >= 1.0:
        return 1000.0, None
    elo = -400.0 * math.log10(1.0 / score - 1.0)
    if se_score is None:
        return elo, None
    deriv = 400.0 / math.log(10.0) / (score * (1.0 - score))
    return elo, deriv * se_score


def sprt_wald_bounds(alpha, beta):
    """Wald's SPRT decision bounds on the log-likelihood ratio; matches the
    lbound/ubound cutechess-cli itself prints for the same alpha/beta."""
    return math.log(beta / (1.0 - alpha)), math.log((1.0 - beta) / alpha)


def sprt_llr(score, mean_var, elo0, elo1):
    """Log-likelihood ratio of H1 (true elo == elo1) vs H0 (true elo == elo0)
    given an observed mean score with variance `mean_var` (i.e. the variance
    of the *sample mean*, such as score_stats()'s se_score ** 2), using the
    usual Gaussian approximation of the trinomial score distribution."""
    if not mean_var:
        return None
    mu0 = 1.0 / (1.0 + 10.0 ** (-elo0 / 400.0))
    mu1 = 1.0 / (1.0 + 10.0 ** (-elo1 / 400.0))
    return (mu1 - mu0) * (2.0 * score - mu0 - mu1) / (2.0 * mean_var)


# --------------------------------------------------------------------------- 
# Slurm executor plumbing (shared by run_match_slurm below). Jobs are plain
# sbatch scripts that just invoke cutechess-cli directly; sbatch's own
# --output file *is* the job's cutechess-cli log, so results can always be
# read straight from that file once (or while) the job runs.
# ---------------------------------------------------------------------------

def tail_new_lines(path, offsets):
    """Return any lines appended to `path` since the last call, tracked via
    the `offsets` dict (keyed by str(path)). Used to stream partial Slurm
    job output into --log while jobs are still running."""
    key = str(path)
    if not os.path.exists(path):
        return []
    with open(path, "r", errors="replace") as f:
        f.seek(offsets.get(key, 0))
        new_text = f.read()
        offsets[key] = f.tell()
    return new_text.splitlines(keepends=True)


def write_slurm_script(args, script_path, out_file, cutechess_argv):
    cpus = args.slurm_cpus_per_task or args.concurrency
    lines = [
        "#!/bin/bash",
        f"#SBATCH --job-name={script_path.stem}",
        f"#SBATCH --output={out_file}",
        f"#SBATCH --time={args.slurm_time}",
        f"#SBATCH --cpus-per-task={cpus}",
    ]
    if args.slurm_partition:
        lines.append(f"#SBATCH --partition={args.slurm_partition}")
    if args.slurm_account:
        lines.append(f"#SBATCH --account={args.slurm_account}")
    if args.slurm_extra:
        lines.extend(f"#SBATCH {tok}" for tok in shlex.split(args.slurm_extra))
    lines.append("")
    lines.append(" ".join(shlex.quote(a) for a in cutechess_argv))
    lines.append("")
    script_path.write_text("\n".join(lines))
    script_path.chmod(0o755)


def submit_slurm_job(script_path):
    result = subprocess.run(["sbatch", "--parsable", str(script_path)], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"sbatch failed for {script_path}: {result.stderr.strip()}")
    return result.stdout.strip().splitlines()[0].split(";")[0]


def slurm_job_states(job_ids):
    if not job_ids:
        return {}
    result = subprocess.run(
        ["sacct", "-j", ",".join(job_ids), "--format=JobID,State", "--noheader", "--parsable2"],
        capture_output=True, text=True,
    )
    states = {}
    if result.returncode == 0:
        for line in result.stdout.splitlines():
            parts = line.split("|")
            if len(parts) >= 2:
                jid = parts[0].split(".")[0]
                if jid in job_ids and jid not in states:
                    states[jid] = parts[1].strip()
    return states


def slurm_state_is_terminal(state):
    if not state:
        return False
    base = state.split()[0]
    return any(base.startswith(t) for t in SLURM_TERMINAL_STATES)


def scancel_jobs(job_ids):
    """Best-effort `scancel` of the given Slurm job IDs. Used to clean up
    orphaned sub-jobs when a batch is interrupted (Ctrl+C) or aborts with an
    unexpected error while jobs are still pending -- without this, those
    sbatch jobs would keep running/queued on the cluster indefinitely (until
    they finish naturally or hit --slurm-time), showing up in `sacct`/
    `squeue` long after this script has stopped."""
    job_ids = [j for j in job_ids if j]
    if not job_ids:
        return
    try:
        subprocess.run(["scancel", *job_ids], capture_output=True, text=True)
    except Exception:
        pass


# --------------------------------------------------------------------------- 
# discover
# ---------------------------------------------------------------------------

def discover_options(engine_path, timeout=10):
    """Query `engine_path` over UCI and return {name: {default,min,max}} for
    every 'spin' option it advertises."""
    proc = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    options = {}
    start = time.time()
    try:
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        while time.time() - start < timeout:
            line = proc.stdout.readline()
            if not line:
                break
            line = line.strip()
            m = OPTION_RE.match(line)
            if m and m.group("type") == "spin" and m.group("min") and m.group("max"):
                options[m.group("name")] = {
                    "default": int(m.group("default")),
                    "min": int(m.group("min")),
                    "max": int(m.group("max")),
                }
            if line == "uciok":
                break
        proc.stdin.write("quit\n")
        proc.stdin.flush()
    finally:
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
    return options


def cmd_discover(args):
    options = discover_options(args.engine)
    if not options:
        print(
            "No 'spin' UCI options were found. Make sure the engine binary was "
            "built with WITH_SEARCH_TUNING enabled (uncomment `#define "
            "WITH_SEARCH_TUNING` in Source/config.hpp and rebuild)."
        )
        return 1

    params = {}
    for name, o in options.items():
        span = o["max"] - o["min"]
        step = max(1, round(span / args.step_fraction))
        params[name] = {
            "value": o["default"],
            "min": o["min"],
            "max": o["max"],
            "step": step,
        }

    atomic_write_json(args.output, params)
    print(f"Discovered {len(params)} tunable 'spin' options -> {args.output}")
    print(
        "Edit this file to keep only the parameters you actually want to tune "
        "(Hash, Threads, MultiPV, NNUE*, Style*, Level, ... are not search-"
        "tuning knobs) before running the 'tune' sub-command."
    )
    return 0


# --------------------------------------------------------------------------- 
# tune
# ---------------------------------------------------------------------------

def load_state(state_path, params_path):
    if os.path.exists(state_path):
        with open(state_path) as f:
            return json.load(f), True

    if not os.path.exists(params_path):
        raise SystemExit(
            f"Parameter file {params_path} not found. Run the 'discover' "
            "sub-command first."
        )
    with open(params_path) as f:
        params_def = json.load(f)

    params = {}
    for name, d in params_def.items():
        vmin, vmax = int(d["min"]), int(d["max"])
        params[name] = {
            "value": int(d["value"]),
            "min": vmin,
            "max": vmax,
            "step": int(d.get("step", max(1, (vmax - vmin) // 40))),
            "min_step": int(d.get("min_step", 1)),
            "next_direction": 1,
            "last_fail_direction": None,
            "converged": False,
        }
    return {"params": params, "cursor": 0, "tests_run": 0}, False


def announce_session(args, state, order, resumed):
    """Print + log a clear banner so it's obvious whether this run resumed
    from an existing checkpoint or started fresh, and dump the current
    per-parameter values so the checkpoint contents are visible."""
    if resumed:
        header = (
            f"[{timestamp()}] Resuming from checkpoint {args.state} "
            f"({state.get('tests_run', 0)} tests already run, "
            f"cursor={state.get('cursor', 0)} / {len(order)} params)."
        )
    else:
        header = (
            f"[{timestamp()}] Starting a NEW tuning session "
            f"({len(order)} parameters) -> {args.state}"
        )

    lines = [header]
    for name in order:
        p = state["params"][name]
        lines.append(
            f"    {name}: value={p['value']} step={p['step']}"
            + (" [converged]" if p.get("converged") else "")
        )

    print("\n".join(lines), flush=True)
    with open(args.log, "a") as logf:
        logf.write("\n" + "\n".join(lines) + "\n")


def write_best_params(best_out, state):
    best = {name: p["value"] for name, p in state["params"].items()}
    atomic_write_json(best_out, best)


def build_option_args(params, override_name=None, override_value=None):
    opts = []
    for name, p in params.items():
        value = override_value if name == override_name else p["value"]
        opts.append(f"option.{name}={value}")
    return opts


def parse_result(text):
    result = {
        "status": "inconclusive",
        "llr": None,
        "wins": 0,
        "losses": 0,
        "draws": 0,
        "games": 0,
    }
    score_matches = SCORE_RE.findall(text)
    if score_matches:
        _, _, w, l, d, _, games = score_matches[-1]
        result["wins"], result["losses"], result["draws"], result["games"] = (
            int(w), int(l), int(d), int(games)
        )
    sprt_matches = SPRT_RE.findall(text)
    if sprt_matches:
        llr, _pct, _lb, _ub, decision = sprt_matches[-1]
        result["llr"] = float(llr)
        if decision == "H1":
            result["status"] = "accept"
        elif decision == "H0":
            result["status"] = "reject"
    if not score_matches and not sprt_matches:
        result["status"] = "error"
    return result


def run_match_local(args, param_name, base_value, candidate_value, candidate_opts, base_opts):
    engine_dir = os.path.dirname(os.path.abspath(args.engine)) or "."
    book = str(args.book)
    book_format = "epd" if book.lower().endswith(".epd") else "pgn"

    argv = [
        args.cutechess,
        "-engine", "name=CANDIDATE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
        *candidate_opts,
        "-engine", "name=BASE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
        *base_opts,
        "-each", f"tc={args.tc}", "timemargin=50",
        "-concurrency", str(args.concurrency),
        "-rounds", str(args.rounds_cap),
        "-repeat",
        "-sprt", f"elo0={args.elo0}", f"elo1={args.elo1}", f"alpha={args.alpha}", f"beta={args.beta}",
        "-openings", f"file={book}", f"format={book_format}", "order=random", "plies=100",
        "-recover",
        "-ratinginterval", str(args.rating_interval),
        "-pgnout", str(args.pgn_out),
    ]

    print(f"[{timestamp()}] Testing {param_name}: {base_value} -> {candidate_value}", flush=True)

    status = {
        "param": param_name,
        "base_value": base_value,
        "candidate_value": candidate_value,
        "started_at": timestamp(),
        "updated_at": timestamp(),
        "games": 0,
        "wins": 0,
        "losses": 0,
        "draws": 0,
        "score": None,
        "llr": None,
        "lbound": None,
        "ubound": None,
        "sprt_decision": None,
        "elo_diff": None,
        "elo_error": None,
        "los": None,
        "draw_ratio": None,
    }
    atomic_write_json(args.status, status)

    lines = []
    with open(args.log, "a") as logf:
        logf.write(f"\n=== {timestamp()} Testing {param_name}: {base_value} -> {candidate_value} ===\n")
        logf.write("$ " + " ".join(shlex.quote(a) for a in argv) + "\n")
        logf.flush()

        proc = subprocess.Popen(
            argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        try:
            for line in proc.stdout:
                lines.append(line)
                logf.write(line)
                logf.flush()

                m = SCORE_RE.search(line)
                if m:
                    _, _, w, l, d, score_pct, games = m.groups()
                    status["wins"], status["losses"], status["draws"] = int(w), int(l), int(d)
                    status["games"] = int(games)
                    status["score"] = float(score_pct)
                    status["updated_at"] = timestamp()
                    atomic_write_json(args.status, status)

                m = ELO_RE.search(line)
                if m:
                    diff, err, los, draw_ratio = m.groups()
                    status["elo_diff"] = float(diff)
                    status["elo_error"] = float(err)
                    status["los"] = float(los)
                    status["draw_ratio"] = float(draw_ratio)
                    status["updated_at"] = timestamp()
                    atomic_write_json(args.status, status)

                m = SPRT_RE.search(line)
                if m:
                    llr, _pct, lb, ub, decision = m.groups()
                    status["llr"] = float(llr)
                    status["lbound"] = float(lb)
                    status["ubound"] = float(ub)
                    status["sprt_decision"] = decision
                    status["updated_at"] = timestamp()
                    atomic_write_json(args.status, status)
                    elo_part = (
                        f", elo={status['elo_diff']:+.1f}+/-{status['elo_error']:.1f}"
                        if status["elo_diff"] is not None else ""
                    )
                    print(
                        f"    [{timestamp()}] {status['games']} games, "
                        f"+{status['wins']}-{status['losses']}={status['draws']}"
                        f"{elo_part}, llr={status['llr']:.3f} "
                        f"(bounds {status['lbound']:.2f}/{status['ubound']:.2f})",
                        flush=True,
                    )
        finally:
            proc.wait()

    status["finished_at"] = timestamp()
    status["returncode"] = proc.returncode
    atomic_write_json(args.status, status)
    return "".join(lines)


def run_match_slurm(args, param_name, base_value, candidate_value, candidate_opts, base_opts):
    """Slurm-distributed equivalent of run_match_local(): splits each SPRT
    decision-batch's games across --slurm-jobs parallel sbatch jobs (each
    running its own cutechess-cli with -concurrency games in parallel), polls
    their state + tails their --output files (which *are* the per-job
    cutechess-cli logs) until the batch finishes, aggregates W/L/D across all
    jobs and all batches played so far for this test, and checks the
    aggregated LLR against Wald's SPRT bounds after every batch -- submitting
    another batch if no decision has been reached yet and the rounds cap
    hasn't been hit. Returns a synthetic text blob in the same format
    run_match_local()/parse_result() already understand, so no other part of
    the tuning loop needs to change."""
    if shutil.which("sbatch") is None:
        msg = f"[{timestamp()}] ERROR: sbatch not found on PATH; cannot use --executor slurm."
        print(msg, flush=True)
        with open(args.log, "a") as logf:
            logf.write(msg + "\n")
        return ""

    engine_dir = os.path.dirname(os.path.abspath(args.engine)) or "."
    book = str(args.book)
    book_format = "epd" if book.lower().endswith(".epd") else "pgn"
    work_dir = Path(args.slurm_work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    lbound, ubound = sprt_wald_bounds(args.alpha, args.beta)
    total_wins = total_losses = total_draws = 0
    llr = None
    decision = None

    status = {
        "param": param_name, "base_value": base_value, "candidate_value": candidate_value,
        "started_at": timestamp(), "updated_at": timestamp(),
        "games": 0, "wins": 0, "losses": 0, "draws": 0, "score": None,
        "llr": None, "lbound": lbound, "ubound": ubound, "sprt_decision": None,
        "elo_diff": None, "elo_error": None, "los": None, "draw_ratio": None,
        "executor": "slurm",
    }
    atomic_write_json(args.status, status)

    header = (
        f"[{timestamp()}] Testing {param_name} via Slurm: {base_value} -> {candidate_value} "
        f"({args.slurm_jobs} jobs x concurrency {args.concurrency}, batches of {args.slurm_batch_games} games, "
        f"bounds [{lbound:.2f}, {ubound:.2f}])"
    )
    print(header, flush=True)
    with open(args.log, "a") as logf:
        logf.write(f"\n=== {timestamp()} Testing {param_name} (SLURM): {base_value} -> {candidate_value} ===\n")
        logf.write(header + "\n")

    batch_index = 0
    while total_wins + total_losses + total_draws < args.rounds_cap:
        remaining = args.rounds_cap - (total_wins + total_losses + total_draws)
        batch_games = min(args.slurm_batch_games, remaining)
        n_jobs = max(1, min(args.slurm_jobs, batch_games))
        per_job = [batch_games // n_jobs] * n_jobs
        for i in range(batch_games % n_jobs):
            per_job[i] += 1

        batch_index += 1
        safe_name = re.sub(r"[^A-Za-z0-9_]", "_", param_name)
        job_ids, out_files, offsets = [], [], {}
        try:
            for i, rounds_i in enumerate(per_job):
                if rounds_i <= 0:
                    continue
                # os.getpid() namespaces the job/file names by *this* running
                # process, so two concurrent sprt_tune.py invocations sharing
                # the same --slurm-work-dir (e.g. tuning the same parameter
                # twice, or a stale run still finishing) can never collide on
                # the same .out/.pgn/.sh path.
                job_name = f"sprt_p{os.getpid()}_{safe_name}_b{batch_index}_{i}"
                out_file = work_dir / f"{job_name}.out"
                pgn_file = work_dir / f"{job_name}.pgn"
                argv = [
                    args.cutechess,
                    "-engine", "name=CANDIDATE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
                    *candidate_opts,
                    "-engine", "name=BASE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
                    *base_opts,
                    "-each", f"tc={args.tc}", "timemargin=50",
                    "-concurrency", str(args.concurrency),
                    "-rounds", str(rounds_i),
                    "-repeat",
                    "-openings", f"file={book}", f"format={book_format}", "order=random", "plies=100",
                    "-recover",
                    "-ratinginterval", str(args.rating_interval),
                    "-pgnout", str(pgn_file),
                ]
                script_path = work_dir / f"{job_name}.sh"
                write_slurm_script(args, script_path, out_file, argv)
                job_id = submit_slurm_job(script_path)
                job_ids.append(job_id)
                out_files.append(out_file)
                offsets[str(out_file)] = 0
                msg = f"    [{timestamp()}] submitted Slurm job {job_id} ({rounds_i} rounds) -> {out_file}"
                print(msg, flush=True)
                with open(args.log, "a") as logf:
                    logf.write(msg + "\n")
        except Exception as exc:
            msg = f"    [{timestamp()}] ERROR submitting Slurm jobs: {exc}"
            print(msg, flush=True)
            with open(args.log, "a") as logf:
                logf.write(msg + "\n")
            scancel_jobs(job_ids)
            return ""

        pending = set(job_ids)
        cur_wins, cur_losses, cur_draws = total_wins, total_losses, total_draws
        try:
            with open(args.log, "a") as logf:
                while pending:
                    time.sleep(max(1, args.slurm_poll_interval))
                    for out_file in out_files:
                        new_lines = tail_new_lines(out_file, offsets)
                        if new_lines:
                            logf.write("".join(new_lines))
                            logf.flush()
                    states = slurm_job_states(list(pending))
                    for jid in list(pending):
                        if slurm_state_is_terminal(states.get(jid, "")):
                            pending.discard(jid)

                    batch_wins = batch_losses = batch_draws = 0
                    for out_file in out_files:
                        if not out_file.exists():
                            continue
                        matches = SCORE_RE.findall(out_file.read_text(errors="replace"))
                        if matches:
                            _, _, w, l, d, _, _ = matches[-1]
                            batch_wins += int(w)
                            batch_losses += int(l)
                            batch_draws += int(d)

                    cur_wins = total_wins + batch_wins
                    cur_losses = total_losses + batch_losses
                    cur_draws = total_draws + batch_draws
                    games = cur_wins + cur_losses + cur_draws
                    score, se_score = score_stats(cur_wins, cur_losses, cur_draws)
                    elo, elo_err = elo_and_sem(cur_wins, cur_losses, cur_draws)
                    llr = sprt_llr(score, se_score ** 2, args.elo0, args.elo1) if (score is not None and se_score) else None

                    status.update(
                        games=games, wins=cur_wins, losses=cur_losses, draws=cur_draws,
                        score=round(score, 3) if score is not None else None,
                        llr=round(llr, 3) if llr is not None else None,
                        elo_diff=round(elo, 1) if elo is not None else None,
                        elo_error=round(elo_err, 1) if elo_err is not None else None,
                        updated_at=timestamp(),
                    )
                    atomic_write_json(args.status, status)

                    elo_part = (
                        f", elo={elo:+.1f}+/-{elo_err:.1f}" if elo is not None and elo_err is not None
                        else (f", elo={elo:+.1f}" if elo is not None else "")
                    )
                    llr_part = f", llr={llr:.3f} (bounds {lbound:.2f}/{ubound:.2f})" if llr is not None else ""
                    progress = f"    [{timestamp()}] {games} games, +{cur_wins}-{cur_losses}={cur_draws}{elo_part}{llr_part}"
                    print(progress, flush=True)
                    logf.write(progress + "\n")
                    logf.flush()
        except BaseException:
            # Ctrl+C or any unexpected error while jobs are still pending:
            # cancel them so they don't keep running/queued on the cluster
            # after this script has stopped (see scancel_jobs docstring).
            scancel_jobs(pending)
            raise

        total_wins, total_losses, total_draws = cur_wins, cur_losses, cur_draws

        if llr is not None:
            if llr >= ubound:
                decision = "H1"
                break
            if llr <= lbound:
                decision = "H0"
                break

    games = total_wins + total_losses + total_draws
    final_score = (total_wins + total_draws / 2.0) / games if games else 0.0
    status["finished_at"] = timestamp()
    status["sprt_decision"] = decision
    atomic_write_json(args.status, status)

    llr_val = llr if llr is not None else 0.0
    decision_part = f" - {decision} was accepted" if decision else ""
    synthetic = (
        f"Score of CANDIDATE vs BASE: {total_wins} - {total_losses} - {total_draws}  [{final_score:.3f}] {games}\n"
        f"SPRT: llr {llr_val:.3f} (0.0%), lbound {lbound:.2f}, ubound {ubound:.2f}{decision_part}\n"
    )
    with open(args.log, "a") as logf:
        logf.write(synthetic)
    return synthetic


def run_match(args, param_name, base_value, candidate_value, candidate_opts, base_opts):
    """Dispatch to the local (single-machine subprocess) or Slurm-distributed
    match runner, selected via --executor. Both return the same text blob
    format understood by parse_result()."""
    if args.executor == "slurm":
        return run_match_slurm(args, param_name, base_value, candidate_value, candidate_opts, base_opts)
    return run_match_local(args, param_name, base_value, candidate_value, candidate_opts, base_opts)


def shrink_step(p):
    new_step = p["step"] // 2
    if new_step < p.get("min_step", 1):
        p["converged"] = True
    else:
        p["step"] = new_step
        p["last_fail_direction"] = None


def run_one_test(args, params, name, p):
    direction = p.get("next_direction", 1)
    base_value = p["value"]
    candidate_value = clamp(base_value + direction * p["step"], p["min"], p["max"])
    if candidate_value == base_value:
        direction = -direction
        candidate_value = clamp(base_value + direction * p["step"], p["min"], p["max"])
    if candidate_value == base_value:
        # Stuck at both bounds with the current step.
        shrink_step(p)
        return {
            "status": "skipped", "llr": None, "wins": 0, "losses": 0, "draws": 0,
            "games": 0, "direction": direction, "candidate_value": candidate_value,
        }

    candidate_opts = build_option_args(params, name, candidate_value)
    base_opts = build_option_args(params, name, base_value)
    text = run_match(args, name, base_value, candidate_value, candidate_opts, base_opts)
    result = parse_result(text)
    result["direction"] = direction
    result["candidate_value"] = candidate_value
    return result


def apply_outcome(p, outcome):
    direction = outcome["direction"]
    if outcome["status"] == "accept":
        p["value"] = outcome["candidate_value"]
        p["last_fail_direction"] = None
        p["next_direction"] = direction
    elif outcome["status"] in ("reject", "inconclusive", "skipped"):
        if p.get("last_fail_direction") == -direction:
            shrink_step(p)
        else:
            p["last_fail_direction"] = direction
        p["next_direction"] = -direction


def cmd_tune(args):
    state, resumed = load_state(args.state, args.params)
    order = list(state["params"].keys())
    if not order:
        print("No parameters to tune (empty params file).")
        return 1
    if args.executor == "slurm" and shutil.which("sbatch") is None:
        print("--executor slurm requires 'sbatch' to be available on PATH.")
        return 1
    announce_session(args, state, order, resumed)

    consecutive_errors = 0
    tests_done = 0
    try:
        while args.max_tests == 0 or tests_done < args.max_tests:
            active = [n for n in order if not state["params"][n].get("converged")]
            if not active:
                print("All parameters converged (step shrunk below min_step in both directions). Stopping.")
                break

            cursor = state.get("cursor", 0)
            name = active[cursor % len(active)]
            state["cursor"] = cursor + 1
            p = state["params"][name]

            outcome = run_one_test(args, state["params"], name, p)
            tests_done += 1

            if outcome["status"] == "error":
                consecutive_errors += 1
                print(f"  -> ERROR running match (attempt {consecutive_errors}); see {args.log} for details.")
                atomic_write_json(args.state, state)
                if consecutive_errors >= 3:
                    print("Three consecutive errors, aborting. Check --engine/--cutechess/--book paths.")
                    break
                continue
            consecutive_errors = 0

            apply_outcome(p, outcome)
            state["tests_run"] = state.get("tests_run", 0) + 1
            atomic_write_json(args.state, state)
            write_best_params(args.best_out, state)

            print(
                "  -> {status} (llr={llr}, +{w}-{l}={d} / {g} games); "
                "{name} now {value} (step {step}{conv})".format(
                    status=outcome["status"],
                    llr=outcome["llr"],
                    w=outcome["wins"], l=outcome["losses"], d=outcome["draws"], g=outcome["games"],
                    name=name, value=p["value"], step=p["step"],
                    conv=", converged" if p["converged"] else "",
                ),
                flush=True,
            )
    except KeyboardInterrupt:
        print("\nInterrupted by user, saving state...")
    finally:
        atomic_write_json(args.state, state)
        write_best_params(args.best_out, state)
        print(f"State saved to {args.state}; best known values in {args.best_out}")
    return 0


# --------------------------------------------------------------------------- 
# CLI
# ---------------------------------------------------------------------------

def build_arg_parser():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_disc = sub.add_parser(
        "discover",
        help="Query an engine's UCI 'spin' options and write a starter parameter file.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p_disc.add_argument("engine", help="Path to the engine binary (built with WITH_SEARCH_TUNING enabled)")
    p_disc.add_argument("--output", default=str(DEFAULT_PARAMS_FILE), help="Where to write the discovered parameter file")
    p_disc.add_argument("--step-fraction", type=float, default=40.0, help="Initial step = (max-min)/step_fraction, rounded, minimum 1")
    p_disc.set_defaults(func=cmd_discover)

    p_tune = sub.add_parser(
        "tune",
        help="Run the SPRT-gated local search tuning loop.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p_tune.add_argument("engine", help="Path to the engine binary (built with WITH_SEARCH_TUNING enabled)")
    p_tune.add_argument("--params", default=str(DEFAULT_PARAMS_FILE), help="Parameter file produced by 'discover' (and pruned by hand)")
    p_tune.add_argument("--state", default=str(DEFAULT_STATE_FILE), help="State file used to resume an interrupted tuning session")
    p_tune.add_argument("--best-out", default=str(DEFAULT_BEST_FILE), help="Where to write the current best known values (name -> value)")
    p_tune.add_argument("--book", default=str(DEFAULT_BOOK), help="Opening book (.epd or .pgn) used for the matches")
    p_tune.add_argument("--cutechess", default=DEFAULT_CUTECHESS, help="Path to the cutechess-cli executable")
    p_tune.add_argument("--tc", default="10+0.1", help="Time control passed to cutechess-cli, e.g. '10+0.1'")
    p_tune.add_argument("--concurrency", type=int, default=8, help="Number of concurrent games")
    p_tune.add_argument("--rounds-cap", type=int, default=10000, help="Upper bound on rounds (game pairs); the SPRT bound normally stops the match earlier")
    p_tune.add_argument("--elo0", type=float, default=0.0, help="H1: the change gains at least this many elo")
    p_tune.add_argument("--elo1", type=float, default=2.0, help="H0: the change does not gain this many elo")
    p_tune.add_argument("--alpha", type=float, default=0.2, help="Type I error rate")
    p_tune.add_argument("--beta", type=float, default=0.2, help="Type II error rate")
    p_tune.add_argument("--max-tests", type=int, default=0, help="Stop after this many parameter tests (0 = run forever, until Ctrl+C or convergence)")
    p_tune.add_argument("--log", default=str(DEFAULT_LOG_FILE), help="Full cutechess-cli output is appended here for every test, updated live as games finish")
    p_tune.add_argument("--status", default=str(DEFAULT_STATUS_FILE), help="JSON file with the live status (games, score, elo, llr) of the currently running match, refreshed as games finish")
    p_tune.add_argument("--rating-interval", type=int, default=100, help="Print/refresh Elo+SPRT status every N finished games (passed to cutechess-cli -ratinginterval)")
    p_tune.add_argument("--pgn-out", default=str(DEFAULT_PGN_FILE), help="PGN of the last match (overwritten after every test)")
    p_tune.add_argument("--executor", choices=["local", "slurm"], default="local", help="'local' runs one cutechess-cli process on this machine (default); 'slurm' splits each test's games across --slurm-jobs parallel sbatch jobs for massive concurrency across a cluster, aggregating results and checking the SPRT bounds after each batch")
    p_tune.add_argument("--slurm-jobs", type=int, default=4, help="Number of parallel Slurm jobs to split each SPRT decision-batch's games across (only used with --executor slurm)")
    p_tune.add_argument("--slurm-batch-games", type=int, default=200, help="Games per SPRT decision-batch (split evenly across --slurm-jobs); after each batch the aggregated LLR is checked against the SPRT bounds before deciding whether to submit another batch (only used with --executor slurm)")
    p_tune.add_argument("--slurm-cpus-per-task", type=int, default=None, help="--cpus-per-task passed to sbatch for each sub-job (default: same as --concurrency)")
    p_tune.add_argument("--slurm-partition", default=None, help="--partition passed to sbatch")
    p_tune.add_argument("--slurm-account", default=None, help="--account passed to sbatch")
    p_tune.add_argument("--slurm-time", default="00:30:00", help="--time passed to sbatch (e.g. 'HH:MM:SS' or 'D-HH:MM:SS')")
    p_tune.add_argument("--slurm-extra", default=None, help="Extra raw sbatch directives, space separated (e.g. '--gres=gpu:0 --mem=4G'), each becomes its own #SBATCH line")
    p_tune.add_argument("--slurm-poll-interval", type=int, default=15, help="Seconds between polls of Slurm job state / tailing of job output files")
    p_tune.add_argument("--slurm-work-dir", default=str(DEFAULT_SLURM_WORK_DIR), help="Directory for generated sbatch scripts and per-job output/PGN files")
    p_tune.set_defaults(func=cmd_tune)

    return parser


def main():
    parser = build_arg_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main() or 0)
