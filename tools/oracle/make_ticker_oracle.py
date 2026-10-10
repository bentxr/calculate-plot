#!/usr/bin/env python3
"""Write tests/oracle/ticker.json and tests/oracle/text_extent.json from matplotlib 3.11.2.

ticker.json: for many view limits, the ticks of the default locator (AutoLocator, no axis, so 9
bins) and of MaxNLocator variants, then ScalarFormatter's labels and offset text for those ticks
(the default rcParams). Doubles are written as exact hex. text_extent.json: RendererAgg's
get_text_width_height_descent for strings, sizes and dpis.
Run with LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 and the oracle venv.
"""
import json
import math
import os
import pathlib
import random
import sys

import matplotlib
matplotlib.use("agg")
import numpy as np
from matplotlib import ticker
from matplotlib.backends.backend_agg import RendererAgg
from matplotlib.font_manager import FontProperties, fontManager

if os.environ.get("LC_ALL") != "C.UTF-8" or os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    sys.exit("set LC_ALL=C.UTF-8 and MPL_IGNORE_SYSTEM_FONTS=1")
assert matplotlib.__version__ == "3.11.2"

OUT = pathlib.Path(__file__).resolve().parents[2] / "tests" / "oracle"
h = float.hex
random.seed(11)


def ranges():
    fixed = [(0, 1), (0, 10), (-1, 1), (0, 7), (1, 2), (0.1, 0.3), (1e6, 1e6 + 1), (1e9, 1.000001e9), (-5, -3),
             (3, 3), (0, 0), (5, 1), (1e-9, 3e-9), (12345.678, 12345.679), (-1e-3, 1e-3), (100, 101),
             (99.99, 100.01), (0.9999, 1.0001), (2.5, 2.5000000001), (-7e7, 8e7), (1e15, 1e15 + 300),
             (0, 2 * math.pi), (-0.5, 10.5), (1950, 2025), (0.001, 1000), (1e300, 1e301), (-1e-300, 1e-300)]
    for a, b in fixed:
        yield float(a), float(b)
    for _ in range(1500):
        center = random.choice([0.0, random.uniform(-1, 1) * 10 ** random.randint(-8, 12)])
        width = 10 ** random.uniform(-12, 12) * max(1.0, abs(center)) ** random.choice([0, 0, 1])
        a = center - width * random.random()
        b = a + width * random.uniform(0.01, 1)
        if random.random() < 0.1:
            a, b = b, a
        yield a, b
    for _ in range(300):  # round numbers, where the edge tolerances matter
        step = random.choice([1, 2, 2.5, 5]) * 10.0 ** random.randint(-6, 6)
        a = step * random.randint(-50, 50)
        yield a, a + step * random.randint(1, 30)


def locators():
    yield "auto", {}, ticker.AutoLocator()
    yield "maxn5", {"nbins": 5}, ticker.MaxNLocator(nbins=5)
    yield "maxn10int", {"nbins": 10, "integer": True}, ticker.MaxNLocator(nbins=10, integer=True)
    yield "maxn4steps", {"nbins": 4, "steps": [1, 3, 10]}, ticker.MaxNLocator(nbins=4, steps=[1, 3, 10])
    yield "maxn7sym", {"nbins": 7, "symmetric": True, "prune": "both"}, ticker.MaxNLocator(nbins=7, symmetric=True, prune="both")


cases = []
for vmin, vmax in ranges():
    for name, params, locator in locators():
        ticks = locator.tick_values(vmin, vmax)
        entry = {"vmin": h(vmin), "vmax": h(vmax), "locator": name, "params": params,
                 "ticks": [h(float(t)) for t in ticks]}
        if name == "auto":
            formatter = ticker.ScalarFormatter()
            formatter.axis = ticker._DummyAxis()
            formatter.axis.set_view_interval(vmin, vmax)
            formatter.set_locs(ticks)
            entry["labels"] = [formatter(t) for t in ticks]
            entry["offset"] = formatter.get_offset()
        cases.append(entry)
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "ticker.json").write_text(json.dumps(cases, separators=(",", ":"), ensure_ascii=False) + "\n")

renderer = RendererAgg(640, 480, 100)
extents = []
for s in ["0", "1.00", "−0.75", "x", "value", "Default figure", "Ag", "fi", "W.V", " ", "", "100%",
          "αβγ", "Hello, world", "1e6", "+1.234e3"]:
    for size in [6.0, 10.0, 12.0, 17.5]:
        for dpi in [72.0, 100.0, 150.0]:
            renderer.dpi = dpi
            prop = FontProperties(size=size)
            w, hh, d = renderer.get_text_width_height_descent(s, prop, ismath=False)
            fonts = [pathlib.Path(str(getattr(f, "path", f))).name for f in fontManager._find_fonts_by_props(prop)]
            extents.append({"s": s, "size": size, "dpi": dpi, "fonts": fonts,
                            "width": h(float(w)), "height": h(float(hh)), "descent": h(float(d))})
(OUT / "text_extent.json").write_text(json.dumps(extents, ensure_ascii=False, indent=0) + "\n")
print(len(cases), "ticker cases,", len(extents), "text extents")
