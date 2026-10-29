#!/usr/bin/env python3
"""Write data/marker_paths.json: the unit polygons and star matplotlib 3.11.2's markers use
(Path.unit_regular_polygon(5, 6, 8) and unit_regular_star(5, 0.381966)), the star of the '*'
hatch (unit_regular_star(5), inner circle 0.5), the polygon of a CirclePolygon (unit_regular_polygon(20): barbs'
empty barbs), and the half width of the half-filled 'h' marker
(np.abs(np.cos(5 * np.pi / 6.)) in MarkerStyle._set_hexagon1). They come from NumPy's vectorised sin
and cos, whose last bits depend on NumPy's build, so calculate-plot takes them as data instead of
computing them.

  tools/oracle/.venv/bin/python -I tools/oracle/make_marker_paths.py
"""
import json
import pathlib

import matplotlib
import numpy as np
from matplotlib.path import Path

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def dump(path):
    return {"vertices": [float.hex(float(v)) for v in path.vertices.ravel()],
            "codes": None if path.codes is None else [int(c) for c in path.codes]}


paths = {f"polygon{n}": dump(Path.unit_regular_polygon(n)) for n in (5, 6, 8, 20)}
paths["star5"] = dump(Path.unit_regular_star(5, innerCircle=0.381966))
paths["star5_hatch"] = dump(Path.unit_regular_star(5))
(ROOT / "data" / "marker_paths.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (unit_regular_polygon, unit_regular_star)",
        "as lib/matplotlib/markers.py, lib/matplotlib/hatch.py and lib/matplotlib/patches.py (CirclePolygon) use them,",
        "and a constant of markers.py, written by",
        "tools/oracle/make_marker_paths.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "paths": paths,
    "constants": {"hexagon1_half_x": float.hex(float(np.abs(np.cos(5 * np.pi / 6.))))}}, indent=0) + "\n")
print(", ".join(paths))
