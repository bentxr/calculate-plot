#!/usr/bin/env python3
"""Write tests/oracle/symlog.json: matplotlib 3.11.2's SymmetricalLogTransform and its inverse (arrays, through
NumPy's log and exp), SymmetricalLogLocator.tick_values / view_limits, and LogFormatterSciNotation's labels on
a symlog axis (set_locs with the axis' linthresh), as float.hex and text.

  tools/oracle/.venv/bin/python -I tools/oracle/make_symlog_oracle.py
"""
import json
import pathlib

import matplotlib
import numpy as np

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import scale, ticker  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(v):
    return float.hex(float(v))


PARAMS = [(10, 2, 1), (10, 1, 1), (10, 0.5, 0.3), (2, 1, 1), (np.e, 3, 2), (10, 1e-3, 1), (4, 100, 0.5)]
rng = np.random.default_rng(1017)
VALUES = [0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 0.5, 10.0, -100.0, 1e6, -1e-6, 1e-300, 1e300, float("inf"), float("nan"),
          1.999999, 2.000001] + [float(v) for v in rng.normal(0, 50, 40)] + [float(v) for v in rng.uniform(-3, 3, 20)]
transforms = []
for base, linthresh, linscale in PARAMS:
    t = scale.SymmetricalLogTransform(base, linthresh, linscale)
    fwd = t.transform(np.array(VALUES))
    finite = [v for v in fwd if np.isfinite(v)] + [0.0, 1.0, -1.0, 5.0, -7.5, 0.25]
    inv = t.inverted().transform(np.array(finite))
    transforms.append({"params": [h(base), h(linthresh), h(linscale)], "values": [h(v) for v in VALUES],
                       "forward": [h(v) for v in fwd], "inputs": [h(v) for v in finite], "inverse": [h(v) for v in inv]})

RANGES = [(-1, 1), (0, 10), (-10, 10), (-1000, 1000), (-5, 1e6), (0.1, 3), (3, 1e5), (-1e8, -3), (1, 2), (-2, 2),
          (-0.5, 0.5), (0, 1e20), (-1e20, 1e20), (5, 5.5), (-3.3, 7.7), (100, 1000), (-1e5, 0.0), (1e-3, 1e-1)]
locators = []
for base, linthresh, _ in PARAMS[:5]:
    for subs in (None, [1.0, 2.0, 5.0], [2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]):
        loc = ticker.SymmetricalLogLocator(base=base, linthresh=linthresh, subs=subs)
        for vmin, vmax in RANGES:
            case = {"base": h(base), "linthresh": h(linthresh), "subs": subs, "range": [h(vmin), h(vmax)]}
            try:
                case["ticks"] = [h(v) for v in loc.tick_values(vmin, vmax)]
            except Exception as e:  # (raise_if_exceeds only warns)
                case["error"] = str(e)
            for mode in ("data", "round_numbers"):
                with matplotlib.rc_context({"axes.autolimit_mode": mode}):
                    case[mode] = [h(v) for v in loc.view_limits(vmin, vmax)]
            locators.append(case)

labels = []
for base, linthresh, linscale in PARAMS[:5]:
    for vmin, vmax in RANGES:
        fig, ax = plt.subplots()
        ax.set_yscale("symlog", base=base, linthresh=linthresh, linscale=linscale)
        ax.set_ylim(vmin, vmax)
        f = ax.yaxis.get_major_formatter()
        locs = ax.yaxis.get_major_locator().tick_values(vmin, vmax)
        f.set_locs(locs)
        extra = [0.0, linthresh, -linthresh, base, 3 * base, 0.5, -2.5 * base ** 3]
        xs = [float(v) for v in locs] + extra
        labels.append({"base": h(base), "linthresh": h(linthresh), "range": [h(vmin), h(vmax)], "x": [h(v) for v in xs],
                       "labels": [f(v) for v in xs]})
        plt.close(fig)

out = {"_note": "matplotlib 3.11.2, NumPy 2.5.3, tools/oracle/make_symlog_oracle.py", "transforms": transforms,
       "locators": locators, "labels": labels}
(ROOT / "tests" / "oracle" / "symlog.json").write_text(json.dumps(out) + "\n")
print(len(transforms), "transforms,", len(locators), "locator cases,", len(labels), "label cases")
