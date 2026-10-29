#!/usr/bin/env python3
"""Write tests/oracle/object_arrays.json: NumPy 2.5.3's repr() and str() of 2-D object arrays (as subplot_mosaic
shows its layouts in its errors): small and long elements, rows that wrap at the line width, lists as elements.

  tools/oracle/.venv/bin/python -I tools/oracle/make_object_array_oracle.py
"""
import json
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert np.__version__ == "2.5.3", np.__version__


def array(rows):
    # subplot_mosaic's _make_array
    out = np.zeros((len(rows), len(rows[0])), dtype=object)
    for j, r in enumerate(rows):
        for k, v in enumerate(r):
            out[j, k] = v
    return out


def words(a):
    # arrayprint._object_format
    return [[("list(%r)" % v) if type(v) is list else repr(v) for v in row] for row in a]


CASES = [[[1, 22], [333, 1]], [["A", "BB"], ["CCC", "A"]], [["A", [["B", "C"]]], ["D", "D"]], [["A", "B", "C"]],
         [["A"], ["B"]], [["a" * 30, "b" * 30, "c" * 30]], [["x" * 20] * 5, ["y" * 3] * 5], [["q" * 70]],
         [["q" * 58, "r"]], [["q" * 57, "r"]], [["k" * 25, "l" * 25], ["m", "n"]], [["s" * 33, "t" * 33]],
         [["u" * 30, "v" * 30], ["w" * 52, "z"]]]
cases = [{"words": words(array(c)), "repr": repr(array(c)), "str": str(array(c))} for c in CASES]
(ROOT / "tests" / "oracle" / "object_arrays.json").write_text(json.dumps({"cases": cases}, indent=1) + "\n")
print(len(cases), "arrays")
