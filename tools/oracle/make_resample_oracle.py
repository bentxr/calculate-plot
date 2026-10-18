#!/usr/bin/env python3
"""Write tests/oracle/resample.json: matplotlib 3.11.2's matplotlib._image.resample (src/_image_resample.h) over random
images: grey and RGBA, of uint8, uint16, float32 and float64; every interpolation filter; resample on and off; alpha,
norm and radius; affine transforms (scales, flips, shears, rotations) and one non-affine transform (its inverse
gives the lookup mesh). Inputs and outputs are the arrays' little-endian bytes in base64; outputs start as zeros, as
AxesImage._make_image's do.

  tools/oracle/.venv/bin/python -I tools/oracle/make_resample_oracle.py
"""
import base64
import json
import pathlib

import matplotlib
import numpy as np
from matplotlib import _image
from matplotlib.transforms import Affine2D, Transform

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


class Stretch(Transform):
    """(x, y) -> (x * (1 + y / 20), y): a non-affine transform whose inverse the resampler samples as a mesh."""
    input_dims = output_dims = 2

    def transform_non_affine(self, values):
        v = np.asarray(values, float)
        return np.column_stack([v[:, 0] * (1 + v[:, 1] / 20), v[:, 1]])

    def inverted(self):
        return Unstretch()


class Unstretch(Transform):
    input_dims = output_dims = 2

    def transform_non_affine(self, values):
        v = np.asarray(values, float)
        return np.column_stack([v[:, 0] / (1 + v[:, 1] / 20), v[:, 1]])

    def inverted(self):
        return Stretch()


def b64(a):
    return base64.b64encode(np.ascontiguousarray(a).astype(a.dtype.newbyteorder("<")).tobytes()).decode("ascii")


rng = np.random.default_rng(20261018)
INTERPOLATIONS = ["NEAREST", "BILINEAR", "BICUBIC", "SPLINE16", "SPLINE36", "HANNING", "HAMMING", "HERMITE", "KAISER",
                  "QUADRIC", "CATROM", "GAUSSIAN", "BESSEL", "MITCHELL", "SINC", "LANCZOS", "BLACKMAN"]
DTYPES = ["uint8", "uint16", "float32", "float64"]
cases = []
for i in range(480):
    dtype = DTYPES[i % 4]
    rgba = (i // 4) % 2 == 1
    h, w = int(rng.integers(1, 9)), int(rng.integers(1, 9))
    oh, ow = int(rng.integers(1, 15)), int(rng.integers(1, 15))
    shape = (h, w, 4) if rgba else (h, w)
    if dtype == "uint8":
        data = rng.integers(0, 256, shape).astype(np.uint8)
    elif dtype == "uint16":
        data = rng.integers(0, 65536, shape).astype(np.uint16)
    else:
        data = rng.uniform(-0.2, 1.2, shape).astype(dtype)
        if not rgba:
            data[rng.uniform(size=shape) < 0.05] = np.nan
    out = np.zeros((oh, ow, 4) if rgba else (oh, ow), dtype)
    interpolation = INTERPOLATIONS[int(rng.integers(len(INTERPOLATIONS)))] if i % 5 else "NEAREST"
    kind = i % 7
    if kind == 0:
        t = Affine2D().scale(ow / w, oh / h)
    elif kind == 1:
        t = Affine2D().scale(-ow / w, oh / h).translate(ow, 0)
    elif kind == 2:
        t = Affine2D().scale(float(rng.uniform(0.3, 3)), float(rng.uniform(0.3, 3))).translate(*rng.uniform(-2, 2, 2))
    elif kind == 3:
        t = Affine2D().rotate_deg(float(rng.uniform(-60, 60))).scale(1.5).translate(3, 1)
    elif kind == 4:
        t = Affine2D().skew_deg(15, 5).scale(ow / w, oh / h)
    elif kind == 5:
        t = Affine2D().scale(1, -1).translate(0.25, oh)  # sx and sy of magnitude 1: the resampler takes NEAREST
    else:
        t = Affine2D().scale(ow / w, oh / h) + Stretch()
    resample = bool(i % 3)
    alpha = [1.0, 0.5, 0.3, 1.0][i % 4 if i % 11 else 2]
    norm = bool(i % 13 == 0)
    radius = [1.0, 2.0, 3.5][i % 3]
    _image.resample(data, out, t, getattr(_image, interpolation), resample=resample, alpha=alpha, norm=norm,
                    radius=radius)
    case = {"dtype": dtype, "shape": list(shape), "out_shape": list(out.shape), "input": b64(data),
            "interpolation": interpolation, "resample": resample, "alpha": alpha, "norm": norm, "radius": radius,
            "output": b64(out)}
    if t.is_affine:
        m = t.get_matrix()
        case["affine"] = [float.hex(float(v)) for v in (m[0, 0], m[1, 0], m[0, 1], m[1, 1], m[0, 2], m[1, 2])]
    else:
        m = t.get_affine().get_matrix()  # the affine part; the mesh is (Unstretch then its inverse) of it
        case["stretch_after"] = [float.hex(float(v)) for v in (m[0, 0], m[1, 0], m[0, 1], m[1, 1], m[0, 2], m[1, 2])]
        # the mesh as the wrapper makes it: the inverse transform of the output's pixel centres
        centres = np.array([[x + 0.5, y + 0.5] for y in range(oh) for x in range(ow)], float)
        case["mesh"] = b64(np.ascontiguousarray(t.inverted().transform(centres), float))
    cases.append(case)

(ROOT / "tests" / "oracle" / "resample.json").write_text(json.dumps({
    "_provenance": "matplotlib 3.11.2 _image.resample, written by tools/oracle/make_resample_oracle.py",
    "cases": cases}, indent=0) + "\n")
print(len(cases), "cases")
