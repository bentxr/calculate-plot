#!/usr/bin/env python3
"""Write tests/oracle/minor_ticker.json: the minor ticks of linear axes in matplotlib 3.11.2
(AutoMinorLocator, which rcParams["xtick.minor.visible"] turns on), for 700 random axes: the view,
the major ticks, the number of divisions asked for (rcParams["xtick.minor.ndivs"]: 'auto' or a
number), what the locator gives and what Axis.get_minorticklocs keeps of it (the ones not on a major
tick). Doubles are written as exact hex.

  tools/oracle/.venv/bin/python -I tools/oracle/make_minor_ticker_oracle.py
"""
import json
import pathlib
import random
import warnings

import matplotlib
matplotlib.use("agg")
import matplotlib.pyplot as plt

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")
h = float.hex
rnd = random.Random(41)
cases = []
for i in range(700):
    w, hgt = rnd.choice([1.5, 2.5, 4.0, 6.4]), rnd.choice([1.2, 2.0, 3.0, 4.8])
    axis_name = rnd.choice("xy")
    ndivs = rnd.choice(["auto", "auto", "auto", 1, 2, 3, 4, 5, 10, 0, 7])
    kind = rnd.random()
    if kind < 0.5:
        center = rnd.choice([0.0, rnd.uniform(-1, 1) * 10 ** rnd.randint(-6, 9)])
        width = 10 ** rnd.uniform(-9, 9) * max(1.0, abs(center)) ** rnd.choice([0, 0, 1])
        lo = center - width * rnd.random()
        hi = lo + width * rnd.uniform(0.01, 1)
    elif kind < 0.85:  # round numbers, where a minor tick falls on a major one or on the edge
        step = rnd.choice([1, 2, 2.5, 5]) * 10.0 ** rnd.randint(-4, 5)
        lo = step * rnd.randint(-40, 40)
        hi = lo + step * rnd.randint(1, 12)
    else:  # a view too small or too odd for two major ticks
        lo = rnd.choice([0.0, 1.0, -3.5, 1e6])
        hi = lo + rnd.choice([0.0, 1e-13, 1e-9]) * max(1.0, abs(lo))
    if rnd.random() < 0.12:
        lo, hi = hi, lo
    with matplotlib.rc_context({f"{axis_name}tick.minor.visible": True, f"{axis_name}tick.minor.ndivs": ndivs}):
        fig, ax = plt.subplots(figsize=(w, hgt))
        getattr(ax, f"set_{axis_name}lim")(lo, hi)
        axis = getattr(ax, f"{axis_name}axis")
        major = axis.get_majorticklocs()
        raw = axis.minor.locator()
        minor = axis.get_minorticklocs()
        cases.append({"axis": axis_name, "view": [h(float(v)) for v in axis.get_view_interval()], "tick_space": int(axis.get_tick_space()),
                      "ndivs": ndivs, "major": [h(float(v)) for v in major], "raw": [h(float(v)) for v in raw],
                      "minor": [h(float(v)) for v in minor]})
        plt.close(fig)
text = json.dumps(cases, separators=(",", ":")).replace('{"axis"', '\n{"axis"')
(ROOT / "tests" / "oracle" / "minor_ticker.json").write_text(text + "\n")
print(len(cases), "axes;", sum(1 for c in cases if not c["raw"]), "without minor ticks;", sum(len(c["raw"]) != len(c["minor"]) for c in cases),
      "with minor ticks on major ones; nan in", sum(any(v == "nan" for v in c["raw"]) for c in cases))
