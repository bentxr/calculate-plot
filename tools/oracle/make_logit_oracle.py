#!/usr/bin/env python3
"""Write tests/oracle/logit.json: matplotlib 3.11.2's LogitTransform (mask and clip) and LogisticTransform, the
LogitLocator's major and minor ticks (tick_values for a number of bins, nonsingular with a minpos, view_limits)
and LogitFormatter's labels (major and minor, after set_locs), as float.hex and text.

  tools/oracle/.venv/bin/python -I tools/oracle/make_logit_oracle.py
"""
import json
import pathlib
import warnings

import matplotlib
import numpy as np
from matplotlib import scale, ticker

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")


def h(v):
    return float.hex(float(v))


rng = np.random.default_rng(1018)
VALUES = [0.0, 1.0, 0.5, -0.1, 1.5, 1e-9, 1 - 1e-9, 0.25, 0.75, 0.999, 0.001, float("nan"), float("inf")] + \
    [float(v) for v in rng.uniform(0, 1, 40)]
transforms = []
for nonpositive in ("mask", "clip"):
    t = scale.LogitTransform(nonpositive)
    fwd = t.transform(np.array(VALUES))
    back = [v for v in fwd if np.isfinite(v)] + [-3.0, 0.0, 2.5, -1000.0, 1000.0]
    inv = t.inverted().transform(np.array(back))
    transforms.append({"nonpositive": nonpositive, "values": [h(v) for v in VALUES], "forward": [h(v) for v in fwd],
                       "inputs": [h(v) for v in back], "inverse": [h(v) for v in inv]})


class FakeAxes:
    name = "rectilinear"


class FakeAxis:
    def __init__(self, space, minpos):
        self.space, self.minpos = space, minpos
        self.axes = FakeAxes()

    def get_tick_space(self):
        return self.space

    def get_minpos(self):
        return self.minpos


RANGES = [(1e-7, 1 - 1e-7), (0.1, 0.9), (0.2, 0.8), (0.4, 0.6), (0.01, 0.5), (0.5, 0.999), (1e-5, 0.3), (0.45, 0.55),
          (0.01, 0.99), (1e-12, 1 - 1e-12), (0.3, 0.31), (0.001, 0.002), (0.9, 0.9999), (-0.5, 1.5), (0.0, 1.0),
          (0.5, 0.5), (float("nan"), 0.5)]
locators = []
for space in (2, 5, 9, 14):
    for minpos in (1e-7, 0.05, float("inf")):
        for minor in (False, True):
            loc = ticker.LogitLocator(minor=minor)
            loc.axis = FakeAxis(space, minpos)
            for vmin, vmax in RANGES:
                case = {"space": space, "minpos": h(minpos), "minor": minor, "range": [h(vmin), h(vmax)]}
                case["nonsingular"] = [h(v) for v in loc.nonsingular(vmin, vmax)]
                try:
                    case["ticks"] = [h(v) for v in loc.tick_values(vmin, vmax)]
                except Exception as e:
                    case["error"] = str(e)
                for mode in ("data", "round_numbers"):
                    with matplotlib.rc_context({"axes.autolimit_mode": mode}):
                        try:
                            case[mode] = [h(v) for v in loc.view_limits(*loc.nonsingular(vmin, vmax))]
                        except Exception as e:
                            case[mode + "_error"] = str(e)
                locators.append(case)

labels = []
for vmin, vmax in RANGES[:13]:
    for space in (5, 9):
        for minor in (False, True):
            for overline in (False, True):
                loc = ticker.LogitLocator(minor=minor)
                loc.axis = FakeAxis(space, 1e-7)
                locs = loc.tick_values(vmin, vmax)
                f = ticker.LogitFormatter(minor=minor, use_overline=overline)
                f.set_locs(locs)
                labels.append({"space": space, "minor": minor, "overline": overline, "range": [h(vmin), h(vmax)],
                               "locs": [h(v) for v in locs], "labels": [f(v) for v in locs]})

out = {"_note": "matplotlib 3.11.2, NumPy 2.5.3, tools/oracle/make_logit_oracle.py", "transforms": transforms,
       "locators": locators, "labels": labels}
(ROOT / "tests" / "oracle" / "logit.json").write_text(json.dumps(out) + "\n")
print(len(transforms), "transforms,", len(locators), "locator cases,", len(labels), "label cases")
