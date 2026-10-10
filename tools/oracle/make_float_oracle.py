#!/usr/bin/env python3
"""Write tests/oracle/float_format.json: how Python and NumPy spell doubles.

For each double (as its exact hexadecimal form): repr(), repr(np.float64(x)), and matplotlib's
_short_float_fmt (f'{x:f}' without trailing zeros or point). calculate-plot must spell them alike.
"""
import json
import math
import pathlib
import random
import struct

import numpy as np

OUT = pathlib.Path(__file__).resolve().parents[2] / "tests" / "oracle" / "float_format.json"


def short_float_fmt(x):
    return f'{x:f}'.rstrip('0').rstrip('.')


values = [0.0, -0.0, 1.0, -1.0, 0.1, 0.5, 1e-5, 1e-4, 9.999e-5, 0.0001, 1e15, 1e16, 9999999999999998.0,
          1e17, 123456789012345680.0, 5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
          8.639999999999986, 184.32000000000002, 72.0, 230.4, 0.111111111111, 1 / 3, 2 / 3, 100.0,
          1e22, 1e23, 0.30000000000000004, 3.14159, 2.5, -2.5, 0.0000005, 0.0000015, 0.0000025,
          1234.5678, 1e-7, 12345.6789e10, 65536.0, 0.000001, 0.9999995, 0.9999994999999999]
random.seed(7)
for _ in range(600):
    exponent = random.choice([random.randint(-12, 22), random.randint(-320, 308)])
    values.append(random.uniform(-1, 1) * 10.0 ** exponent)
for _ in range(400):
    bits = random.getrandbits(64)
    x = struct.unpack("<d", struct.pack("<Q", bits))[0]
    if math.isfinite(x):
        values.append(x)

cases = [{"hex": float.hex(x), "repr": repr(x), "numpy": repr(np.float64(x)),
          "short": short_float_fmt(x) if abs(x) < 1e30 else None} for x in values]
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(cases, indent=0) + "\n")
print(f"{len(cases)} doubles")
