#!/usr/bin/env python3
"""Write tests/oracle/norms.json: matplotlib 3.11.2's norms (Normalize, LogNorm, SymLogNorm, AsinhNorm, PowerNorm,
TwoSlopeNorm, CenteredNorm, BoundaryNorm, NoNorm, the norms named by a scale) called on masked arrays of several
dtypes, then inverted: the outputs as float.hex with their dtype and mask, the limits the call autoscaled, or the
error raised.

  tools/oracle/.venv/bin/python -I tools/oracle/make_norms_oracle.py
"""
import json
import pathlib
import warnings

import matplotlib
import numpy as np
from matplotlib import colorizer, colors, scale

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")


def h(v):
    return float.hex(float(v))


def values(a):
    """A masked array as {"dtype", "data", "mask"} (data as float.hex; masked entries' data kept too)."""
    a = np.ma.asarray(a)
    return {"dtype": str(a.dtype), "data": [h(v) for v in np.ravel(a.data)],
            "mask": [int(i) for i in np.flatnonzero(np.ma.getmaskarray(a))]}


rng = np.random.default_rng(20261018)
F = [0.0, 1.0, -1.0, 0.5, 2.5, 1e-3, 1e3, -7.25, 3.0, 10.0, 100.0, 1e-300]
DATA = {
    "float64": [np.array(F), np.array(rng.uniform(-5, 5, 30)), np.array(rng.lognormal(0, 3, 30)),
                np.array([1.0, 1.0, 1.0]), np.array([2.0, np.nan, 5.0, 3.0]), np.array([np.inf, 1.0, 2.0]),
                np.array(rng.uniform(0, 1, 25))],
    "float32": [np.array(F, dtype=np.float32), np.array(rng.uniform(-5, 5, 30), dtype=np.float32),
                np.array(rng.uniform(0.1, 9, 20), dtype=np.float32)],
    "int64": [np.array([0, 3, -2, 7, 10, 255]), np.array(rng.integers(-50, 50, 20))],
    "int32": [np.array([1, 2, 3, 900000], dtype=np.int32)],
    "int16": [np.array([-300, 0, 12, 4000], dtype=np.int16)],
    "uint8": [np.array([0, 1, 7, 128, 254, 255], dtype=np.uint8), np.array(rng.integers(0, 256, 20), dtype=np.uint8)],
    "uint16": [np.array([0, 1000, 65535], dtype=np.uint16)],
    "bool": [np.array([True, False, True])],
}
MASKS = [None, [1], [0, 2]]


def inputs():
    for dtype, arrays in DATA.items():
        for k, a in enumerate(arrays):
            for m in MASKS:
                if m is not None and (k % 2 or max(m) >= a.size):
                    continue
                mask = np.zeros(a.shape, bool)
                if m is not None:
                    mask[m] = True
                yield np.ma.array(a, mask=mask if m is not None else np.ma.nomask)


