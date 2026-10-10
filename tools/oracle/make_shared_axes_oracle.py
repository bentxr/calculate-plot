#!/usr/bin/env python3
"""Write tests/oracle/shared_axes.json: what matplotlib 3.11.2's plt.subplots(sharex=, sharey=) sets
up, for every grid up to 3 x 3 and every way of sharing: the Axes each one shares an axis with (in
the order Axes._shared_axes gives them), the Axes whose axis the shared locator belongs to (before
and after a log scale is set on one of them), which tick labels stay on, whether the offset texts
show, and who autoscales after limits are set on one of them.

  tools/oracle/.venv/bin/python -I tools/oracle/make_shared_axes_oracle.py
"""
import itertools
import json
import pathlib

import matplotlib

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"

SHARING = ["none", "all", "row", "col"]
RC = [{}, {"xtick.labelbottom": False, "xtick.labeltop": True, "ytick.labelleft": False, "ytick.labelright": True}]


def state(axs):
    index = {ax: i for i, ax in enumerate(axs)}
    out = {}
    for name in "xy":
        axes = [ax._axis_map[name] for ax in axs]
        out[name] = {
            "siblings": [[index[s] for s in ax._shared_axes[name].get_siblings(ax)] for ax in axs],
            "owner": [index[axis.major.locator.axis.axes] for axis in axes],
            "label1": [bool(axis._major_tick_kw["label1On"]) for axis in axes],
            "label2": [bool(axis._major_tick_kw["label2On"]) for axis in axes],
            "minor_label1": [bool(axis._minor_tick_kw["label1On"]) for axis in axes],
            "minor_label2": [bool(axis._minor_tick_kw["label2On"]) for axis in axes],
            "offset_text": [bool(axis.offsetText.get_visible()) for axis in axes],
            "autoscale": [bool(axis._get_autoscale_on()) for axis in axes],
            "scale": [axis.get_scale() for axis in axes],
        }
    return out


cases = []
for rc, nrows, ncols, sharex, sharey in itertools.product(range(len(RC)), [1, 2, 3], [1, 2, 3], SHARING, SHARING):
    plt.rcdefaults()
    plt.rcParams.update(RC[rc])
    fig, axs = plt.subplots(nrows, ncols, sharex=sharex, sharey=sharey, squeeze=False)
    axs = list(axs.flat)
    case = {"rc": RC[rc], "nrows": nrows, "ncols": ncols, "sharex": sharex, "sharey": sharey, "made": state(axs)}
    on = (len(axs) - 1) // 2
    axs[on].set_xscale("log")
    axs[-1].set_yscale("log")
    case["log"] = {"x_on": on, "y_on": len(axs) - 1, "state": state(axs)}
    axs[0].set_xlim(2, 5)
    axs[on].set_ylim(3, 4)
    case["limits"] = {"x_on": 0, "y_on": on, "state": state(axs),
                      "xlim": [list(map(float, ax.get_xlim())) for ax in axs],
                      "ylim": [list(map(float, ax.get_ylim())) for ax in axs]}
    cases.append(case)
    plt.close(fig)

(ROOT / "tests" / "oracle" / "shared_axes.json").write_text(json.dumps(cases, separators=(",", ":")) + "\n")
print(len(cases), "grids")
