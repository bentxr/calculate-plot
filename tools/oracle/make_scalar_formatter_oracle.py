#!/usr/bin/env python3
"""Write tests/oracle/scalar_formatter.json: ScalarFormatter's labels and offset text in matplotlib
3.11.2 under the rcParams that shape them (axes.formatter.useoffset, .offset_threshold, .limits and
.use_mathtext, and axes.unicode_minus), for the ticks of the default locator over many view limits.
Doubles are written as exact hex.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_scalar_formatter_oracle.py
"""
import json
import math
import os
import pathlib
import random
import sys

import matplotlib
matplotlib.use("agg")
from matplotlib import ticker

if os.environ.get("LC_ALL") != "C.UTF-8" or os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    sys.exit("set LC_ALL=C.UTF-8 and MPL_IGNORE_SYSTEM_FONTS=1")
assert matplotlib.__version__ == "3.11.2"

OUT = pathlib.Path(__file__).resolve().parents[2] / "tests" / "oracle"
h = float.hex
rnd = random.Random(31)


def ranges():
    fixed = [(0, 1), (0, 10), (-1, 1), (0, 7), (1, 2), (0.1, 0.3), (1e6, 1e6 + 1), (1e9, 1.000001e9), (-5, -3), (3, 3), (0, 0),
             (5, 1), (1e-9, 3e-9), (12345.678, 12345.679), (-1e-3, 1e-3), (100, 101), (99.99, 100.01), (0.9999, 1.0001),
             (2.5, 2.5000000001), (-7e7, 8e7), (1e15, 1e15 + 300), (0, 2 * math.pi), (-0.5, 10.5), (1950, 2025), (0.001, 1000),
             (1e300, 1e301), (-1e-300, 1e-300), (-1234507, -1234500), (1e5, 2e5), (1e-5, 2e-5), (1e6, 2e6), (9e5, 1.1e6),
             (1e-6, 2e-6), (1e-4, 2e-4), (0, 1e5), (0, 99999), (-1e-5, 0), (10, 12), (100, 100.5), (1000, 1001), (10000, 10001),
             (1e21, 1e21 + 1e6), (1e22, 1e22 + 1e7), (1e23, 1e23 + 1e8), (1e24, 1e24 + 1e9), (3e22, 3e22 + 1e8)]
    for a, b in fixed:
        yield float(a), float(b)
    for _ in range(450):
        center = rnd.choice([0.0, rnd.uniform(-1, 1) * 10 ** rnd.randint(-8, 12)])
        width = 10 ** rnd.uniform(-12, 12) * max(1.0, abs(center)) ** rnd.choice([0, 0, 1])
        a = center - width * rnd.random()
        b = a + width * rnd.uniform(0.01, 1)
        if rnd.random() < 0.1:
            a, b = b, a
        yield a, b
    for _ in range(150):  # round numbers, where the edge tolerances matter
        step = rnd.choice([1, 2, 2.5, 5]) * 10.0 ** rnd.randint(-6, 6)
        a = step * rnd.randint(-50, 50)
        yield a, a + step * rnd.randint(1, 30)


SINGLE = ([{"axes.formatter.useoffset": False}, {"axes.formatter.use_mathtext": True}, {"axes.unicode_minus": False}]
          + [{"axes.formatter.offset_threshold": n} for n in (1, 2, 3, 5, 6, 10, 0, -1, -2, -400, 22, 23)]
          + [{"axes.formatter.limits": list(l)} for l in ((-3, 3), (0, 0), (2, 2), (-2, -2), (-7, 7), (1, 1), (0, 1), (-1, 0),
                                                           (5, -5), (-1, 1), (-300, 300), (3, 3), (-4, -4))])


def settings():
    """The rcParams of a case: one alone, or several at random."""
    if rnd.random() < 0.5:
        return dict(rnd.choice(SINGLE))
    rc = {}
    for _ in range(rnd.randint(2, 4)):
        rc.update(rnd.choice(SINGLE))
    return rc


cases = []
for vmin, vmax in ranges():
    ticks = [float(t) for t in ticker.AutoLocator().tick_values(vmin, vmax)]
    for rc in [settings() for _ in range(4)]:
        with matplotlib.rc_context(rc):
            formatter = ticker.ScalarFormatter()
            formatter.axis = ticker._DummyAxis()
            formatter.axis.set_view_interval(vmin, vmax)
            formatter.set_locs(ticks)
            labels = [formatter(t) for t in ticks]
            offset = formatter.get_offset()
        cases.append({"vmin": h(vmin), "vmax": h(vmax), "rc": rc, "ticks": [h(t) for t in ticks], "labels": labels, "offset": offset})
text = json.dumps(cases, separators=(",", ":"), ensure_ascii=False).replace('{"vmin"', '\n{"vmin"')
(OUT / "scalar_formatter.json").write_text(text + "\n")
print(len(cases), "cases,", sum(1 for c in cases if c["offset"]), "with an offset text")