NORMS = [
    ("Normalize", {}), ("Normalize", {"vmin": -1.0, "vmax": 4.0}), ("Normalize", {"vmin": -1.0, "vmax": 4.0, "clip": True}),
    ("Normalize", {"vmin": 2.0}), ("Normalize", {"vmax": 3}), ("Normalize", {"vmin": 2.5, "vmax": 2.5}),
    ("Normalize", {"vmin": 5.0, "vmax": 1.0}), ("Normalize", {"vmin": 0, "vmax": 255}),
    ("LogNorm", {}), ("LogNorm", {"vmin": 0.01, "vmax": 50.0}), ("LogNorm", {"vmin": 0.01, "vmax": 50.0, "clip": True}),
    ("LogNorm", {"vmin": -1.0, "vmax": 10.0}),
    ("SymLogNorm", {"linthresh": 0.5}), ("SymLogNorm", {"linthresh": 1.0, "linscale": 0.3, "vmin": -6.0, "vmax": 8.0}),
    ("SymLogNorm", {"linthresh": 0.1, "base": 2, "clip": True, "vmin": -3.0, "vmax": 3.0}),
    ("AsinhNorm", {}), ("AsinhNorm", {"linear_width": 0.2, "vmin": -4.0, "vmax": 4.0}),
    ("PowerNorm", {"gamma": 0.5}), ("PowerNorm", {"gamma": 2.0, "vmin": -1.0, "vmax": 6.0}),
    ("PowerNorm", {"gamma": 1.7, "vmin": 0.0, "vmax": 3.0, "clip": True}),
    ("TwoSlopeNorm", {"vcenter": 0.0}), ("TwoSlopeNorm", {"vcenter": 1.0, "vmin": -2.0, "vmax": 9.0}),
    ("TwoSlopeNorm", {"vcenter": 50.0}),
    ("CenteredNorm", {}), ("CenteredNorm", {"vcenter": 1.5}), ("CenteredNorm", {"vcenter": -1.0, "halfrange": 3.0}),
    ("CenteredNorm", {"halfrange": 2.0, "clip": True}),
    ("BoundaryNorm", {"boundaries": [-1.0, 0.0, 1.0, 3.0, 10.0], "ncolors": 256}),
    ("BoundaryNorm", {"boundaries": [-1.0, 0.0, 1.0, 3.0, 10.0], "ncolors": 4}),
    ("BoundaryNorm", {"boundaries": [-1.0, 0.0, 1.0, 3.0, 10.0], "ncolors": 4, "clip": True}),
    ("BoundaryNorm", {"boundaries": [0, 2, 4], "ncolors": 9, "extend": "both"}),
    ("BoundaryNorm", {"boundaries": [0.5, 5.0], "ncolors": 7, "extend": "max"}),
    ("BoundaryNorm", {"boundaries": [0.5, 5.0], "ncolors": 1}),
    ("BoundaryNorm", {"boundaries": [1.0, 2.0, 4.0], "ncolors": 3, "extend": "min"}),
    ("NoNorm", {}),
    ("scale:linear", {}), ("scale:log", {}), ("scale:symlog", {}), ("scale:asinh", {}), ("scale:logit", {}),
    ("scale:linear", {"vmin": 0.0, "vmax": 2.0, "clip": True}), ("scale:logit", {"vmin": 0.1, "vmax": 0.9}),
    # the constructors' errors
    ("TwoSlopeNorm", {"vcenter": 5.0, "vmax": 3.0}), ("TwoSlopeNorm", {"vcenter": 1.0, "vmin": 1.0}),
    ("BoundaryNorm", {"boundaries": [1.0], "ncolors": 3}), ("BoundaryNorm", {"boundaries": [0.0, 1.0], "ncolors": 3, "clip": True, "extend": "min"}),
    ("BoundaryNorm", {"boundaries": [0.0, 1.0, 2.0], "ncolors": 2, "extend": "both"}),
    ("SymLogNorm", {"linthresh": -1.0}), ("SymLogNorm", {"linthresh": 1.0, "base": 1.0}),
    ("SymLogNorm", {"linthresh": 1.0, "linscale": 0.0}), ("AsinhNorm", {"linear_width": 0.0}),
]


def make(name, kwargs):
    if name.startswith("scale:"):
        cls = colorizer._auto_norm_from_scale(scale._scale_mapping[name[6:]])
        return cls(**kwargs)
    return getattr(colors, name)(**kwargs)


cases = []
for name, kwargs in NORMS:
    for clip_arg in (None, True) if name in ("Normalize", "LogNorm", "PowerNorm") else (None,):
        for a in inputs():
            case = {"norm": name, "kwargs": {k: (h(v) if isinstance(v, float) else v) for k, v in kwargs.items()},
                    "input": values(a)}
            if clip_arg is not None:
                case["clip"] = clip_arg
            try:
                norm = make(name, kwargs)
            except Exception as e:
                case["error"] = f"{type(e).__name__}: {e}"
                cases.append(case)
                break  # the constructor's error: one case is enough
            try:
                out = norm(a.copy()) if clip_arg is None else norm(a.copy(), clip=clip_arg)
                case["output"] = values(out)
            except Exception as e:
                case["error"] = f"{type(e).__name__}: {e}"
            case["vmin"] = None if norm.vmin is None else h(norm.vmin)
            case["vmax"] = None if norm.vmax is None else h(norm.vmax)
            if "error" not in case and name != "NoNorm":
                probe = np.ma.array([0.0, 0.25, 0.5, 1.0, -0.5, 1.5])
                try:
                    case["inverse"] = values(norm.inverse(probe))
                except Exception as e:
                    case["inverse_error"] = f"{type(e).__name__}: {e}"
            cases.append(case)

out = ROOT / "tests" / "oracle" / "norms.json"
out.write_text(json.dumps({"_provenance": "matplotlib 3.11.2 norms, written by tools/oracle/make_norms_oracle.py",
                           "cases": cases}, indent=0) + "\n")
print(len(cases), "cases,", sum("error" in c for c in cases), "errors")
