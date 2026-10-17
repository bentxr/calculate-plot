#!/usr/bin/env python3
"""Write tests/oracle/transform_algebra.json: how matplotlib 3.11.2 compares and subtracts the
transforms of an Axes. Equality of transforms is partly by value (affine transforms with equal
matrices are equal), so the answers change with the view limits: for an Axes in several states and
every ordered pair (A, B) of its transforms (transData, transAxes, transScale, transLimits, the
x-axis and y-axis blends, a Rectangle's transform over four of them, and four more) this records A == B,
A.contains_branch(B), A.contains_branch_separately(B), A.depth, and three points through A - B.

  tools/oracle/.venv/bin/python -I tools/oracle/make_transform_algebra_oracle.py
"""
import json
import pathlib

import matplotlib

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import Rectangle  # noqa: E402
from matplotlib.transforms import Affine2D, IdentityTransform  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"

POINTS = [[0.25, 0.75], [1.5, 2.0], [3.0, 4.0]]
RECTANGLE = [0.1, 0.2, 0.5, 0.3]
OVER = ["transData", "transAxes", "xaxis", "yaxis"]


def h(x):
    return float.hex(float(x))


def transforms_of(ax):
    t = {"transData": ax.transData, "transAxes": ax.transAxes, "transScale": ax.transScale,
         "transLimits": ax.transLimits, "xaxis": ax.get_xaxis_transform(), "yaxis": ax.get_yaxis_transform()}
    for name in OVER:
        r = Rectangle(RECTANGLE[:2], RECTANGLE[2], RECTANGLE[3])
        r.set_transform(t[name])
        t["rectangle+" + name] = r.get_transform()
    # equal by value though of different kinds: a composite of two Affine2D, one Affine2D with the
    # same matrix; the identity, and a unit rectangle's own transform (a generic composite)
    t["scale+shift"] = Affine2D().scale(2, 3) + Affine2D().translate(1, 1)
    t["scaled, shifted"] = Affine2D().scale(2, 3).translate(1, 1)
    t["identity"] = IdentityTransform()
    # one entry of the matrix away from "scaled, shifted"
    t["shifted in y"] = Affine2D.from_values(2, 0, 0, 3, 1, 2)
    t["sheared in x"] = Affine2D.from_values(2, 0, 0.5, 3, 1, 1)
    t["sheared in y"] = Affine2D.from_values(2, 0.5, 0, 3, 1, 1)
    unit = Rectangle((0, 0), 1, 1)
    unit.set_transform(IdentityTransform())
    t["unit rectangle"] = unit.get_transform()
    return t


STATES = [
    ("as made", lambda ax: None),
    ("xlim", lambda ax: ax.set_xlim(2, 5)),
    ("ylim", lambda ax: ax.set_ylim(-1, 3)),
    ("both", lambda ax: (ax.set_xlim(2, 5), ax.set_ylim(-1, 3))),
    ("data", lambda ax: (ax.plot([1, 2, 3], [10, 20, 15]), ax.viewLim)),
    ("log", lambda ax: (ax.plot([1, 2, 3], [10, 20, 15]), ax.set_yscale("log"), ax.viewLim)),
    ("unit again", lambda ax: (ax.set_xlim(2, 5), ax.set_xlim(0, 1))),
]

cases = []
for name, prepare in STATES:
    fig, ax = plt.subplots()
    prepare(ax)
    t = transforms_of(ax)
    pairs = []
    for a in t:
        for b in t:
            A, B = t[a], t[b]
            entry = {"a": a, "b": b, "equal": bool(A == B), "contains": bool(A.contains_branch(B)),
                     "separately": [bool(v) for v in A.contains_branch_separately(B)], "depth": int(A.depth)}
            if A.is_affine and B.is_affine:
                entry["minus"] = [[h(v) for v in p] for p in (A - B).transform(POINTS)]
            pairs.append(entry)
    cases.append({"state": name, "pairs": pairs})
    plt.close(fig)

out = {"points": POINTS, "rectangle": RECTANGLE, "over": OVER, "states": cases}
(ROOT / "tests" / "oracle" / "transform_algebra.json").write_text(json.dumps(out, separators=(",", ":")) + "\n")
print(len(cases), "states,", sum(len(c["pairs"]) for c in cases), "pairs,",
      sum(p["equal"] and p["a"] != p["b"] for c in cases for p in c["pairs"]), "equal pairs of different transforms")
