#!/usr/bin/env python3
"""Write data/colormaps.json (matplotlib 3.11.2's listed colormaps used so far: their colour lists)
and tests/oracle/colormapping.json (Normalize + colormap of random float data, as
ScalarMappable.to_rgba does it for scatter's c values).

  tools/oracle/.venv/bin/python -I tools/oracle/make_colormaps.py
"""
import json
import pathlib
import random

import matplotlib
import numpy as np
from matplotlib import cm, colors

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


NAMES = ["viridis"]
maps = {}
for name in NAMES:
    cmap = matplotlib.colormaps[name]
    assert isinstance(cmap, colors.ListedColormap)
    maps[name] = [[h(v) for v in c] for c in np.asarray(cmap.colors, float)]
(ROOT / "data" / "colormaps.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/_cm_listed.py (the listed colormaps' colours),",
        "written by tools/oracle/make_colormaps.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "colormaps": maps}, indent=0) + "\n")

rnd = random.Random(21)
cases = []
for i in range(300):
    n = rnd.randint(1, 40)
    kind = rnd.random()
    if kind < 0.6:
        data = [rnd.uniform(-50, 50) for _ in range(n)]
    elif kind < 0.8:
        data = [rnd.choice([0.0, 0.25, 0.5, 1.0, 2.0]) for _ in range(n)]
    else:
        data = [rnd.uniform(0, 1) * 10 ** rnd.randint(-8, 8) for _ in range(n)]
    alpha = rnd.choice([None, None, 0.5, 1.0, 0.0])
    sm = cm.ScalarMappable(norm=colors.Normalize(), cmap="viridis")
    rgba = sm.to_rgba(np.asarray(data, float), alpha)
    cases.append({"data": [h(v) for v in data], "alpha": alpha, "rgba": [[h(v) for v in r] for r in rgba]})
(ROOT / "tests" / "oracle" / "colormapping.json").write_text(json.dumps(cases, indent=0) + "\n")
print(len(maps), "colormaps,", len(cases), "mappings")
