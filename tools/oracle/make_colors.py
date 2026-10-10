#!/usr/bin/env python3
"""Write data/colors.json (matplotlib 3.11.2's map of colour names, as colors._colors_full_map
holds it) and tests/oracle/colors.json (to_rgba of many specifications, recorded from matplotlib).

  tools/oracle/.venv/bin/python -I tools/oracle/make_colors.py
"""
import json
import pathlib
import random

import matplotlib
from matplotlib import colors

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"

names = {k: (list(v) if isinstance(v, tuple) else v) for k, v in colors._colors_full_map.items()}
(ROOT / "data" / "colors.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/_color_data.py and lib/matplotlib/colors.py",
        "(the map of colour names), written by tools/oracle/make_colors.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt. The xkcd colour names come from the xkcd colour survey",
        "(https://xkcd.com/color/rgb/) under CC0 1.0 (public domain); the CSS4 names are the W3C's.",
    ],
    "colors": names}, indent=0, ensure_ascii=False) + "\n")

random.seed(5)
specs = list(names) + [n.upper() for n in list(names)[:200]] + ["none", "None", "NONE"]
specs += [f"C{i}" for i in range(12)] + ["0", "1", "0.25", "0.5", ".75", "1e-1"]
specs += ["#%06x" % random.getrandbits(24) for _ in range(100)] + ["#%03x" % random.getrandbits(12) for _ in range(40)]
specs += ["#%08x" % random.getrandbits(32) for _ in range(60)] + ["#%04X" % random.getrandbits(16) for _ in range(40)]
cases = []
for spec in specs:
    for alpha in [None, 0.5, 0.0, 1.0]:
        try:
            rgba = colors.to_rgba(spec, alpha)
            cases.append({"spec": spec, "alpha": alpha, "rgba": [float.hex(float(x)) for x in rgba]})
        except ValueError:
            cases.append({"spec": spec, "alpha": alpha, "error": True})
for bad in ["notacolor", "#12345", "1.5", "-0.1", "#ggg", "C", "Cx"]:
    try:
        colors.to_rgba(bad)
        cases.append({"spec": bad, "alpha": None, "rgba": "unexpected"})
    except ValueError:
        cases.append({"spec": bad, "alpha": None, "error": True})
(ROOT / "tests" / "oracle" / "colors.json").write_text(json.dumps(cases, separators=(",", ":"), ensure_ascii=False) + "\n")
print(len(names), "names,", len(cases), "cases")
