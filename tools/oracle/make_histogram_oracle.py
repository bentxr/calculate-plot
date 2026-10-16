#!/usr/bin/env python3
"""Write tests/oracle/histogram.json: NumPy 2.5.3's np.sum of float64 arrays, np.histogram_bin_edges and
np.histogram (bins, range, density, weights; bins by name: the rules of _hist_bin_selectors, on float64
and on int64 data), and the routines those rules use (np.dot of float64 vectors, np.mean, np.std,
np.percentile(x, [75, 25])), as float.hex; or the error NumPy raises. The data are given by a generator
both sides compute exactly ((i * p) % q / q * scale + offset, or for integers (i * p) % q + offset), so
that long arrays need not be stored.

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
        if spec.get("int"):
            return [(i * spec["p"]) % spec["q"] + spec["offset"] for i in range(spec["n"])]
        return generated(spec)
    return [float(v) if isinstance(v, str) else v for v in spec]


def array(spec, integer=False):
    return np.asarray(data(spec), np.int64 if integer else float)


def hexes(values):
    return [float.hex(float(v)) for v in values]


def gen(n, p=7919, q=10007, scale=2.0, offset=-1.0):
    return {"n": n, "p": p, "q": q, "scale": scale, "offset": offset}


def igen(n, p=7919, q=101, offset=-50):
    return {"n": n, "p": p, "q": q, "offset": offset, "int": True}


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

# np.dot of float64 vectors (BLAS ddot), as the 'stone' rule takes it of the bin probabilities
dots = []
for n in list(range(1, 70)) + [100, 127, 128, 129, 255, 300, 1000]:
    for spec, other in ((gen(n), None), (gen(n, 31, 97, 0.1, 0.0), gen(n)), (gen(n, 104729, 1000003, 1e6, -3.3), None)):
        a = array(spec)
        b = a if other is None else array(other)
        dots.append({"a": spec, "b": other, "dot": float.hex(float(a.dot(b)))})
    p = np.histogram(array(gen(n * 3 + 1)), bins=n, range=(-1.0, 1.0))[0] / (n * 3 + 1)
    dots.append({"a": hexes(p), "b": None, "dot": float.hex(float(p.dot(p))), "hex": True})

# np.mean, np.std and np.percentile(x, [75, 25]) of float64 and int64 data
stats = []
STATS = [([1.0], False), ([1, 2], False), ([1, 2, 3], False), ([3, 1, 2, 2], False), ([0.1 * i for i in range(30)], False),
         ([1e300, -1e300, 5.0], False), ([5, 5, 5, 5], False), ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], True), ([7], True)]
for n in [2, 3, 4, 5, 7, 8, 9, 10, 16, 17, 100, 129, 1000, 8191, 8192, 8193, 20000]:
    STATS += [(gen(n), False), (gen(n, 104729, 1000003, 1e6, -3.3), False), (igen(n), True),
              (igen(n, 104729, 1000003, -500000), True)]
for spec, integer in STATS:
    a = array(spec, integer)
    p75, p25 = np.percentile(a, [75, 25])
    stats.append({"data": spec, "int": integer, "mean": float.hex(float(np.mean(a))), "std": float.hex(float(np.std(a))),
                  "p75": float.hex(float(p75)), "p25": float.hex(float(p25))})

# bins by name: histogram_bin_edges and histogram, with and without a range
NAMES = ["auto", "fd", "doane", "scott", "stone", "rice", "sturges", "sqrt"]
NAMED = [([1.5], False, None), ([2, 2, 2], False, None), ([1, 2], False, None), ([1, 2, 3], False, None),
         ([0.1 * i for i in range(30)], False, None), ([0.1 * i for i in range(30)], False, [0.5, 2.0]),
         ([0.0, 1.0, NAN, 0.5, 0.25], False, [0.0, 1.0]), ([float(i * i) for i in range(40)], False, None),
         ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], True, None), ([1, 2, 2, 3, 3, 3, 4, 4, 4, 4], True, [1, 4]),
         ([3, 3, 3], True, [3, 3]), ([1, 2, 3], False, [2.5, 2.5]), ([1, 2, 3], False, [5, 6]),
         (gen(100), False, None), (gen(1000), False, None), (gen(1000), False, [-0.5, 0.75]),
         (gen(1000, 31, 97, 0.1, 0.0), False, None), (gen(500, 104729, 1000003, 1e6, -3.3), False, None),
         (igen(1000, 7919, 4, 0), True, None), (igen(1000), True, [-50, 50]), (igen(20000), True, None),
         (gen(20000), False, None), (gen(3), False, None), (gen(9), False, None), (gen(60), False, None)]
named = []
for spec, integer, rng in NAMED:
    a = array(spec, integer)
    r = None if rng is None else tuple(rng)
    for name in NAMES:
        case = {"data": spec, "int": integer, "bins": name, "range": rng}
        try:
            case["edges"] = hexes(np.histogram_bin_edges(a, name, r))
            n, _ = np.histogram(a, name, r)
            case["counts"] = hexes(n)
        except (ValueError, TypeError) as e:
            case["error"] = str(e)
        named.append(case)
for spec, integer, rng, name, weights in [([1, 2, 3], False, None, "bogus", None), ([1, 2, 3], False, None, "auto", [1, 1, 1]),
                                          ([NAN, 1.0], False, None, "auto", None), ([], False, None, "fd", None),
                                          ([], False, [0, 1], "sturges", None)]:
    case = {"data": spec, "int": integer, "bins": name, "range": rng, "weights": weights}
    try:
        case["edges"] = hexes(np.histogram_bin_edges(array(spec, integer), name, rng, weights))
    except (ValueError, TypeError) as e:
        case["error"] = str(e)
    named.append(case)

out = {"_note": "NumPy 2.5.3 (BSD-3-Clause), tools/oracle/make_histogram_oracle.py", "sums": sums,
       "histograms": histograms, "dots": dots, "stats": stats, "named": named}
(ROOT / "tests" / "oracle" / "histogram.json").write_text(json.dumps(out, indent=None) + "\n")
print(len(sums), "sums,", len(histograms), "histograms;", sum("error" in h for h in histograms), "errors;", len(dots), "dots,",
      len(stats), "stats,", len(named), "named;", sum("error" in h for h in named), "errors")
