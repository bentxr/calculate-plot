#!/usr/bin/env python3
"""Write tests/oracle/histogram.json: NumPy 2.5.3's np.sum of float64 arrays, np.histogram_bin_edges and
np.histogram (bins, range, density, weights), as float.hex; or the error NumPy raises. The data are
given by a generator both sides compute exactly ((i * p) % q / q * scale + offset), so that long
arrays need not be stored.

  tools/oracle/.venv/bin/python -I tools/oracle/make_histogram_oracle.py
"""
import json
import pathlib
import warnings

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert np.__version__ == "2.5.3"
warnings.simplefilter("ignore")


def generated(spec):
    n, p, q, scale, offset = spec["n"], spec["p"], spec["q"], spec["scale"], spec["offset"]
    return [(i * p) % q / q * scale + offset for i in range(n)]


def data(spec):
    """A spec is a list of numbers (with "nan", "inf", "-inf" as strings) or a generator."""
    if isinstance(spec, dict):
        return generated(spec)
    return [float(v) if isinstance(v, str) else v for v in spec]


def hexes(values):
    return [float.hex(float(v)) for v in values]


def gen(n, p=7919, q=10007, scale=2.0, offset=-1.0):
    return {"n": n, "p": p, "q": q, "scale": scale, "offset": offset}


sums = []
for n in [0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 100, 127, 128, 129, 130, 255, 256, 257, 1000, 8191, 8192, 8193, 9000,
          16384, 20000, 65536, 70001]:
    for spec in (gen(n), gen(n, 104729, 1000003, 1e6, -3.3), gen(n, 31, 97, 0.1, 0.0)):
        a = np.asarray(data(spec), float)
        sums.append({"data": spec, "sum": float.hex(float(np.sum(a)))})

NAN, INF = "nan", "inf"
HIST = [
    # (data, bins, range, density, weights)
    ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], 10, None, False, None),
    ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], 3, None, False, None),
    ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], 4, None, True, None),
    ([0.1, 0.2, 0.3], 1, None, False, None),
    ([5, 5, 5], 10, None, False, None),
    ([5.5], 3, None, True, None),
    ([], 10, None, False, None),
    ([], 4, [2, 3], False, None),
    ([0.0, 1.0, NAN, 0.5], 4, [0, 1], False, None),
    ([0.0, 1.0, NAN, 0.5], 4, None, False, None),
    ([0.0, 1.0, INF, 0.5], 4, None, False, None),
    ([1, 2, 3], 4, [3, 1], False, None),
    ([1, 2, 3], 4, [0.0, INF], False, None),
    ([1, 2, 3], 0, None, False, None),
    ([1, 2, 3], -2, None, False, None),
    ([1, 2, 3], [0, 1.5, 2, 4], None, False, None),
    ([1, 2, 3], [0, 3, 2], None, False, None),
    ([1, 2, 3, 4, 4], [1, 2, 3, 4], None, False, None),
    ([1, 2, 3, 4, 4], [1, 2, 3, 4], None, True, None),
    ([1, 2, 3, 4, 4, NAN], [1, 2, 3, 4], None, False, [1, 2, 3, 4, 5, 6]),
    ([1, 2, 3, 4, 4], [1, 2, 2, 4], None, False, None),
    ([1, 2, 3, 4, 4], 3, [1.5, 3.5], False, [0.1, 0.2, 0.3, 0.4, 0.5]),
    ([1, 2, 3, 4, 4], 3, [1.5, 3.5], True, [0.1, 0.2, 0.3, 0.4, 0.5]),
    ([1, 2, 3, 4, 4], 3, None, False, [0.1, 0.2]),
    ([0.1 * i for i in range(30)], 7, None, False, None),
    ([0.1 * i for i in range(30)], 7, [0.3, 2.2], True, None),
    ([1e-300, 2e-300, 3e-300], 5, None, False, None),
    ([1e300, -1e300], 3, None, False, None),
    ([0, 1e-320], 3, None, False, None),
    (gen(1000), 10, None, False, None),
    (gen(1000), 37, None, True, None),
    (gen(1000), 13, [-0.5, 0.75], False, gen(1000, 31, 97, 0.1, 0.0)),
    (gen(1000), 13, [-0.5, 0.75], True, gen(1000, 31, 97, 0.1, 0.0)),
    (gen(1000), [-1, -0.3, 0, 0.01, 0.5, 1], None, False, gen(1000, 31, 97, 0.1, 0.0)),
    (gen(1000), [-1, -0.3, 0, 0.01, 0.5, 1], None, True, gen(1000, 31, 97, 0.1, 0.0)),
    (gen(70001, 104729, 1000003, 1e6, -3.3), 50, None, False, None),
    (gen(70001, 104729, 1000003, 1e6, -3.3), 50, None, True, gen(70001)),
    (gen(70001, 104729, 1000003, 1e6, -3.3), [-3.3, 1e5, 2.5e5, 1e6], None, True, gen(70001)),
]
histograms = []
for a, bins, rng, density, weights in HIST:
    case = {"data": a, "bins": bins, "range": rng, "density": density, "weights": weights}
    x = np.asarray(data(a), float)
    w = None if weights is None else np.asarray(data(weights), float)
    r = None if rng is None else tuple(float(v) if isinstance(v, str) else v for v in rng)
    try:
        n, edges = np.histogram(x, bins, r, density=density, weights=w)
        case["counts"] = hexes(n)
        case["edges"] = hexes(edges)
    except (ValueError, TypeError) as e:
        case["error"] = str(e)
    try:
        case["bin_edges"] = hexes(np.histogram_bin_edges(x, bins, r, w))
    except (ValueError, TypeError) as e:
        case["bin_edges_error"] = str(e)
    histograms.append(case)

out = {"_note": "NumPy 2.5.3 (BSD-3-Clause), tools/oracle/make_histogram_oracle.py", "sums": sums,
       "histograms": histograms}
(ROOT / "tests" / "oracle" / "histogram.json").write_text(json.dumps(out, indent=None) + "\n")
print(len(sums), "sums,", len(histograms), "histograms;", sum("error" in h for h in histograms), "errors")
