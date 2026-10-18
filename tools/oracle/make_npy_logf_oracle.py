#!/usr/bin/env python3
"""Write tests/oracle/npy_logf.json: NumPy 2.5.3's np.log of float32 arrays on the reference machine (x86-64 with AVX2
and FMA, where NumPy uses its own SIMD kernel): special values, every power of two, subnormals, and 40 000 random
floats, as float.hex pairs (input, result).

  tools/oracle/.venv/bin/python -I tools/oracle/make_npy_logf_oracle.py
"""
import json
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert np.__version__ == "2.5.3"
rng = np.random.default_rng(1018)
special = np.array([0.0, -0.0, 1.0, -1.0, np.inf, -np.inf, np.nan, -np.nan, 0.5, 2.0, 0.70710677, 0.70710683,
                    np.finfo(np.float32).tiny, np.finfo(np.float32).max, 1e-45, 3e-39], np.float32)
powers = np.ldexp(np.float32(1), np.arange(-149, 128)).astype(np.float32)
bits = rng.integers(0, 0x7F800000, 30000, dtype=np.uint32).view(np.float32)
near_one = np.float32(1) + rng.uniform(-0.3, 0.3, 10000).astype(np.float32)
x = np.concatenate([special, powers, bits, near_one]).astype(np.float32)
with np.errstate(all="ignore"):
    y = np.log(x)
values = [[float.hex(float(a)), float.hex(float(b))] for a, b in zip(x, y)]
(ROOT / "tests" / "oracle" / "npy_logf.json").write_text(json.dumps({
    "_provenance": "NumPy 2.5.3 np.log of float32 on x86-64 AVX2+FMA, written by tools/oracle/make_npy_logf_oracle.py",
    "values": values}) + "\n")
print(len(values), "values")
