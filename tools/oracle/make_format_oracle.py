#!/usr/bin/env python3
"""Write tests/oracle/auto_format.json: matplotlib 3.11.2's cbook._auto_format_str(fmt, value) (what
bar_label makes of its fmt) for format strings and values, the value a np.float64 or a np.int64 as
bar_label's data values are: the text, or the exception it raises.

  tools/oracle/.venv/bin/python -I tools/oracle/make_format_oracle.py
"""
import json
import pathlib

import numpy as np
import matplotlib
from matplotlib import cbook

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"

FORMATS = ["%g", "%.1f", "%.2f", "%d", "%i", "%u", "%5.1f", "%-8.3e", "%+g", "% g", "%#g", "%.0f", "%E", "%G", "%F",
           "%s", "%r", "%.3s", "%8s", "%-6s|", "%%", "%g%%", "x=%g", "%g units", "abc", "", "%.3d", "%05d", "%x", "%X",
           "%o", "%c", "%q", "%g %g", "%(a)s", "%*d", "%.10g", "%.15e", "%.20f", "%10.4f|", "%-10.4f|", "%+.2e",
           "{}", "{:.2f}", "{:g}", "{:e}", "{:+.1%}", "{:>8.2f}", "{0}", "{:,.2f}", "{:d}", "{:x}", "{0:<7}|",
           "{:^9.3g}|", "{:*^9.1f}", "{:010.3f}", "{:+}", "{:.3}", "{1}", "{a}", "{{}}", "{{{:g}}}", "{", "}",
           "{:%}", "{:.0%}", "{:G}", "{:E}", "{:#g}", "{: .2f}", "{!r}", "{!s}", "{:=+8.2f}", "%g and {}", "{:.2f} %"]
FLOATS = [0.0, -0.0, 3.0, 3.14159, -2.5, 1e-05, 123456789.0, 1e16, 0.5, 2.5, 1.5, 1 / 3, 1e300, -7.25e-12,
          float("inf"), float("-inf"), 999.9999, 0.125]
INTS = [3, -2, 0, 123456789, 65, 7]

cases = []
for fmt in FORMATS:
    for v, kind in [(np.float64(x), "float") for x in FLOATS] + [(np.int64(x), "int") for x in INTS]:
        case = {"fmt": fmt, "value": float.hex(float(v)) if kind == "float" else int(v), "int": kind == "int"}
        try:
            case["text"] = cbook._auto_format_str(fmt, v)
        except Exception as e:  # noqa: BLE001
            case["error"] = f"{type(e).__name__}: {e}"
        cases.append(case)
(ROOT / "tests" / "oracle" / "auto_format.json").write_text(
    json.dumps({"_note": "matplotlib 3.11.2 cbook._auto_format_str, tools/oracle/make_format_oracle.py", "cases": cases},
               ensure_ascii=False) + "\n")
print(len(cases), "cases,", sum("error" in c for c in cases), "errors")
