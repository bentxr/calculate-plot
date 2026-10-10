#!/usr/bin/env python3
"""Write data/rcdefaults.json: matplotlib 3.11.2's default rcParams (rcParamsDefault, already
validated), as calculate-plot reads them. Tuples are lists, cyclers {"$cycler": {key: [...]}},
None is null; private ("_internal.*") keys are kept, as matplotlib's code reads them too.

  tools/oracle/.venv/bin/python -I tools/oracle/make_rcdefaults.py
"""
import json
import pathlib

import matplotlib
from cycler import Cycler

OUT = pathlib.Path(__file__).resolve().parents[2] / "data" / "rcdefaults.json"


def value(v):
    if isinstance(v, Cycler):
        return {"$cycler": {k: [value(x) for x in vs] for k, vs in v.by_key().items()}}
    if isinstance(v, (list, tuple)):
        return [value(x) for x in v]
    if isinstance(v, (str, bool, int, float)) or v is None:
        return v
    if hasattr(v, "tolist"):  # NumPy scalars and arrays
        return value(v.tolist())
    if v is matplotlib.rcsetup._auto_backend_sentinel:  # "backend": chosen at run time
        return {"$auto": True}
    raise TypeError(f"{type(v)}: {v!r}")


assert matplotlib.__version__ == "3.11.2"
defaults = {k: value(matplotlib.rcParamsDefault[k]) for k in sorted(matplotlib.rcParamsDefault)}
document = {
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/mpl-data/matplotlibrc and lib/matplotlib/rcsetup.py",
        "(the default rcParams, as matplotlib validates them), written by tools/oracle/make_rcdefaults.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md",
    ],
    "rcParams": defaults,
}
OUT.parent.mkdir(exist_ok=True)
OUT.write_text(json.dumps(document, indent=1, ensure_ascii=False) + "\n")
print(len(defaults), "rcParams")
