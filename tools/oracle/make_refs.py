#!/usr/bin/env python3
"""Regenerate the renderer corpus: tests/corpus/<case>/{trace.json, ref.png}.

Run with the reference environment (requirements.lock), e.g.
  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_refs.py [case ...]
The PNG holds the exact RGBA buffer matplotlib's Agg renderer produced.
"""
import hashlib
import json
import os
import pathlib
import sys

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    sys.exit("set MPL_IGNORE_SYSTEM_FONTS=1 so that only matplotlib's bundled fonts are used")

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import matplotlib  # noqa: E402

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
from PIL import Image  # noqa: E402

import cases  # noqa: E402
import record  # noqa: E402
import replay  # noqa: E402
import numpy as np  # noqa: E402

CORPUS = HERE.parents[1] / "tests" / "corpus"


def main(names):
    assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
    for name in names or sorted(cases.CASES):
        plt.rcdefaults()
        fig = cases.CASES[name]()
        trace, rgba = record.record(fig)
        plt.close(fig)
        # The trace must be complete: matplotlib's own renderer, fed only the trace (after a JSON
        # round trip), must reproduce the figure exactly.
        again = replay.replay(json.loads(json.dumps(trace)))
        if not np.array_equal(again, rgba):
            sys.exit(f"{name}: replaying the trace does not reproduce the figure "
                     f"({int((again != rgba).any(axis=2).sum())} pixels differ)")
        out = CORPUS / name
        out.mkdir(parents=True, exist_ok=True)
        (out / "trace.json").write_text(json.dumps(trace, separators=(",", ":")) + "\n")
        Image.fromarray(rgba, "RGBA").save(out / "ref.png", optimize=True)
        # An independent check of PNG decoding: the hash of the raw pixels, row by row, RGBA.
        decoded = np.asarray(Image.open(out / "ref.png").convert("RGBA"))
        assert np.array_equal(decoded, rgba)
        (out / "ref.json").write_text(json.dumps({
            "width": int(rgba.shape[1]), "height": int(rgba.shape[0]),
            "rgba_sha256": hashlib.sha256(rgba.tobytes()).hexdigest()}, indent=1) + "\n")
        print(f"{name}: {len(trace['events'])} events, {rgba.shape[1]}x{rgba.shape[0]}")


if __name__ == "__main__":
    main(sys.argv[1:])
