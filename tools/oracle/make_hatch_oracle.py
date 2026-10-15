#!/usr/bin/env python3
"""Write tests/oracle/hatch.json: matplotlib 3.11.2's hatch paths (hatch.get_path) for a set of
patterns and densities: vertices as float.hex, codes, and whether the path would be simplified; or
the error matplotlib raises.

  tools/oracle/.venv/bin/python -I tools/oracle/make_hatch_oracle.py
"""
import json
import pathlib
import warnings

import matplotlib
from matplotlib import hatch

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")

PATTERNS = ["/", "\\", "|", "-", "+", "x", "X", "o", "O", ".", "*", "//", "\\\\", "xx", "|||", "--", "++", "oo", "OO",
            "..", "**", "/o", "\\O", "|*", "-.", "+o", "x*", "X.", "*-", "o-|", "/\\|-+xXoO.*", "", "abc", "/a", " ",
            "///////", "......"]
DENSITIES = [6, 1, 2, 3, 10]

cases = []
for density in DENSITIES:
    for p in PATTERNS:
        try:
            path = hatch.get_path(p, density)
        except ValueError as e:  # the shapes' arrays have room for an even number of rows only
            cases.append({"pattern": p, "density": density, "error": str(e)})
            continue
        cases.append({
            "pattern": p, "density": density,
            "vertices": [float.hex(float(v)) for v in path.vertices.ravel()],
            "codes": None if path.codes is None else [int(c) for c in path.codes],
            "simplify": bool(path.should_simplify),
        })
(ROOT / "tests" / "oracle" / "hatch.json").write_text(json.dumps({"cases": cases}, indent=None) + "\n")
print(len(cases), "hatch paths")
