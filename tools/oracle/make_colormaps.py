#!/usr/bin/env python3
"""Write data/colormaps.json (every colormap matplotlib 3.11.2 registers: the colour lists of the listed ones, the
segment data of the segmented ones; a channel matplotlib computes with a function is written as its 256 values, since
NumPy's vectorised sin and cos made them), tests/oracle/colormapping.json (Normalize + viridis of random float data, as
ScalarMappable.to_rgba does it for scatter's c values) and tests/oracle/colormaps.json (each registered colormap's
lookup table as a SHA-256 of its float64 bytes with a few rows, the tables of resampled colormaps, and Colormap.__call__
on floats, float32, integers, masked values, alphas and bytes).

  tools/oracle/.venv/bin/python -I tools/oracle/make_colormaps.py
"""
import hashlib
import json
import pathlib
import random

import matplotlib
import numpy as np
from matplotlib import cm, colors

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


def num(x):
    """A float64 as a JSON number that reads back to the same double (Python's repr)."""
    return float(x)


def digest(lut):
    return hashlib.sha256(np.ascontiguousarray(lut, dtype="<f8").tobytes()).hexdigest()


registry = matplotlib.colormaps
listed, segmented = {}, {}
for name in registry:
    cmap = registry[name]
    if isinstance(cmap, colors.ListedColormap):
        if name.endswith("_r") and name[:-2] in registry:
            continue  # reversed() of the base
        listed[name] = [[num(v) for v in c] for c in colors.to_rgba_array(cmap.colors)]
        assert cmap.N == len(listed[name])
        continue
    assert isinstance(cmap, colors.LinearSegmentedColormap), name
    data = cmap._segmentdata
    functions = any(callable(data[k]) for k in data)
    if name.endswith("_r") and name[:-2] in registry and not functions:
        continue  # reversed() of the base
    assert cmap.N == 256 and cmap._gamma == 1.0
    cmap._init()
    spec = {}
    for key, channel in zip(("red", "green", "blue", "alpha"), range(4)):
        if key not in data:
            continue
        if callable(data[key]):
            spec[key] = {"lut": [num(v) for v in cmap._lut[:-3, channel]]}
        else:
            spec[key] = [[num(v) for v in row] for row in np.asarray(data[key], float)]
    segmented[name] = spec

(ROOT / "data" / "colormaps.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/_cm.py and lib/matplotlib/_cm_listed.py (the colormaps'",
        "colours and segment data; the channels _cm.py computes with functions as their 256 values),",
        "written by tools/oracle/make_colormaps.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "listed": listed, "segmented": segmented}, separators=(",", ":")) + "\n")

# Normalize + viridis, as scatter maps its values
rnd = random.Random(21)
cases = []
for i in range(300):
    n = rnd.randint(1, 40)
    kind = rnd.random()
    if kind < 0.6:
        data = [rnd.uniform(-50, 50) for _ in range(n)]
    elif kind < 0.8:
        data = [rnd.choice([0.0, 0.25, 0.5, 1.0, 2.0]) for _ in range(n)]
    else:
        data = [rnd.uniform(0, 1) * 10 ** rnd.randint(-8, 8) for _ in range(n)]
    alpha = rnd.choice([None, None, 0.5, 1.0, 0.0])
    sm = cm.ScalarMappable(norm=colors.Normalize(), cmap="viridis")
    rgba = sm.to_rgba(np.asarray(data, float), alpha)
    cases.append({"data": [h(v) for v in data], "alpha": alpha, "rgba": [[h(v) for v in r] for r in rgba]})
(ROOT / "tests" / "oracle" / "colormapping.json").write_text(json.dumps(cases, indent=0) + "\n")


# every registered colormap's table, and resampled ones
def table(cmap):
    cmap._init()
    lut = cmap._lut
    return {"N": cmap.N, "sha256": digest(lut), "rows": {str(i): [h(v) for v in lut[i]] for i in sorted({0, cmap.N // 2, cmap.N - 1, cmap.N, cmap.N + 1, cmap.N + 2})}}


tables = []
for name in registry:
    entry = {"name": name, "table": table(registry[name]), "resampled": {}}
    for size in (1, 2, 7, 300):
        try:
            entry["resampled"][str(size)] = table(registry[name].resampled(size))
        except Exception as e:  # noqa: BLE001
            entry["resampled"][str(size)] = {"error": f"{type(e).__name__}: {e}"}
    tables.append(entry)

rng = np.random.default_rng(1018)
names = list(registry)
calls = []
for i in range(400):
    name = names[int(rng.integers(len(names)))]
    cmap = registry[name]
    n = int(rng.integers(1, 30))
    kind = ["float64", "float32", "int16", "int64"][i % 4]
    if kind.startswith("float"):
        x = rng.uniform(-0.3, 1.3, n)
        x[rng.uniform(size=n) < 0.1] = np.nan
        x[rng.uniform(size=n) < 0.1] = 1.0
        x = x.astype(kind)
    else:
        x = rng.integers(-3, cmap.N + 3, n).astype(kind)
    mask = rng.uniform(size=n) < 0.15 if i % 3 == 0 else None
    arg = np.ma.array(x, mask=mask) if mask is not None else x
    alpha_kind = i % 5
    alpha = None if alpha_kind < 2 else float(rng.uniform(-0.2, 1.2)) if alpha_kind < 4 else rng.uniform(-0.2, 1.2, n)
    as_bytes = i % 7 == 0
    out = cmap(arg, alpha=alpha, bytes=as_bytes)
    calls.append({
        "name": name, "dtype": kind, "x": [h(v) for v in np.asarray(x, float)],
        "mask": [int(k) for k in np.flatnonzero(mask)] if mask is not None else None,
        "alpha": None if alpha is None else (h(alpha) if np.ndim(alpha) == 0 else [h(a) for a in alpha]),
        "bytes": as_bytes,
        "rgba": [[int(v) if as_bytes else h(v) for v in row] for row in np.asarray(out)]})

(ROOT / "tests" / "oracle" / "colormaps.json").write_text(json.dumps({
    "_provenance": "matplotlib 3.11.2 colormaps, written by tools/oracle/make_colormaps.py",
    "tables": tables, "calls": calls}, indent=0) + "\n")
print(len(listed), "listed,", len(segmented), "segmented,", len(tables), "tables,", len(calls), "calls")
