#!/usr/bin/env python3
"""Plot the optimization landscape explored by ax_tune.py.

Loads the Ax experiment state (--ax-state, the same file ax_tune.py's `tune`
sub-command checkpoints after every trial) and renders an N x N grid of
matplotlib subplots, N being the number of tuned parameters:

  - Diagonal cell i: the observed metric (default "elo") plotted against
    parameter i alone (one point per completed trial), with a 1D Gaussian
    Process regression (scikit-learn, RBF + white-noise kernel) overlaid as
    a mean curve plus a shaded 95% confidence band -- the same "posterior
    mean +/- uncertainty" look classic Bayesian-optimization plots use, with
    the band naturally pinching in near observed points and widening in
    unsampled gaps. Each parameter's GP is fit independently, so it is normal
    for some diagonals to come out flat (that parameter shows ~0 signal on
    its own) while another looks much wigglier (the model found a real
    likelihood improvement from a short length scale for that one) --
    --gp-min-length-scale-frac puts a floor (as a fraction of the parameter's
    range) under how short that length scale is allowed to be, to keep the
    diagonals more visually consistent. Falls back to a low-order
    least-squares polynomial trend curve (--diagonal-degree, default 2: a
    single parabola, no uncertainty band) if scikit-learn is unavailable or
    too few points are observed.
  - Off-diagonal cell (i, j): a 2D landscape for the (parameter_j,
    parameter_i) plane, built by fitting a smooth radial-basis-function
    surface (scipy.interpolate.RBFInterpolator, thin-plate-spline kernel)
    through the observed metric values and evaluating it on a regular grid,
    so you get a smooth filled-contour "landscape" (well-defined everywhere,
    including slightly outside the sampled region) instead of a jagged
    piecewise-linear one, with the actual trial points overlaid and the
    current best point marked with a star. --smoothing controls how tightly
    the surface has to pass through the (noisy) observed points.

This only looks at *observed* trial data (no dependency on the internal
BoTorch/GP model object, which is fragile to introspect across Ax versions)
so it works from very few trials onward, and always matches what actually
happened on the board.

Requires: pip install ax-platform matplotlib scipy scikit-learn (scipy and
matplotlib are already pulled in transitively by ax-platform in practice;
scikit-learn is optional -- only used for the diagonal uncertainty band and
the script degrades gracefully to a plain polynomial trend without it).

Example
-------
    python3 Tools/fit/ax_plot.py
    python3 Tools/fit/ax_plot.py --ax-state Tools/fit/ax_tune_state.json \
        --out landscape.png --resolution 80
    # smoother / more damped-to-noise landscape:
    python3 Tools/fit/ax_plot.py --smoothing 2000
"""

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

DEFAULT_AX_STATE_FILE = Path(__file__).resolve().parent / "ax_tune_state.json"
DEFAULT_PARAMS_FILE = Path(__file__).resolve().parent / "ax_tune_params.json"
DEFAULT_BEST_FILE = Path(__file__).resolve().parent / "ax_tune_best.json"
DEFAULT_OUT_FILE = Path(__file__).resolve().parent / "ax_tune_landscape.png"


def load_trials(ax_state_path, params_path, best_path, metric):
    try:
        from ax.service.ax_client import AxClient
    except ImportError:
        print(
            "The 'ax-platform' package is required for this script.\n"
            "Install it with: pip install ax-platform",
            file=sys.stderr,
        )
        sys.exit(1)

    if not Path(ax_state_path).exists():
        print(f"Ax state file {ax_state_path} not found; run ax_tune.py tune first.")
        sys.exit(1)

    ax_client = AxClient.load_from_json_file(str(ax_state_path))
    df = ax_client.get_trials_data_frame()
    if metric not in df.columns:
        print(f"Metric '{metric}' not found in trial data; available columns: {list(df.columns)}")
        sys.exit(1)

    df = df[df["trial_status"] == "COMPLETED"].dropna(subset=[metric])
    if df.empty:
        print("No completed trials with data yet; run a few more trials first.")
        sys.exit(1)

    search_space_params = ax_client.experiment.search_space.parameters
    if Path(params_path).exists():
        with open(params_path) as f:
            ordered_names = list(json.load(f).keys())
    else:
        ordered_names = list(search_space_params.keys())
    params = [name for name in ordered_names if name in df.columns and name in search_space_params]
    if not params:
        print("No tuned parameters found in common between --params and the Ax state file.")
        sys.exit(1)

    bounds = {name: (search_space_params[name].lower, search_space_params[name].upper) for name in params}

    # Prefer the --best file (ax_tune_best.json), the exact values ax_tune.py
    # itself last wrote out via write_best_params() during the tuning run, so
    # the star plotted here always matches what's in that file. Only fall
    # back to recomputing (which can legitimately disagree: Ax's model-based
    # recommendation involves a fresh continuous optimization of the
    # posterior mean each time it's called, so a freshly-loaded AxClient can
    # return a different, but not necessarily worse, point than the one
    # produced live during the run) if that file is missing.
    best_point = None
    if Path(best_path).exists():
        with open(best_path) as f:
            best_json = json.load(f)
        if all(name in best_json for name in params):
            best_point = {name: best_json[name] for name in params}
    if best_point is None:
        try:
            best_params, _ = ax_client.get_best_parameters()
            if best_params is not None:
                best_point = {name: best_params[name] for name in params}
        except Exception:
            pass
    if best_point is None:
        best_row = df.loc[df[metric].idxmax()]
        best_point = {name: best_row[name] for name in params}

    return df, params, bounds, best_point


