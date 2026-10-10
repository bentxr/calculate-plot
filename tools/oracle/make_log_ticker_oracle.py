#!/usr/bin/env python3
"""Write tests/oracle/log_ticker.json: what matplotlib 3.11.2's log axes compute for random view
intervals and axis lengths: LogLocator's major and minor ticks (minor ticks without those on major
ones, as Axis.get_minorticklocs), LogFormatterSciNotation's labels, and LogLocator's nonsingular and
view_limits.

  tools/oracle/.venv/bin/python -I tools/oracle/make_log_ticker_oracle.py
"""
import json
import math
import pathlib
import random

import matplotlib

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


rnd = random.Random(12)
cases = []
for i in range(600):
    w, hgt = rnd.choice([1.5, 2.5, 4.0, 6.4]), rnd.choice([1.2, 2.0, 3.0, 4.8])
    fig, ax = plt.subplots(figsize=(w, hgt))
    axis_name = rnd.choice("xy")
    getattr(ax, f"set_{axis_name}scale")("log")
    kind = rnd.random()
    if kind < 0.6:
        lo = 10 ** rnd.uniform(-8, 6)
        hi = lo * 10 ** rnd.uniform(0.05, 14)
    elif kind < 0.8:  # within a decade
        lo = 10 ** rnd.uniform(-3, 3)
        hi = lo * rnd.uniform(1.01, 9.5)
    else:  # round decades
        lo = 10.0 ** rnd.randint(-6, 3)
        hi = lo * 10.0 ** rnd.randint(1, 12)
    if rnd.random() < 0.15:
        lo, hi = hi, lo
    getattr(ax, f"set_{axis_name}lim")(lo, hi)
    axis = getattr(ax, f"{axis_name}axis")
    major = axis.get_majorticklocs()
    minor = axis.get_minorticklocs()
    major_labels = axis.get_major_formatter().format_ticks(major)
    minor_labels = axis.get_minor_formatter().format_ticks(minor)
    loc = axis.get_major_locator()
    a, b = rnd.choice([(lo, hi), (lo, lo), (0, hi), (-1, -0.5), (math.inf, 1)])
    cases.append({"axis": axis_name, "figsize": [w, hgt], "view": [h(v) for v in axis.get_view_interval()],
                  "tick_space": int(axis.get_tick_space()),
                  "major": [h(v) for v in major], "minor": [h(v) for v in minor],
                  "major_labels": major_labels, "minor_labels": minor_labels,
                  "nonsingular_in": [h(a), h(b)], "nonsingular": [h(v) for v in loc.nonsingular(a, b)],
                  "view_limits": [h(v) for v in loc.view_limits(lo, hi)]})
    plt.close(fig)
(ROOT / "tests" / "oracle" / "log_ticker.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "log axes")
