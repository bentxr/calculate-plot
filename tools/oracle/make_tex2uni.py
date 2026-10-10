#!/usr/bin/env python3
"""Write data/tex2uni.json: matplotlib 3.11.2's table of TeX symbol names and their Unicode code
points (_mathtext_data.tex2uni), which mathtext uses to read commands such as \\times.

  tools/oracle/.venv/bin/python -I tools/oracle/make_tex2uni.py
"""
import json
import pathlib

import matplotlib
from matplotlib._mathtext_data import tex2uni

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
(ROOT / "data" / "tex2uni.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext_data.py (tex2uni), written by",
        "tools/oracle/make_tex2uni.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "tex2uni": dict(sorted(tex2uni.items()))}, indent=0) + "\n")
print(len(tex2uni), "names")
