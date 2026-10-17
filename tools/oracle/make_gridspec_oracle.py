#!/usr/bin/env python3
"""Write tests/oracle/gridspec.json: subplot positions from matplotlib 3.11.2's GridSpec (random grids,
spacings, ratios, subplot parameters and cell spans), as SubplotSpec.get_position gives them.

  tools/oracle/.venv/bin/python -I tools/oracle/make_gridspec_oracle.py
"""
import json
import pathlib
import random

import matplotlib
from matplotlib.figure import Figure
from matplotlib.gridspec import GridSpec

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


rnd = random.Random(3)
cases = []
for i in range(400):
    nrows, ncols = rnd.randint(1, 5), rnd.randint(1, 5)
    fig = Figure()
    params = {}
    if rnd.random() < 0.5:
        left = round(rnd.uniform(0.0, 0.3), rnd.choice([2, 3, 7]))
        bottom = round(rnd.uniform(0.0, 0.3), rnd.choice([2, 3, 7]))
        params = {"left": left, "bottom": bottom, "right": round(rnd.uniform(0.6, 1.0), 3),
                  "top": round(rnd.uniform(0.6, 1.0), 3)}
        fig.subplots_adjust(**params)
    kw = {}
    if rnd.random() < 0.5:
        kw["wspace"] = rnd.choice([0, 0.05, 0.2, 0.333, 1.0])
    if rnd.random() < 0.5:
        kw["hspace"] = rnd.choice([0, 0.05, 0.2, 0.333, 1.0])
    if rnd.random() < 0.3:
        kw["width_ratios"] = [rnd.choice([1, 2, 3, 0.5, 1.7]) for _ in range(ncols)]
    if rnd.random() < 0.3:
        kw["height_ratios"] = [rnd.choice([1, 2, 3, 0.5, 1.7]) for _ in range(nrows)]
    gs = GridSpec(nrows, ncols, figure=fig, **kw)
    spans = []
    for _ in range(4):
        r0 = rnd.randrange(nrows)
        r1 = rnd.randint(r0 + 1, nrows)
        c0 = rnd.randrange(ncols)
        c1 = rnd.randint(c0 + 1, ncols)
        spans.append({"rows": [r0, r1], "cols": [c0, c1],
                      "extents": [h(v) for v in gs[r0:r1, c0:c1].get_position(fig).extents]})
    cases.append({"nrows": nrows, "ncols": ncols, "subplotpars": params, "gridspec": kw, "spans": spans})
(ROOT / "tests" / "oracle" / "gridspec.json").write_text(json.dumps(cases, indent=0) + "\n")
print(len(cases), "grids")
