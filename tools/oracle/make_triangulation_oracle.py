#!/usr/bin/env python3
"""Write tests/oracle/triangulation.json: matplotlib 3.11.2's Delaunay triangulations (tri.Triangulation(x, y),
through Qhull 8.0.2 with "qhull d Qt Qbb Qc Qz") of point sets: random ones of several sizes and spreads, grids
(cocircular points), sets with duplicate points, shifted far from the origin; their triangles and neighbors; and the
errors matplotlib raises (fewer than 3 points, fewer than 3 unique points, collinear points).

  tools/oracle/.venv/bin/python -I tools/oracle/make_triangulation_oracle.py
"""
import json
import pathlib

import numpy as np
import matplotlib

matplotlib.use("agg")
from matplotlib.tri import Triangulation  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
rng = np.random.default_rng(1221)

SETS = []
for n in (3, 4, 5, 10, 37, 100, 400):
    SETS.append((f"random {n}", rng.uniform(-1, 1, n), rng.uniform(-1, 1, n)))
SETS.append(("random wide", rng.uniform(-1e6, 1e6, 60), rng.uniform(-1e-3, 1e-3, 60)))
SETS.append(("random far", 1e7 + rng.uniform(0, 1, 50), -3e6 + rng.uniform(0, 1, 50)))
gx, gy = np.meshgrid(np.arange(6.0), np.arange(5.0))
SETS.append(("grid", gx.ravel(), gy.ravel()))
gx, gy = np.meshgrid(np.linspace(0, 1, 9), np.linspace(-2, 2, 7))
SETS.append(("grid linspace", gx.ravel(), gy.ravel()))
t = np.linspace(0, 2 * np.pi, 12, endpoint=False)
SETS.append(("circle", np.cos(t), np.sin(t)))
SETS.append(("circle and centre", np.append(np.cos(t), 0.0), np.append(np.sin(t), 0.0)))
x = rng.uniform(0, 1, 30)
y = rng.uniform(0, 1, 30)
SETS.append(("duplicates", np.concatenate([x, x[:7]]), np.concatenate([y, y[:7]])))
SETS.append(("integers", rng.integers(0, 8, 40).astype(float), rng.integers(0, 8, 40).astype(float)))
SETS.append(("two points", np.array([0.0, 1.0]), np.array([0.0, 1.0])))
SETS.append(("repeated", np.array([1.0, 1.0, 1.0, 2.0, 2.0]), np.array([3.0, 3.0, 3.0, 4.0, 4.0])))
SETS.append(("collinear", np.array([0.0, 1.0, 2.0, 3.0]), np.array([0.0, 1.0, 2.0, 3.0])))


def h(v):
    return float.hex(float(v))


cases = []
for name, x, y in SETS:
    entry = {"name": name, "x": [h(v) for v in x], "y": [h(v) for v in y]}
    try:
        tri = Triangulation(x, y)
        entry["triangles"] = tri.triangles.tolist()
        entry["neighbors"] = tri.neighbors.tolist()
    except (ValueError, RuntimeError) as e:
        entry["error"] = str(e)
    cases.append(entry)

(ROOT / "tests" / "oracle" / "triangulation.json").write_text(json.dumps({"cases": cases}, separators=(",", ":")) + "\n")
print(len(cases), "sets,", sum("error" in c for c in cases), "errors")
for c in cases:
    if "error" in c:
        print(c["name"], ":", c["error"])
