#!/usr/bin/env python3
"""Write tests/oracle/bezier.json: matplotlib 3.11.2's bezier.py routines as FancyArrowPatch uses them
(BezierSegment of a scalar t, split_de_casteljau, find_bezier_t_intersecting_with_closedpath and
split_path_inout with inside_circle, get_cos_sin, check_if_parallel, get_intersection, get_normal_points,
get_parallels, make_wedged_bezier2), as float.hex; or the error matplotlib raises.

  tools/oracle/.venv/bin/python -I tools/oracle/make_bezier_oracle.py
"""
import json
import pathlib
import warnings

import matplotlib
import numpy as np
from matplotlib import bezier
from matplotlib.path import Path

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")


def h(v):
    return float.hex(float(v))


def hp(points):
    return [[h(x), h(y)] for x, y in np.asarray(points, float).reshape(-1, 2)]


rng = np.random.default_rng(20261016)


def pts(n, scale=300.0):
    return [[float(x), float(y)] for x, y in np.round(rng.uniform(-scale, scale, (n, 2)), 3)]


CURVES = [[[0, 0], [1, 1]], [[0, 0], [50, 100], [100, 0]], [[0, 0], [30, 90], [70, -40], [100, 10]],
          [[10.5, 20.25], [10.5, 20.25], [300.125, -7.5]], [[1e-3, 2e-3], [5e-4, 1e5], [-3.7, 4.1]]]
CURVES += [pts(n) for n in (2, 3, 4) for _ in range(6)]
TS = [0.0, 1.0, 0.5, 0.25, 0.75, 0.1, 0.3333333333333333, 0.9921875, 1e-9, 0.123456789]

evals, splits = [], []
for c in CURVES:
    bz = bezier.BezierSegment(c)
    evals.append({"points": c, "t": [h(t) for t in TS], "values": [hp(bz(t)) for t in TS]})
    for t in TS[2:6]:
        left, right = bezier.split_de_casteljau(c, t)
        splits.append({"points": c, "t": h(t), "left": hp(left), "right": hp(right)})

intersections = []
for c in CURVES:
    for frac in (0.1, 0.3, 0.6, 0.95):
        for at_end in (False, True):
            cx, cy = c[-1] if at_end else c[0]
            r = frac * float(np.hypot(c[-1][0] - c[0][0], c[-1][1] - c[0][1])) + 1e-3
            case = {"points": c, "circle": [h(cx), h(cy), h(r)]}
            bz = bezier.BezierSegment(c)
            try:
                t0, t1 = bezier.find_bezier_t_intersecting_with_closedpath(
                    lambda t: tuple(bz(t)), bezier.inside_circle(cx, cy, r))
                case["t"] = [h(t0), h(t1)]
                left, right = bezier.split_bezier_intersecting_with_closedpath(c, bezier.inside_circle(cx, cy, r))
                case["left"], case["right"] = hp(left), hp(right)
            except ValueError as e:
                case["error"] = str(e)
            intersections.append(case)

PATHS = [([[0, 0], [100, 0]], None), ([[0, 0], [50, 50], [100, 0]], [1, 3, 3]),
         ([[0, 0], [20, 30], [60, 30], [100, 0]], [1, 4, 4, 4]),
         ([[0, 0], [30, 0], [30, 40], [70, 40], [70, 80], [100, 80]], None),
         ([[0, 0], [30, 0], [40, 10], [50, 0], [80, 0]], [1, 2, 3, 3, 2]),
         ([[5, 5], [200, 5], [200, 300], [400, 300]], [1, 2, 2, 2])]
inouts = []
for verts, codes in PATHS:
    p = Path(verts, codes)
    for (cx, cy) in (verts[0], verts[-1]):
        for r in (1.0, 2.0, 10.0, 45.0, 500.0):
            case = {"vertices": verts, "codes": codes, "circle": [h(cx), h(cy), h(r)]}
            try:
                a, b = bezier.split_path_inout(p, bezier.inside_circle(cx, cy, r))
                case["in"] = {"vertices": hp(a.vertices), "codes": None if a.codes is None else [int(x) for x in a.codes]}
                case["out"] = {"vertices": hp(b.vertices), "codes": None if b.codes is None else [int(x) for x in b.codes]}
            except ValueError as e:
                case["error"] = str(e)
            inouts.append(case)

geometry = []
QUADS = [[[0, 0], [50, 100], [100, 0]], [[0, 0], [50, 0], [100, 0]], [[0, 0], [50, 0], [0, 0]],
         [[10, 10], [10, 10], [40, 50]], [[0, 0], [1e-9, 0], [100, 1e-9]]] + [pts(3) for _ in range(12)]
for q in QUADS:
    for w in (0.0, 1.5, 7.25):
        case = {"bezier2": q, "width": h(w)}
        left, right = bezier.get_parallels(q, w)
        case["parallels"] = [hp(left), hp(right)]
        for w1, wm, w2 in ((1., 0.5, 0.), (1., .6, .3), (1, 0.5, 0)):
            left, right = bezier.make_wedged_bezier2(q, w, w1=w1, wm=wm, w2=w2)
            case.setdefault("wedged", []).append({"w": [h(w1), h(wm), h(w2)], "left": hp(left), "right": hp(right)})
        geometry.append(case)

lines = []
for _ in range(40):
    x0, y0, x1, y1 = (float(v) for v in np.round(rng.uniform(-100, 100, 4), 2))
    c = {"p": [h(x0), h(y0), h(x1), h(y1)], "cos_sin": [h(v) for v in bezier.get_cos_sin(x0, y0, x1, y1)]}
    c["parallel"] = int(bezier.check_if_parallel(x0, y0, x1, y1))
    a1, a2 = (float(v) for v in rng.uniform(0, 360, 2))
    t1, t2 = np.deg2rad(a1), np.deg2rad(a2)
    try:
        c["intersection"] = [h(v) for v in bezier.get_intersection(x0, y0, np.cos(t1), np.sin(t1), x1, y1, np.cos(t2), np.sin(t2))]
    except ValueError as e:
        c["intersection_error"] = str(e)
    c["angles"] = [h(np.cos(t1)), h(np.sin(t1)), h(np.cos(t2)), h(np.sin(t2))]
    c["normal"] = [h(v) for v in bezier.get_normal_points(x0, y0, np.cos(t1), np.sin(t1), 3.5)]
    lines.append(c)
for dx1, dy1, dx2, dy2 in ((1, 1, 2, 2), (1, 1, -2, -2), (0, 0, 0, 0), (1, 0, 1, 1e-7), (-1, 0, 1, 0), (0, -0.0, 0, 0)):
    lines.append({"p": [h(dx1), h(dy1), h(dx2), h(dy2)], "cos_sin": [h(v) for v in bezier.get_cos_sin(dx1, dy1, dx2, dy2)],
                  "parallel": int(bezier.check_if_parallel(dx1, dy1, dx2, dy2))})

out = {"_note": "matplotlib 3.11.2 (PSF-style licence), tools/oracle/make_bezier_oracle.py", "evals": evals,
       "splits": splits, "intersections": intersections, "inouts": inouts, "geometry": geometry, "lines": lines}
(ROOT / "tests" / "oracle" / "bezier.json").write_text(json.dumps(out) + "\n")
print(len(evals), "evals,", len(splits), "splits,", len(intersections), "intersections (",
      sum("error" in c for c in intersections), "errors ),", len(inouts), "inouts (", sum("error" in c for c in inouts),
      "errors ),", len(geometry), "geometry,", len(lines), "lines")
