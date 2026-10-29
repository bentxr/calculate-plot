#!/usr/bin/env python3
"""Write tests/oracle/polar.json: the transforms of matplotlib 3.11.2's polar projection on their own.
PolarTransform of points (negative radii are NaN) and of paths (with _interpolation_steps > 1 a segment at a
constant radius becomes arcs of Path.arc, one at a constant angle stays a line, others are interpolated; NaN
vertices are removed as Path.iter_segments removes them, long line paths simplified), with and without a radial
origin, a reversed radius and a log radial scale; InvertedPolarTransform; PolarAffine's matrix; _WedgeBbox's
points for views of whole circles, wedges and annuli at several offsets and directions.

  tools/oracle/.venv/bin/python -I tools/oracle/make_polar_oracle.py
"""
import json
import pathlib

import numpy as np
import matplotlib

matplotlib.use("agg")
from matplotlib import transforms as mtransforms  # noqa: E402
from matplotlib.path import Path  # noqa: E402
from matplotlib.projections.polar import (InvertedPolarTransform, PolarAffine,  # noqa: E402
                                          PolarTransform, _WedgeBbox)
from matplotlib.scale import LogTransform  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
rng = np.random.default_rng(1219)


def h(x):
    return float.hex(float(x))


def hexes(a):
    return [[h(v) for v in p] for p in np.asarray(a, dtype=float).reshape(-1, 2)]


class Origin:
    """What PolarTransform asks of its axis: get_rorigin and get_rsign of the origin's lockable box."""

    def __init__(self, view, rorigin):
        self.lim = mtransforms.LockableBbox(mtransforms.Bbox(view))
        self.lim.locked_y0 = rorigin

    def get_rorigin(self):
        return self.lim.y0

    def get_rsign(self):
        return np.sign(self.lim.y1 - self.lim.y0)


def scale_of(name):
    if name == "linear":
        return mtransforms.TransformWrapper(mtransforms.IdentityTransform())
    return mtransforms.TransformWrapper(
        mtransforms.blended_transform_factory(mtransforms.IdentityTransform(), LogTransform(10)))


# (name, view limits [[t0, r0], [t1, r1]], locked rorigin or None, scale); None for no axis at all
SETTINGS = [
    ("no axis", None, None, "linear"),
    ("plain", [[0, 0], [2 * np.pi, 3]], None, "linear"),
    ("rorigin", [[0, 1], [2 * np.pi, 4]], -1.5, "linear"),
    ("rmin", [[0, 1], [2 * np.pi, 4]], None, "linear"),
    ("reversed", [[0, 5], [2 * np.pi, 1]], None, "linear"),
    ("log", [[0, 0.1], [2 * np.pi, 100]], None, "log"),
    ("log rorigin", [[0, 1], [2 * np.pi, 100]], 0.5, "log"),
]


def transform_of(setting):
    _, view, rorigin, scale = setting
    if view is None:
        return PolarTransform(), InvertedPolarTransform()
    origin = Origin(view, rorigin)
    return (PolarTransform(origin, scale_transform=scale_of(scale)), InvertedPolarTransform(origin))


POINTS = np.concatenate([
    np.column_stack([rng.uniform(-7, 7, 40), rng.uniform(-1, 6, 40)]),
    [[0, 0], [np.pi, 2], [-np.pi / 2, 0.5], [np.nan, 1], [1, np.nan], [2 * np.pi, 3], [0.3, -0.25], [1e-300, 1]],
])


def line(ts, rs, codes=None, steps=1):
    return Path(np.column_stack([ts, rs]), codes, _interpolation_steps=steps)


