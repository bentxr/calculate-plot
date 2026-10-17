#!/usr/bin/env python3
"""Write tests/oracle/violin.json: matplotlib 3.11.2's cbook.violin_stats (mlab.GaussianKDE with the bandwidth
rules 'scott', 'silverman' or a number; np.mean, np.median, np.percentile) of float64 and int64 data sets, as
float.hex. The data are given by a generator both sides compute exactly.

  tools/oracle/.venv/bin/python -I tools/oracle/make_violin_oracle.py
"""
import json
import pathlib

import numpy as np
import matplotlib
from matplotlib import cbook

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def generated(spec):
    n, p, q, scale, offset = spec["n"], spec["p"], spec["q"], spec["scale"], spec["offset"]
    if spec.get("int"):
        return [(i * p) % q + offset for i in range(n)]
    return [(i * p) % q / q * scale + offset for i in range(n)]


def data(spec):
    if isinstance(spec, dict):
        return generated(spec)
    return [float(v) if isinstance(v, str) else v for v in spec]


def gen(n, p=7919, q=10007, scale=2.0, offset=-1.0):
    return {"n": n, "p": p, "q": q, "scale": scale, "offset": offset}


def igen(n, p=7919, q=101, offset=-50):
    return {"n": n, "p": p, "q": q, "scale": 1, "offset": offset, "int": True}


def hexes(values):
    return [float.hex(float(v)) for v in np.atleast_1d(values)]


CASES = []
for spec, integer in [(gen(2), False), (gen(3), False), (gen(10), False), (gen(99), False), (gen(100), False), (gen(101), False),
                      (gen(250), False), (gen(40, 31, 97, 0.1, 0.0), False), (igen(30), True), (igen(7), True),
                      ([1.0, 2.0, 2.0, 3.0, 7.5], False), ([5, 5, 5], True), ([4.25], False), ([], False),
                      ([1.0, "nan", 2.0, "inf", 3.5, -1.0], False), ([0.0, -0.0, 1e-300, 2.0], False)]:
    for bw in [None, "scott", "silverman", 0.3, 2]:
        for points in [100, 7, 500]:
            for quantiles in [None, [0.1, 0.5, 0.9]]:
                CASES.append((spec, integer, bw, points, quantiles))

cases = []
for spec, integer, bw, points, quantiles in CASES:
    x = np.asarray(data(spec), np.int64 if integer else float)
    case = {"data": spec, "int": integer, "bw": bw, "points": points, "q": quantiles}
    try:
        s, = cbook.violin_stats([x], ("GaussianKDE", bw), points=points, quantiles=None if quantiles is None else [quantiles])
        case.update({k: hexes(s[k]) for k in ("vals", "coords", "mean", "median", "min", "max", "quantiles")})
    except Exception as e:  # noqa: BLE001
        case["error"] = f"{type(e).__name__}: {e}"
    cases.append(case)
(ROOT / "tests" / "oracle" / "violin.json").write_text(
    json.dumps({"_note": "matplotlib 3.11.2 cbook.violin_stats, tools/oracle/make_violin_oracle.py", "cases": cases}) + "\n")
print(len(cases), "cases,", sum("error" in c for c in cases), "errors")
