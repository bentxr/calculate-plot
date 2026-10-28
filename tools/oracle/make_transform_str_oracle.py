#!/usr/bin/env python3
"""Write tests/oracle/transform_str.json: str() of transforms, as RendererSVG hashes it for clip path ids.

RendererSVG names a clip path by a hash of str(clippath_trans): the transform tree printed by
_make_str_method, boxes by Bbox.__format__, Affine2D by its scale shorthand or by NumPy's str() of
its matrix. This records NumPy's str() of float64 arrays of up to three columns (random values,
signed zeros, values that ask for scientific notation, integers) and str() of a few transform trees.

  tools/oracle/.venv/bin/python -I tools/oracle/make_transform_str_oracle.py
"""
import json
import pathlib
import random

import matplotlib
import numpy as np
from matplotlib.transforms import (Affine2D, Bbox, BboxTransform, BboxTransformFrom, BboxTransformTo,
                                   IdentityTransform, TransformedBbox, blended_transform_factory)

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2", matplotlib.__version__

rnd = random.Random(12)


def rows(shape, draw):
    return [[draw() for _ in range(shape[1])] for _ in range(shape[0])]


arrays = [
    [[1.0, 0.0, 0.0], [-0.0, -1.0, 345.6], [0.0, 0.0, 1.0]],
    [[72.0, 0.0, 0.0], [0.0, 72.0, 0.0], [0.0, 0.0, 1.0]],
    [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 1.0]],
    [[-0.0, -0.0], [-0.0, -0.0]],
    [[1e-5, 1.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
    [[1e8, 1.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
    [[99999999.0, 1.0, 0.5], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
    [[1234.5, 0.5, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
    [[0.1 + 0.2, 1 / 3, 2 / 3], [0.7837500000000002, 0.10999999999999999, 0.88], [0.0, 0.0, 1.0]],
    [[123456.123456789, -0.000123, 1.0]],
    [[1e300, -1e-300, 0.0]],
    [[0.5, 0.25, 0.125]],
    [[0.0001, 1.0, 0.0]],
    [[0.00009999, 1.0, 0.0]],
    [[0.123456785, 0.123456775, 0.123456795]],
    [[5e-324, 1.0, 2.0]],
]
for _ in range(150):
    shape = (rnd.randint(1, 3), rnd.randint(1, 3))
    kind = rnd.randrange(5)
    if kind == 0:
        draw = lambda: rnd.uniform(-1000, 1000)
    elif kind == 1:
        draw = lambda: float(rnd.randint(-50, 50))
    elif kind == 2:
        draw = lambda: rnd.choice([0.0, -0.0, 1.0, -1.0, rnd.uniform(-1, 1)])
    elif kind == 3:
        draw = lambda: rnd.uniform(-1, 1) * 10.0 ** rnd.randint(-12, 12)
    else:
        draw = lambda: round(rnd.uniform(0, 500), rnd.randint(0, 9))
    arrays.append(rows(shape, draw))
# each one fits NumPy's line width of 75 columns
numpy_str = [{"rows": [[float.hex(v) for v in r] for r in a], "str": str(np.array(a, dtype=np.float64))} for a in arrays]
assert all(max(len(line) for line in c["str"].split("\n")) <= 75 for c in numpy_str)

fig_bbox = TransformedBbox(Bbox.from_bounds(0, 0, 6.4, 4.8), Affine2D().scale(72.0))
ax_bbox = TransformedBbox(Bbox([[0.7837500000000002, 0.10999999999999999], [0.8126250000000002, 0.88]]),
                          BboxTransformTo(fig_bbox))
flip = Affine2D().scale(1, -1).translate(0, 345.6)
trees = {
    # (name in the C++ test: the same tree is built there)
    "identity": IdentityTransform(),
    "scale": Affine2D().scale(72.0),
    "scale_xy": Affine2D().scale(2.0, 3.5),
    "flip": flip,
    "rotate": Affine2D().rotate_deg(30),
    "axes_flip": BboxTransformTo(ax_bbox) + flip,
    "affines": Affine2D().scale(2.0) + Affine2D().translate(1, 2),
    "identity_dropped": IdentityTransform() + flip,
    "from_to": BboxTransform(Bbox.from_bounds(0, 0, 1, 1), Bbox.from_bounds(1, 2, 3, 4)),
    "from": BboxTransformFrom(Bbox.from_bounds(1, 2, 3, 4)),
    "blended": blended_transform_factory(Affine2D().scale(2.0), BboxTransformTo(ax_bbox)),
}
out = {"numpy_str": numpy_str, "transforms": {k: str(t) for k, t in trees.items()}}
(ROOT / "tests" / "oracle" / "transform_str.json").write_text(json.dumps(out, indent=0) + "\n")
print(len(numpy_str), "arrays,", len(trees), "transforms")