def plot_landscape(df, params, bounds, best_point, metric, resolution, smoothing, diagonal_degree,
                    gp_min_length_scale_frac, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from scipy.interpolate import RBFInterpolator

    try:
        import warnings
        from sklearn.gaussian_process import GaussianProcessRegressor
        from sklearn.gaussian_process.kernels import RBF, WhiteKernel, ConstantKernel
        from sklearn.exceptions import ConvergenceWarning
        have_sklearn = True
    except ImportError:
        have_sklearn = False

    n = len(params)
    fig, axes = plt.subplots(n, n, figsize=(3.3 * n, 3.1 * n), squeeze=False)
    values = df[metric].to_numpy(dtype=float)
    vmin, vmax = values.min(), values.max()
    mappable = None

    for i, row_param in enumerate(params):
        for j, col_param in enumerate(params):
            ax = axes[i][j]
            if i == j:
                x = df[row_param].to_numpy(dtype=float)
                x_lo, x_hi = bounds[row_param]
                grid_1d = np.linspace(x_lo, x_hi, resolution)
                span = max(x_hi - x_lo, 1e-9)
                fitted_gp = False
                if have_sklearn and len(x) >= 4:
                    try:
                        ls_lo = span * gp_min_length_scale_frac
                        kernel = ConstantKernel(1.0, (1e-5, 1e5)) * RBF(
                            length_scale=max(span / 4, ls_lo * 1.5), length_scale_bounds=(ls_lo, span * 2)
                        ) + WhiteKernel(noise_level=1.0, noise_level_bounds=(1e-5, 1e5))
                        gp = GaussianProcessRegressor(kernel=kernel, normalize_y=True,
                                                       n_restarts_optimizer=3)
                        with warnings.catch_warnings():
                            warnings.simplefilter("ignore", category=ConvergenceWarning)
                            gp.fit(x.reshape(-1, 1), values)
                        gp_mean, gp_std = gp.predict(grid_1d.reshape(-1, 1), return_std=True)
                        ax.fill_between(grid_1d, gp_mean - 1.96 * gp_std, gp_mean + 1.96 * gp_std,
                                        color="tab:blue", alpha=0.2, zorder=1, linewidth=0)
                        ax.plot(grid_1d, gp_mean, color="tab:blue", lw=1.5, zorder=2, alpha=0.9)
                        fitted_gp = True
                    except Exception:
                        fitted_gp = False
                if not fitted_gp and len(x) >= diagonal_degree + 2:
                    try:
                        coeffs = np.polyfit(x, values, deg=diagonal_degree)
                        ax.plot(grid_1d, np.polyval(coeffs, grid_1d), color="tab:blue",
                                lw=1.5, zorder=2, alpha=0.8)
                    except Exception:
                        pass
                ax.scatter(x, values, c=values, cmap="viridis", vmin=vmin, vmax=vmax,
                           s=28, edgecolors="k", linewidths=0.4, zorder=3)
                ax.axvline(best_point[row_param], color="red", ls="--", lw=1, zorder=2)
                ax.set_xlim(x_lo, x_hi)
            else:
                x = df[col_param].to_numpy(dtype=float)
                y = df[row_param].to_numpy(dtype=float)
                x_lo, x_hi = bounds[col_param]
                y_lo, y_hi = bounds[row_param]
                grid_x, grid_y = np.meshgrid(
                    np.linspace(x_lo, x_hi, resolution),
                    np.linspace(y_lo, y_hi, resolution),
                )
                grid_z = None
                if len(x) >= 4:
                    try:
                        rbf = RBFInterpolator(np.column_stack([x, y]), values,
                                              kernel="thin_plate_spline", smoothing=smoothing)
                        grid_z = rbf(np.column_stack([grid_x.ravel(), grid_y.ravel()])).reshape(grid_x.shape)
                    except Exception:
                        grid_z = None
                if grid_z is not None:
                    cf = ax.contourf(grid_x, grid_y, grid_z, levels=14, cmap="viridis",
                                      vmin=vmin, vmax=vmax, alpha=0.9, zorder=1)
                    mappable = cf
                sc = ax.scatter(x, y, c=values, cmap="viridis", vmin=vmin, vmax=vmax,
                                 s=24, edgecolors="white", linewidths=0.6, zorder=3)
                if mappable is None:
                    mappable = sc
                ax.scatter([best_point[col_param]], [best_point[row_param]], marker="*",
                           s=170, c="red", edgecolors="black", linewidths=0.8, zorder=4)
                ax.set_xlim(x_lo, x_hi)
                ax.set_ylim(y_lo, y_hi)

            if i == n - 1:
                ax.set_xlabel(col_param, fontsize=9)
            else:
                ax.set_xticklabels([])
            if j == 0:
                ax.set_ylabel(row_param if i != j else metric, fontsize=9)
            else:
                ax.set_yticklabels([])
            ax.tick_params(labelsize=7)

    if mappable is not None:
        fig.colorbar(mappable, ax=axes, shrink=0.6, label=metric, pad=0.02)

    best_str = ", ".join(f"{k}={v}" for k, v in best_point.items())
    fig.suptitle(
        f"ax_tune.py optimization landscape ({len(df)} completed trials)\n"
        f"best: {best_str}  ({metric}={df[metric].max():.1f} max observed)",
        fontsize=11,
    )
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Wrote {out_path} ({n}x{n} grid, {len(df)} trials).")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ax-state", default=str(DEFAULT_AX_STATE_FILE),
                         help="Ax experiment state JSON produced by ax_tune.py tune (default: %(default)s)")
    parser.add_argument("--params", default=str(DEFAULT_PARAMS_FILE),
                         help="Parameter file (only used to pick which parameters/order to plot; "
                              "falls back to every tuned parameter in the Ax state if missing) (default: %(default)s)")
    parser.add_argument("--best", default=str(DEFAULT_BEST_FILE),
                         help="Best-params JSON written by ax_tune.py (--best-out); used as the plotted best "
                              "point so it always matches that file (falls back to recomputing it if missing) "
                              "(default: %(default)s)")
    parser.add_argument("--metric", default="elo", help="Metric column to plot (default: %(default)s)")
    parser.add_argument("--resolution", type=int, default=120,
                         help="Grid resolution per axis for the interpolated landscape background (default: %(default)s)")
    parser.add_argument("--smoothing", type=float, default=0.0,
                         help="RBF smoothing factor for the off-diagonal 2D landscapes (0 = exact interpolation "
                              "through every point, smooth but can look noisy; increase, e.g. 500-5000 depending "
                              "on the metric's scale, to damp noise and get a smoother regressed surface instead "
                              "of an exact fit) (default: %(default)s)")
    parser.add_argument("--diagonal-degree", type=int, default=2,
                         help="Degree of the least-squares polynomial trend curve fit on each diagonal marginal "
                              "plot (2 = a single parabola; does not pass through every point) -- only used as a "
                              "fallback when the GP uncertainty band below can't be fit (default: %(default)s)")
    parser.add_argument("--gp-min-length-scale-frac", type=float, default=0.05,
                         help="Lower bound on the diagonal GP's length scale, as a fraction of each parameter's "
                              "range (default 0.05 = 5%% of the range). The GP independently picks its own "
                              "wiggliness per parameter by maximum likelihood, which can occasionally pick a very "
                              "short length scale for one parameter and a flat/no-signal fit for the others -- "
                              "that's not a bug, it reflects a real difference in fitted likelihood, but can look "
                              "inconsistent across the grid. Raise this (e.g. 0.1-0.2) to force smoother, more "
                              "conservative diagonal curves everywhere (default: %(default)s)")
    parser.add_argument("--out", default=str(DEFAULT_OUT_FILE), help="Output image path (default: %(default)s)")
    args = parser.parse_args()

    df, params, bounds, best_point = load_trials(args.ax_state, args.params, args.best, args.metric)
    plot_landscape(df, params, bounds, best_point, args.metric, args.resolution, args.smoothing,
                   args.diagonal_degree, args.gp_min_length_scale_frac, args.out)


if __name__ == "__main__":
    main()
