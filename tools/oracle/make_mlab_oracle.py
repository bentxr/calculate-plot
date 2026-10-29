#!/usr/bin/env python3
"""Write tests/oracle/mlab.json: matplotlib 3.11.2's mlab spectra (psd, csd, specgram in every mode, cohere,
magnitude_spectrum, angle_spectrum, phase_spectrum) of random real and complex signals, with windows (Hanning, none,
a given array), overlaps that give more and fewer than eight segments, padding, one- and two-sided spectra, and
scaling by frequency or not; outputs as hex floats.

  tools/oracle/.venv/bin/python -I tools/oracle/make_mlab_oracle.py
"""
import json
import pathlib

import numpy as np
import matplotlib

from matplotlib import mlab

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
rng = np.random.default_rng(12252)


def h(v):
    return float.hex(float(v))


def values(a):
    a = np.asarray(a)
    if np.iscomplexobj(a):
        return [[h(z.real), h(z.imag)] for z in a.ravel()]
    return [h(v) for v in a.ravel()]


x = np.round(np.sin(np.arange(600) * 0.37) + rng.normal(0, 0.5, 600), 4)
y = np.round(np.cos(np.arange(600) * 0.21) + rng.normal(0, 0.5, 600), 4)
z = np.round(x + 1j * y, 4)
short = np.round(rng.normal(0, 1, 50), 4)
win = np.round(rng.uniform(0.2, 1.0, 64), 4)

CASES = [
    ("psd", dict(x=x)),
    ("psd", dict(x=x, NFFT=64, Fs=10, noverlap=32)),
    ("psd", dict(x=x, NFFT=64, noverlap=0, pad_to=128, sides="twosided", scale_by_freq=False)),
    ("psd", dict(x=x, NFFT=100, Fs=3, noverlap=99 - 49, window="none")),
    ("psd", dict(x=short, NFFT=64)),
    ("psd", dict(x=z, NFFT=64, noverlap=16)),
    ("psd", dict(x=x, NFFT=64, window="array")),
    ("csd", dict(x=x, y=y, NFFT=64, noverlap=32, Fs=5)),
    ("csd", dict(x=x, y=y, NFFT=128, sides="twosided")),
    ("cohere", dict(x=x, y=y, NFFT=64, noverlap=32)),
    ("specgram", dict(x=x, NFFT=64, noverlap=32, Fs=4)),
    ("specgram", dict(x=x, NFFT=64, noverlap=16, mode="magnitude")),
    ("specgram", dict(x=x, NFFT=64, mode="angle", pad_to=100, noverlap=0)),
    ("specgram", dict(x=x, NFFT=64, mode="phase", noverlap=8)),
    ("specgram", dict(x=x, NFFT=32, mode="complex", sides="twosided", noverlap=4)),
    ("specgram", dict(x=z, NFFT=32, noverlap=8)),
    ("magnitude_spectrum", dict(x=short, Fs=8)),
    ("magnitude_spectrum", dict(x=x, pad_to=1024, sides="twosided")),
    ("angle_spectrum", dict(x=short)),
    ("phase_spectrum", dict(x=short, Fs=3, pad_to=60)),
    ("phase_spectrum", dict(x=z[:40])),
]


def run(name, kw):
    kw = dict(kw)
    w = kw.pop("window", None)
    if w == "none":
        kw["window"] = mlab.window_none
    elif w == "array":
        kw["window"] = win
    out = getattr(mlab, name)(**kw)
    return out


cases = []
for name, kw in CASES:
    result = run(name, kw)
    entry = {"call": name, "kwargs": {k: (v if not isinstance(v, np.ndarray) else None) for k, v in kw.items()
                                       if k not in ("x", "y")}}
    entry["x"] = values(kw["x"])
    if "y" in kw:
        entry["y"] = values(kw["y"])
    if isinstance(kw.get("window"), str) and kw["window"] == "array":
        entry["window"] = values(win)
    entry["results"] = [values(r) for r in result]
    entry["shapes"] = [list(np.shape(r)) for r in result]
    cases.append(entry)

(ROOT / "tests" / "oracle" / "mlab.json").write_text(json.dumps({"cases": cases}, separators=(",", ":")) + "\n")
print(len(cases), "cases")
