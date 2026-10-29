#!/usr/bin/env python3
"""Write tests/oracle/fft.json: NumPy 2.5.3's np.fft.fft (of complex and of real input) and np.fft.rfft (pocketfft)
over lengths of every radix pocketfft takes (2, 3, 4, 5, 7, 11, their mixtures, primes through Bluestein), with
zero padding (n longer than the input) and truncation (n shorter), one transform at a time and batches along axis 0
of a 2-D array (NumPy's vector lanes), as hex floats.

  tools/oracle/.venv/bin/python -I tools/oracle/make_fft_oracle.py
"""
import json
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert np.__version__ == "2.5.3", np.__version__
rng = np.random.default_rng(1225)


def h(v):
    return float.hex(float(v))


def complexes(a):
    return [[h(z.real), h(z.imag)] for z in np.asarray(a).ravel()]


LENGTHS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 20, 25, 30, 31, 32, 49, 60, 64, 77, 97, 100, 121,
           128, 210, 256, 257, 360, 509, 512, 1000, 1024, 1031]
cases = []
for n in LENGTHS:
    re = rng.normal(size=n)
    im = rng.normal(size=n)
    entry = {"n": n, "re": [h(v) for v in re], "im": [h(v) for v in im],
             "fft": complexes(np.fft.fft(re + 1j * im)),
             "fft_real": complexes(np.fft.fft(re)),
             "rfft": complexes(np.fft.rfft(re))}
    cases.append(entry)
# padding and truncation
padded = []
for n, m in [(5, 8), (12, 7), (30, 64), (100, 97)]:
    x = rng.normal(size=n)
    padded.append({"x": [h(v) for v in x], "n": m, "fft": complexes(np.fft.fft(x, n=m)), "rfft": complexes(np.fft.rfft(x, n=m))})
# batches along axis 0 (mlab's layout: rows are samples, columns are segments)
batches = []
for rows, cols in [(16, 3), (256, 5), (60, 2), (31, 4), (128, 1)]:
    a = rng.normal(size=(rows, cols))
    batches.append({"rows": rows, "cols": cols, "a": [h(v) for v in a.ravel()],
                    "fft": complexes(np.fft.fft(a, axis=0)), "fft_n": complexes(np.fft.fft(a, n=2 * rows, axis=0))})

out = {"cases": cases, "padded": padded, "batches": batches}
(ROOT / "tests" / "oracle" / "fft.json").write_text(json.dumps(out, separators=(",", ":")) + "\n")
print(len(cases), "lengths,", len(padded), "padded,", len(batches), "batches")
