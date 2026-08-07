#!/usr/bin/env python3
"""Bayesian-optimization tuner for Minic's WITH_SEARCH_TUNING UCI options,
using Meta's Ax platform (https://ax.dev) instead of the coordinate-descent
SPRT approach in sprt_tune.py.

This is a sibling of sprt_tune.py: same `discover` sub-command, same
cutechess-cli-based match runner, same live --log/--status streaming and
checkpoint/resume banner, but the `tune` sub-command replaces the
one-parameter-at-a-time SPRT accept/reject loop with Ax's Bayesian
optimization loop (a Gaussian-process surrogate under the hood, via
BoTorch): Ax proposes a full vector of parameter values, we play it against
a FIXED reference build (the values from the params file) over a fixed
number of games, convert the result to an Elo estimate with its standard
error, and report `(elo, sem)` back to Ax so it can model the noisy
objective and propose the next, hopefully better, point.

Requires: pip install ax-platform

Two sub-commands are provided:

  discover  Query an engine binary over UCI and dump every "spin" option to a
            JSON parameter file (value/min/max/step). Edit that file to keep
            only the parameters you actually want to tune (Hash, Threads,
            MultiPV, NNUE*, Style*, Level, ... are not search-tuning knobs
            and should usually be removed). Identical to sprt_tune.py's
            `discover`.

  tune      Ask Ax for the next parameter vector to try, play it against the
            reference build via cutechess-cli (fixed number of games, no
            SPRT early-stop), report the resulting Elo estimate to Ax, and
            repeat. Ax's own state (Gaussian-process model + trial history)
            is checkpointed to --ax-state after every trial so the process
            can be interrupted (Ctrl+C) and resumed later.

Each trial's match can be run in one of two ways, via --executor:

  local  (default) One cutechess-cli process on this machine, playing
         -concurrency games in parallel.

  slurm  Splits the trial's --games-per-trial games across --slurm-jobs
         parallel `sbatch` jobs (each an independent cutechess-cli process
         with its own -concurrency). Results are read directly from each
         job's Slurm --output file (streamed live, and also once the job
         finishes); once every job is done the W/L/D across all of them is
         aggregated into the trial's Elo/SEM estimate reported back to Ax.
         This gives "massive concurrency" = --slurm-jobs x --concurrency
         games running at once, spread across whatever nodes Slurm
         schedules them on.

Example
-------
    # Build a binary with WITH_SEARCH_TUNING enabled first, then:
    python3 Tools/fit/ax_tune.py discover ./Tourney/minic_dev_linux_x64
    # -> edit Tools/fit/ax_tune_params.json to select parameters
    python3 Tools/fit/ax_tune.py tune ./Tourney/minic_dev_linux_x64 \
        --concurrency 8 --tc 10+0.1 --games-per-trial 200
    # ...or distributed across a Slurm cluster:
    python3 Tools/fit/ax_tune.py tune ./Tourney/minic_dev_linux_x64 \
        --executor slurm --slurm-jobs 16 --concurrency 8 --games-per-trial 400
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

DEFAULT_PARAMS_FILE = Path(__file__).resolve().parent / "ax_tune_params.json"
DEFAULT_AX_STATE_FILE = Path(__file__).resolve().parent / "ax_tune_state.json"
DEFAULT_BEST_FILE = Path(__file__).resolve().parent / "ax_tune_best.json"
DEFAULT_LOG_FILE = Path(__file__).resolve().parent / "ax_tune.log"
DEFAULT_STATUS_FILE = Path(__file__).resolve().parent / "ax_tune_status.json"
DEFAULT_PGN_FILE = Path(__file__).resolve().parent / "ax_tune_last.pgn"
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
SCORE_RE = re.compile(
    r"Score of (\S+) vs (\S+): (\d+) - (\d+) - (\d+)\s+\[([\d.]+)\]\s+(\d+)"
)


def timestamp():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def atomic_write_json(path, data):
    tmp = f"{path}.tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)
    os.replace(tmp, path)


# ---------------------------------------------------------------------------
# discover (identical to sprt_tune.py)
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
    import time as _time
    start = _time.time()
    try:
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        while _time.time() - start < timeout:
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
# Elo helpers (self-computed from W/L/D counts, no dependency on cutechess's
# own "Elo difference" text, which can print "nan" for small/extreme samples)
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
    orphaned sub-jobs when a trial is interrupted (Ctrl+C) or aborts with an
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
# tune (Ax Bayesian optimization)
# ---------------------------------------------------------------------------

def build_option_args(values):
    return [f"option.{name}={int(round(v))}" for name, v in values.items()]


def run_match_local(args, trial_index, candidate_values, base_values):
    engine_dir = os.path.dirname(os.path.abspath(args.engine)) or "."
    book = str(args.book)
    book_format = "epd" if book.lower().endswith(".epd") else "pgn"

    candidate_opts = build_option_args(candidate_values)
    base_opts = build_option_args(base_values)

    argv = [
        args.cutechess,
        "-engine", "name=CANDIDATE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
        *candidate_opts,
        "-engine", "name=BASE", f"cmd={args.engine}", f"dir={engine_dir}", "proto=uci",
        *base_opts,
        "-each", f"tc={args.tc}", "timemargin=50",
        "-concurrency", str(args.concurrency),
        "-rounds", str(args.games_per_trial),
        "-repeat",
        "-openings", f"file={book}", f"format={book_format}", "order=random", "plies=100",
        "-recover",
        "-ratinginterval", str(args.rating_interval),
        "-pgnout", str(args.pgn_out),
    ]

    print(f"[{timestamp()}] Trial {trial_index}: {candidate_values}", flush=True)

    status = {
        "trial": trial_index,
        "candidate": candidate_values,
        "started_at": timestamp(),
        "updated_at": timestamp(),
        "games": 0,
        "wins": 0,
        "losses": 0,
        "draws": 0,
        "score": None,
        "elo_diff": None,
        "elo_sem": None,
    }
    atomic_write_json(args.status, status)

    games_since_print = 0
    lines = []
    with open(args.log, "a") as logf:
        logf.write(f"\n=== {timestamp()} Trial {trial_index}: {candidate_values} ===\n")
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
                    elo, sem = elo_and_sem(status["wins"], status["losses"], status["draws"])
                    status["elo_diff"], status["elo_sem"] = elo, sem
                    status["updated_at"] = timestamp()
                    atomic_write_json(args.status, status)

                    games_since_print += 1
                    if games_since_print >= args.rating_interval:
                        games_since_print = 0
                        elo_part = (
                            f", elo={elo:+.1f}+/-{sem:.1f}" if elo is not None and sem is not None
                            else (f", elo={elo:+.1f}" if elo is not None else "")
                        )
                        print(
                            f"    [{timestamp()}] {status['games']} games, "
                            f"+{status['wins']}-{status['losses']}={status['draws']}{elo_part}",
                            flush=True,
                        )
        finally:
            proc.wait()

    status["finished_at"] = timestamp()
    status["returncode"] = proc.returncode
    atomic_write_json(args.status, status)

    if status["games"] == 0:
        return {"status": "error", **status}
    return {"status": "ok", **status}


def run_match_slurm(args, trial_index, candidate_values, base_values):
    """Slurm-distributed equivalent of run_match_local(): splits this trial's
    --games-per-trial games across --slurm-jobs parallel sbatch jobs (each
    running its own cutechess-cli with -concurrency games in parallel), polls
    their state + tails their --output files (which *are* the per-job
    cutechess-cli logs) until every job finishes, and aggregates W/L/D across
    all of them into a single Elo/SEM estimate for this trial. Returns the
    same {"status": ..., **status} dict shape run_match_local() returns, so
    cmd_tune() doesn't need to change."""
    if shutil.which("sbatch") is None:
        msg = f"[{timestamp()}] ERROR: sbatch not found on PATH; cannot use --executor slurm."
        print(msg, flush=True)
        with open(args.log, "a") as logf:
            logf.write(msg + "\n")
        return {"status": "error", "games": 0, "wins": 0, "losses": 0, "draws": 0,
                "elo_diff": None, "elo_sem": None}

    engine_dir = os.path.dirname(os.path.abspath(args.engine)) or "."
    book = str(args.book)
    book_format = "epd" if book.lower().endswith(".epd") else "pgn"
    work_dir = Path(args.slurm_work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    candidate_opts = build_option_args(candidate_values)
    base_opts = build_option_args(base_values)

    n_jobs = max(1, min(args.slurm_jobs, args.games_per_trial))
    per_job = [args.games_per_trial // n_jobs] * n_jobs
    for i in range(args.games_per_trial % n_jobs):
        per_job[i] += 1

    status = {
        "trial": trial_index, "candidate": candidate_values,
        "started_at": timestamp(), "updated_at": timestamp(),
        "games": 0, "wins": 0, "losses": 0, "draws": 0, "score": None,
        "elo_diff": None, "elo_sem": None, "executor": "slurm",
    }
    atomic_write_json(args.status, status)

    header = (
        f"[{timestamp()}] Trial {trial_index} via Slurm: {candidate_values} "
        f"({n_jobs} jobs x concurrency {args.concurrency})"
    )
    print(header, flush=True)
    with open(args.log, "a") as logf:
        logf.write(f"\n=== {timestamp()} Trial {trial_index} (SLURM): {candidate_values} ===\n")
        logf.write(header + "\n")

    job_ids, out_files, offsets = [], [], {}
    try:
        for i, rounds_i in enumerate(per_job):
            if rounds_i <= 0:
                continue
            # os.getpid() namespaces the job/file names by *this* running
            # process, so two concurrent ax_tune.py invocations sharing the
            # same --slurm-work-dir (e.g. two separate tuning sessions, or a
            # stale run still finishing while a new one starts) can never
            # collide on the same .out/.pgn/.sh path, even if their trial
            # indices happen to coincide.
            job_name = f"ax_p{os.getpid()}_trial{trial_index}_{i}"
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
        return {"status": "error", **status}

    pending = set(job_ids)
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

                tw = tl = td = 0
                for out_file in out_files:
                    if not out_file.exists():
                        continue
                    matches = SCORE_RE.findall(out_file.read_text(errors="replace"))
                    if matches:
                        _, _, w, l, d, _, _ = matches[-1]
                        tw += int(w)
                        tl += int(l)
                        td += int(d)
                games = tw + tl + td
                elo, sem = elo_and_sem(tw, tl, td)
                status.update(
                    wins=tw, losses=tl, draws=td, games=games,
                    score=round((tw + td / 2.0) / games, 3) if games else None,
                    elo_diff=round(elo, 1) if elo is not None else None,
                    elo_sem=round(sem, 1) if sem is not None else None,
                    updated_at=timestamp(),
                )
                atomic_write_json(args.status, status)

                elo_part = (
                    f", elo={elo:+.1f}+/-{sem:.1f}" if elo is not None and sem is not None
                    else (f", elo={elo:+.1f}" if elo is not None else "")
                )
                progress = f"    [{timestamp()}] {games}/{args.games_per_trial} games, +{tw}-{tl}={td}{elo_part}"
                print(progress, flush=True)
                logf.write(progress + "\n")
                logf.flush()
    except BaseException:
        # Ctrl+C or any unexpected error while jobs are still pending: cancel
        # them so they don't keep running/queued on the cluster after this
        # script has stopped (see scancel_jobs docstring).
        scancel_jobs(pending)
        raise

    status["finished_at"] = timestamp()
    atomic_write_json(args.status, status)
    if status["games"] == 0:
        return {"status": "error", **status}
    return {"status": "ok", **status}


def run_match(args, trial_index, candidate_values, base_values):
    """Dispatch to the local (single-machine subprocess) or Slurm-distributed
    match runner, selected via --executor. Both return the same
    {"status": ..., **status} dict shape."""
    if args.executor == "slurm":
        return run_match_slurm(args, trial_index, candidate_values, base_values)
    return run_match_local(args, trial_index, candidate_values, base_values)


def announce_session(args, params_def, resumed, trials_completed):
    if resumed:
        header = (
            f"[{timestamp()}] Resuming Ax study from checkpoint {args.ax_state} "
            f"({trials_completed} trials already completed)."
        )
    else:
        header = (
            f"[{timestamp()}] Starting a NEW Ax Bayesian-optimization study "
            f"({len(params_def)} parameters) -> {args.ax_state}"
        )
    lines = [header]
    for name, d in params_def.items():
        lines.append(f"    {name}: reference={d['value']} bounds=[{d['min']}, {d['max']}]")

    print("\n".join(lines), flush=True)
    with open(args.log, "a") as logf:
        logf.write("\n" + "\n".join(lines) + "\n")


def write_best_params(args, ax_client, params_def):
    try:
        result = ax_client.get_best_parameters()
    except Exception:
        return
    if not result or result[0] is None:
        return
    best_params = result[0]
    out = {name: int(round(best_params[name])) for name in params_def}
    atomic_write_json(args.best_out, out)


def cmd_tune(args):
    try:
        from ax.service.ax_client import AxClient, ObjectiveProperties
    except ImportError:
        print(
            "The 'ax-platform' package is required for this sub-command.\n"
            "Install it with: pip install ax-platform",
            file=sys.stderr,
        )
        return 1

    if not os.path.exists(args.params):
        print(
            f"Parameter file {args.params} not found. Run the 'discover' "
            "sub-command first."
        )
        return 1
    with open(args.params) as f:
        params_def = json.load(f)
    if not params_def:
        print("No parameters to tune (empty params file).")
        return 1
    if args.executor == "slurm" and shutil.which("sbatch") is None:
        print("--executor slurm requires 'sbatch' to be available on PATH.")
        return 1

    base_values = {name: int(d["value"]) for name, d in params_def.items()}

    resumed = os.path.exists(args.ax_state)
    if resumed:
        ax_client = AxClient.load_from_json_file(args.ax_state)
    else:
        if args.num_sobol_trials > 0:
            # By default Ax picks its own initial-exploration length (a
            # heuristic based on the number of tuned parameters). Setting
            # --num-sobol-trials overrides this with an explicit number of
            # quasi-random (Sobol) trials to run before switching to the
            # Bayesian-optimization (GP) model -- more = more upfront
            # exploration/coverage of the search space before the model
            # starts exploiting what it has learned, fewer = the GP kicks in
            # sooner (useful when games are expensive and you trust the
            # model to find structure quickly).
            from ax.generation_strategy.generation_strategy import GenerationStrategy
            from ax.generation_strategy.generation_node import GenerationStep
            from ax.adapter.registry import Generators

            generation_strategy = GenerationStrategy(
                nodes=[
                    GenerationStep(
                        generator=Generators.SOBOL,
                        num_trials=args.num_sobol_trials,
                        min_trials_observed=args.num_sobol_trials,
                        max_parallelism=args.num_sobol_trials,
                    ),
                    GenerationStep(generator=Generators.BOTORCH_MODULAR, num_trials=-1),
                ]
            )
            ax_client = AxClient(generation_strategy=generation_strategy, random_seed=args.seed)
        else:
            ax_client = AxClient(random_seed=args.seed)
        ax_client.create_experiment(
            name="minic_search_tuning",
            parameters=[
                {
                    "name": name,
                    "type": "range",
                    "bounds": [int(d["min"]), int(d["max"])],
                    "value_type": "int",
                }
                for name, d in params_def.items()
            ],
            objectives={"elo": ObjectiveProperties(minimize=False)},
        )

    trials_completed = len(ax_client.experiment.trials) if resumed else 0
    announce_session(args, params_def, resumed, trials_completed)

    consecutive_errors = 0
    trials_done = 0
    try:
        while args.max_trials == 0 or trials_done < args.max_trials:
            # get_next_trial() can occasionally raise on a transient BoTorch
            # acquisition-optimization failure (e.g. a numerical hiccup while
            # fitting the surrogate); retry a few times with a short delay
            # before giving up, rather than crashing the whole tuning run.
            for attempt in range(5):
                try:
                    parameterization, trial_index = ax_client.get_next_trial()
                    break
                except Exception as exc:
                    print(f"  -> WARNING: get_next_trial() failed (attempt {attempt + 1}/5): {exc}")
                    if attempt == 4:
                        raise
                    time.sleep(5)
            candidate_values = {name: int(round(parameterization[name])) for name in params_def}

            outcome = run_match(args, trial_index, candidate_values, base_values)
            trials_done += 1

            if outcome["status"] == "error":
                consecutive_errors += 1
                print(f"  -> ERROR running match (attempt {consecutive_errors}); see {args.log} for details.")
                ax_client.log_trial_failure(trial_index=trial_index)
                ax_client.save_to_json_file(args.ax_state)
                if consecutive_errors >= 3:
                    print("Three consecutive errors, aborting. Check --engine/--cutechess/--book paths.")
                    break
                continue
            consecutive_errors = 0

            elo, sem = outcome["elo_diff"], outcome["elo_sem"]
            raw_data = {"elo": (elo, sem)} if sem is not None else {"elo": elo}
            ax_client.complete_trial(trial_index=trial_index, raw_data=raw_data)
            ax_client.save_to_json_file(args.ax_state)
            write_best_params(args, ax_client, params_def)

            elo_part = f"{elo:+.1f}+/-{sem:.1f}" if sem is not None else f"{elo:+.1f}"
            print(
                f"  -> trial {trial_index} done: elo={elo_part}, "
                f"+{outcome['wins']}-{outcome['losses']}={outcome['draws']} / {outcome['games']} games",
                flush=True,
            )
    except KeyboardInterrupt:
        print("\nInterrupted by user, saving state...")
    finally:
        ax_client.save_to_json_file(args.ax_state)
        write_best_params(args, ax_client, params_def)
        print(f"Ax state saved to {args.ax_state}; best known values in {args.best_out}")
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
    p_disc.add_argument("--step-fraction", type=float, default=40.0, help="Initial step = (max-min)/step_fraction, rounded, minimum 1 (unused by 'tune', kept for parity with sprt_tune.py's file format)")
    p_disc.set_defaults(func=cmd_discover)

    p_tune = sub.add_parser(
        "tune",
        help="Run the Ax Bayesian-optimization tuning loop.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p_tune.add_argument("engine", help="Path to the engine binary (built with WITH_SEARCH_TUNING enabled)")
    p_tune.add_argument("--params", default=str(DEFAULT_PARAMS_FILE), help="Parameter file produced by 'discover' (and pruned by hand); the 'value' of each parameter is used as the FIXED reference/baseline throughout the whole study")
    p_tune.add_argument("--ax-state", default=str(DEFAULT_AX_STATE_FILE), help="JSON file holding Ax's experiment/model state, used to resume an interrupted study")
    p_tune.add_argument("--best-out", default=str(DEFAULT_BEST_FILE), help="Where to write Ax's current best known parameters (name -> value)")
    p_tune.add_argument("--book", default=str(DEFAULT_BOOK), help="Opening book (.epd or .pgn) used for the matches")
    p_tune.add_argument("--cutechess", default=DEFAULT_CUTECHESS, help="Path to the cutechess-cli executable")
    p_tune.add_argument("--tc", default="10+0.1", help="Time control passed to cutechess-cli, e.g. '10+0.1'")
    p_tune.add_argument("--concurrency", type=int, default=8, help="Number of concurrent games")
    p_tune.add_argument("--games-per-trial", type=int, default=200, help="Fixed number of games played per Ax trial (no SPRT early-stop; Ax needs a consistent noise model)")
    p_tune.add_argument("--max-trials", type=int, default=0, help="Stop after this many trials (0 = run forever, until Ctrl+C)")
    p_tune.add_argument("--seed", type=int, default=None, help="Random seed for Ax's initial quasi-random exploration phase")
    p_tune.add_argument("--num-sobol-trials", type=int, default=0, help="Number of initial quasi-random (Sobol) trials to run before switching to Bayesian-optimization (GP) trials; 0 (default) = let Ax choose automatically based on the number of tuned parameters. Raise this for more upfront exploration/coverage of the search space before the model starts exploiting what it has learned; lower it to let the GP kick in sooner (only used when starting a NEW study, ignored on resume since the generation strategy is already part of --ax-state)")
    p_tune.add_argument("--log", default=str(DEFAULT_LOG_FILE), help="Full cutechess-cli output is appended here for every trial, updated live as games finish")
    p_tune.add_argument("--status", default=str(DEFAULT_STATUS_FILE), help="JSON file with the live status (games, score, elo) of the currently running trial, refreshed as games finish")
    p_tune.add_argument("--rating-interval", type=int, default=10, help="Print/refresh progress every N finished games (also passed to cutechess-cli -ratinginterval for the human-readable log block)")
    p_tune.add_argument("--pgn-out", default=str(DEFAULT_PGN_FILE), help="PGN of the last trial's match (overwritten after every trial)")
    p_tune.add_argument("--executor", choices=["local", "slurm"], default="local", help="'local' runs one cutechess-cli process on this machine (default); 'slurm' splits each trial's --games-per-trial games across --slurm-jobs parallel sbatch jobs for massive concurrency across a cluster")
    p_tune.add_argument("--slurm-jobs", type=int, default=4, help="Number of parallel Slurm jobs to split each trial's games across (only used with --executor slurm)")
    p_tune.add_argument("--slurm-cpus-per-task", type=int, default=None, help="--cpus-per-task passed to sbatch for each sub-job (default: same as --concurrency)")
    p_tune.add_argument("--slurm-partition", default=None, help="--partition passed to sbatch")
    p_tune.add_argument("--slurm-account", default=None, help="--account passed to sbatch")
    p_tune.add_argument("--slurm-time", default="02:00:00", help="--time passed to sbatch (e.g. 'HH:MM:SS' or 'D-HH:MM:SS')")
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
