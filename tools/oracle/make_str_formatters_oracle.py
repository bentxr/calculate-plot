#!/usr/bin/env python3
"""Write tests/oracle/str_formatters.json: matplotlib 3.11.2's FormatStrFormatter(fmt)(x) and
StrMethodFormatter(fmt)(x, pos) (with rcParams axes.unicode_minus on and off) over format strings and tick values:
the label, or the exception raised.

  tools/oracle/.venv/bin/python -I tools/oracle/make_str_formatters_oracle.py
"""
import json
import pathlib

import matplotlib
import numpy as np
from matplotlib import ticker

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"

PERCENT = ["%.1f", "%g", "%d", "%5.2f", "%+.3e", "%.0f%%", "x=%s", "%r", "%.2f %s", "{x:.2f}", "%i", "%10.4g", "%-8.2f|",
           "%x", "%o", "%c", "%.3G", "%E"]
BRACE = ["{x:.2f}", "{x:+.1f} u", "{x}", "{x:g}", "{pos}:{x:.1f}", "{x:>8.3f}", "{x:e}", "{x:%}", "{x:,.2f}", "{}",
         "{0}", "{x!r}", "{x:.0f}", "{y}", "{x:+08.2f}", "{{x}}", "{x:n}"]
VALUES = [0.0, 1.0, -1.0, 2.5, -2.5, 0.123456789, -1234.5678, 1e-7, -3e9, 100.0, -0.0, 7.0, float("nan"), float("inf")]
cases = []
for fmt in PERCENT:
    for x in VALUES:
        case = {"kind": "percent", "fmt": fmt, "x": float.hex(x) if np.isfinite(x) else repr(x)}
        try:
            case["text"] = ticker.FormatStrFormatter(fmt)(np.float64(x))
        except Exception as e:
            case["error"] = f"{type(e).__name__}: {e}"
        cases.append(case)
for fmt in BRACE:
    for minus in (True, False):
        for k, x in enumerate(VALUES):
            case = {"kind": "brace", "fmt": fmt, "x": float.hex(x) if np.isfinite(x) else repr(x), "pos": k, "unicode_minus": minus}
            with matplotlib.rc_context({"axes.unicode_minus": minus}):
                try:
                    case["text"] = ticker.StrMethodFormatter(fmt)(np.float64(x), k)
                except Exception as e:
                    case["error"] = f"{type(e).__name__}: {e}"
            cases.append(case)
(ROOT / "tests" / "oracle" / "str_formatters.json").write_text(json.dumps({"cases": cases}, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "cases")
