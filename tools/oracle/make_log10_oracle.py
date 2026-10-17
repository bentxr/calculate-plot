#!/usr/bin/env python3
"""Write tests/oracle/log10.json: base-10 logarithms as the reference environment computes them.

matplotlib's log scales call np.log10 and math.log10, which both end in the C library's log10 here
(this script checks that they agree: NumPy has its own vector code on some processors). The C
library of the reference environment is glibc, whose log10 is not correctly rounded, so other C
libraries give other last bits; calculate-plot computes log10 itself, the way glibc does.

  tools/oracle/.venv/bin/python -I tools/oracle/make_log10_oracle.py
"""
import json
import math
import pathlib
import platform
import random

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert platform.libc_ver()[0] == "glibc", platform.libc_ver()

rnd = random.Random(10)
xs = []
xs += [float(f"1e{n}") for n in range(-320, 309)]                       # decades, subnormal ones too
xs += [k * float(f"1e{n}") for n in range(-20, 21) for k in range(2, 10)]  # minor tick values
xs += [2.0 ** n for n in range(-1074, 1024, 7)]
xs += [math.nextafter(1.0, 0.0), math.nextafter(1.0, 2.0), 5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
       0.6, 0.5, 2.0, 3.0, math.e, math.pi]
xs += [rnd.uniform(0, 1000) for _ in range(3000)]
xs += [10 ** rnd.uniform(-300, 300) for _ in range(3000)]
xs += [1 + rnd.uniform(-1e-3, 1e-3) for _ in range(500)]
xs += [float(i) / 8 for i in range(1, 1001)]

values = [math.log10(x) for x in xs]
assert all(float(a) == b for a, b in zip(np.log10(np.array(xs)), values)), "np.log10 and math.log10 differ here"
out = {"libc": " ".join(platform.libc_ver()), "values": [[float.hex(x), float.hex(v)] for x, v in zip(xs, values)]}
(ROOT / "tests" / "oracle" / "log10.json").write_text(json.dumps(out, separators=(",", ":")) + "\n")
print(len(xs), "values,", out["libc"])
