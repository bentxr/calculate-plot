#!/usr/bin/env python3
"""Write tests/oracle/contour.json: contourpy 1.4.0's mpl2014 generator as matplotlib 3.11.2 uses it
(contour_generator(x, y, z, name='mpl2014', corner_mask=, line_type=SeparateCode, fill_type=OuterCode,
chunk_size=)) over random fields, some with masked (NaN) values: the lines of some levels and the filled regions
between pairs of levels, as vertices (float.hex) and kind codes.

  tools/oracle/.venv/bin/python -I tools/oracle/make_contour_oracle.py
"""
import json
import pathlib

import contourpy
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert contourpy.__version__ == "1.4.0"
rng = np.random.default_rng(1018)


def h(v):
    return float.hex(float(v))


cases = []
for k in range(60):
    ny, nx = int(rng.integers(2, 12)), int(rng.integers(2, 12))
    x1, y1 = np.cumsum(rng.uniform(0.2, 1.5, nx)), np.cumsum(rng.uniform(0.2, 1.5, ny))
    x, y = np.meshgrid(x1, y1)
    if k % 4 == 3:  # a warped grid
        x = x + 0.2 * np.sin(y)
        y = y + 0.1 * np.cos(x)
    z = np.sin(x * rng.uniform(0.3, 1.5)) * np.cos(y * rng.uniform(0.3, 1.5)) * 3 + rng.normal(0, 0.3, (ny, nx))
    if k % 3 == 0:
        z[rng.uniform(size=z.shape) < 0.15] = np.nan
    corner = bool(k % 2)
    chunk = [None, 0, 2, 3][k % 4]
    gen = contourpy.contour_generator(x, y, z, name="mpl2014", corner_mask=corner,
                                      line_type=contourpy.LineType.SeparateCode,
                                      fill_type=contourpy.FillType.OuterCode, chunk_size=chunk)
    levels = sorted(float(v) for v in rng.uniform(np.nanmin(z), np.nanmax(z), 4))
    case = {"x": [h(v) for v in x.ravel()], "y": [h(v) for v in y.ravel()], "z": [h(v) for v in z.ravel()],
            "ny": ny, "nx": nx, "corner_mask": corner, "chunk_size": chunk, "levels": [h(v) for v in levels],
            "lines": [], "filled": []}
    for level in levels:
        verts, codes = gen.lines(level)
        case["lines"].append({"vertices": [[h(v) for v in p.ravel()] for p in verts], "codes": [[int(c) for c in cs] for cs in codes]})
    for lo, hi in zip(levels[:-1], levels[1:]):
        verts, codes = gen.filled(lo, hi)
        case["filled"].append({"vertices": [[h(v) for v in p.ravel()] for p in verts], "codes": [[int(c) for c in cs] for cs in codes]})
    cases.append(case)
(ROOT / "tests" / "oracle" / "contour.json").write_text(json.dumps({"cases": cases}) + "\n")
print(len(cases), "cases")
