#!/usr/bin/env python3
"""Write data/marker_paths.json: the unit polygons and star matplotlib 3.11.2's markers use
(Path.unit_regular_polygon(5, 6, 8) and unit_regular_star(5, 0.381966)). Their vertices come from
NumPy's vectorised sin and cos, whose last bits depend on NumPy's build, so calculate-plot takes
them as data instead of computing them.

  tools/oracle/.venv/bin/python -I tools/oracle/make_marker_paths.py
"""
import json
import pathlib

import matplotlib
from matplotlib.path import Path

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def dump(path):
    return {"vertices": [float.hex(float(v)) for v in path.vertices.ravel()],
            "codes": None if path.codes is None else [int(c) for c in path.codes]}


paths = {f"polygon{n}": dump(Path.unit_regular_polygon(n)) for n in (5, 6, 8)}
paths["star5"] = dump(Path.unit_regular_star(5, innerCircle=0.381966))
(ROOT / "data" / "marker_paths.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (unit_regular_polygon, unit_regular_star)",
        "as lib/matplotlib/markers.py uses them, written by tools/oracle/make_marker_paths.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "paths": paths}, indent=0) + "\n")
print(", ".join(paths))