M, L, C3, C4, CL = Path.MOVETO, Path.LINETO, Path.CURVE3, Path.CURVE4, Path.CLOSEPOLY
PATHS = [
    ("pointwise", line([0, 1, 2, 3], [1, 2, 1.5, 0.5])),
    ("gridline r", line([0, 2 * np.pi], [2, 2], steps=180)),
    ("gridline theta", line([0.7, 0.7], [0, 1], steps=180)),
    ("interpolated", line([0, 1.3, 2.9], [0.5, 2.5, 1.0], steps=4)),
    ("arc back", line([3, 0.5], [1.5, 1.5], steps=2)),
    ("arc beyond", line([0, 9], [1, 1], steps=2)),
    ("arc beyond back", line([2, -13], [2.5, 2.5], steps=2)),
    ("arc exactly", line([0, 4 * np.pi], [1, 1], steps=2)),
    ("rectangle", line([0.2, 1.1, 1.1, 0.2, 0.2], [1, 1, 2, 2, 1], [M, L, L, L, CL], steps=100)),
    ("nans", line([0, 0.5, np.nan, 1.5, 2.0, 2.5], [1, 1, 1, 2, np.nan, 2], steps=3)),
    ("curves", line([0, 0.5, 1, 1.5, 2, 2.5, 3], [1, 2, 1, 2, 1, 2, 2], [M, C3, C3, C4, C4, C4, L], steps=2)),
    ("subpaths", line([0, 1, 2, 2, 3], [1, 1, 1, 2, 2], [M, L, M, L, L], steps=5)),
    ("long", line(np.linspace(0, 6, 150), 2 + np.sin(np.linspace(0, 30, 150)), steps=2)),
    ("long same radius", line(np.linspace(0, 6, 140), np.ones(140), steps=2)),
    ("negative", line([0, 1, 1], [-0.5, -0.5, 2], steps=2)),
    ("empty", line(np.zeros(0), np.zeros(0), steps=2)),
]

transforms = []
for setting in SETTINGS:
    forward, inverse = transform_of(setting)
    entry = {"name": setting[0], "view": setting[1] and hexes(setting[1]),
             "rorigin": None if setting[2] is None else h(setting[2]), "scale": setting[3],
             "points": hexes(forward.transform_non_affine(POINTS)), "paths": []}
    for name, path in PATHS:
        out = forward.transform_path_non_affine(path)
        entry["paths"].append({"name": name, "vertices": hexes(out.vertices),
                               "codes": None if out.codes is None else [int(c) for c in out.codes]})
    xy = np.column_stack([rng.uniform(-3, 3, 30), rng.uniform(-3, 3, 30)])
    xy = np.concatenate([xy, [[0, 0], [-1, 0], [-1, -0.0], [0, -2], [np.nan, 1]]])
    entry["inverse"] = {"xy": hexes(xy), "out": hexes(inverse.transform_non_affine(xy))}
    transforms.append(entry)

AFFINES = [
    ([[0, 0], [2 * np.pi, 3]], "linear"),
    ([[0, 5], [2 * np.pi, 1]], "linear"),
    ([[0, -2], [1, 0.3]], "linear"),
    ([[0, 0.1], [2 * np.pi, 100]], "log"),
    ([[0, 3e-3], [2 * np.pi, 7.5]], "log"),
]
affines = []
for view, scale in AFFINES:
    m = PolarAffine(scale_of(scale), mtransforms.Bbox(view)).get_matrix()
    affines.append({"view": hexes(view), "scale": scale, "matrix": [h(v) for v in m[:2].ravel()]})

# (theta limits in degrees, r limits, rorigin, theta offset in radians, direction)
WEDGES = [
    ((0, 360), (0, 1), None, 0, 1),
    ((0, 360), (1, 4), -1.5, 0, 1),
    ((0, 180), (0, 2), None, 0, 1),
    ((0, 90), (0, 2), None, np.pi / 2, 1),
    ((-45, 45), (0, 3), None, 0, -1),
    ((30, 200), (1, 3), None, np.pi * 0.25, -1),
    ((10, 80), (2, 3), 0, 0, 1),
    ((200, 30), (0, 1), None, 0, 1),
    ((0, 120), (5, 1), None, 1.0, 1),
    ((45, 315), (0.5, 1.5), 0.25, np.pi * 1.5, -1),
]
wedges = []
for (t0, t1), (r0, r1), rorigin, offset, direction in WEDGES:
    view = mtransforms.Bbox([[np.deg2rad(t0), r0], [np.deg2rad(t1), r1]])
    shift = mtransforms.Affine2D().scale(direction, 1.0) + mtransforms.Affine2D().translate(offset, 0.0)
    real = mtransforms.TransformedBbox(view, shift)
    origin = mtransforms.LockableBbox(view)
    origin.locked_y0 = rorigin
    box = _WedgeBbox((0.5, 0.5), real, origin)
    wedges.append({"view": hexes(view.get_points()), "rorigin": None if rorigin is None else h(rorigin),
                   "offset": h(offset), "direction": direction, "points": hexes(box.get_points())})

out = {"points": hexes(POINTS), "transforms": transforms, "affines": affines, "wedges": wedges}
(ROOT / "tests" / "oracle" / "polar.json").write_text(json.dumps(out, separators=(",", ":")) + "\n")
print(len(transforms), "transforms,", sum(len(t["paths"]) for t in transforms), "paths,", len(affines), "affines,",
      len(wedges), "wedges")
