#!/usr/bin/env python3
"""Rebuild the renderer corpus (tools/oracle/cases.py, recorded as traces in tests/corpus) as figure
files: each case function runs under the call recorder of conformance.py, and what it records is
written to tests/figures/corpus_<case>/fig.json. Cases that need calls the recorder cannot write (or
the engine does not have) are listed with the reason. make_figures.py then records the references of
the new cases, and this script checks that they draw the corpus' own image (tests/corpus/<case>/ref.png).

  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/corpus_models.py [--check]
"""
import json
import os
import pathlib
import sys

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    sys.exit("set MPL_IGNORE_SYSTEM_FONTS=1")
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import matplotlib  # noqa: E402

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import cases  # noqa: E402
import conformance  # noqa: E402

ROOT = HERE.parents[1]


def record(name):
    plt.close("all")
    plt.rcdefaults()
    rec = conformance.Recorder()
    conformance.ACTIVE = rec
    try:
        fig = cases.CASES[name]()
    finally:
        conformance.ACTIVE = None
    if rec.problem:
        return None, rec.problem
    if len(plt.get_fignums()) > 1:
        return None, "several figures"
    try:
        return rec.figure_file({}), None
    except conformance.NotRecorded as e:
        return None, str(e)
    finally:
        plt.close(fig)


def main():
    assert matplotlib.__version__ == "3.11.2"
    if "--check" in sys.argv:
        for case in sorted((ROOT / "tests" / "figures").glob("corpus_*")):
            name = case.name.removeprefix("corpus_")
            new = np.asarray(Image.open(case / "ref.png").convert("RGBA"))
            old = np.asarray(Image.open(ROOT / "tests" / "corpus" / name / "ref.png").convert("RGBA"))
            print(f"{name}: {'same image' if new.shape == old.shape and np.array_equal(new, old) else 'DIFFERENT IMAGE'}")
        return
    conformance.install_recorder()
    for name in sorted(cases.CASES):
        model, why = record(name)
        if model is None:
            print(f"{name}: not recorded: {why}")
            continue
        out = ROOT / "tests" / "figures" / f"corpus_{name}"
        out.mkdir(exist_ok=True)
        (out / "fig.json").write_text(json.dumps(model, ensure_ascii=False) + "\n")
        print(f"{name}: written")


if __name__ == "__main__":
    main()
