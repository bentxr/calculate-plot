#!/usr/bin/env python3
"""Check that the exported scripts draw the same pixels whatever the locale of who runs them.
Matplotlib shapes text for the language of the process locale (Spanish, for one, does not form the
"fi" ligature of DejaVu Sans), so the scripts set the locale themselves. Every
tests/figures/<case>/fig.py is run here in a process of its own under another locale (es_ES.UTF-8
unless one is given) and what it saves is compared with ref.png; then once more as on a system
without the C.UTF-8 locale, where the scripts fall back to C.

  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/check_locale.py [LOCALE]
"""
import concurrent.futures
import locale
import os
import pathlib
import subprocess
import sys

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    sys.exit("set MPL_IGNORE_SYSTEM_FONTS=1")

ROOT = pathlib.Path(__file__).resolve().parents[2]
FIGURES = ROOT / "tests" / "figures"

CHILD = r"""
import io, locale, runpy, sys
started_in = locale.setlocale(locale.LC_CTYPE)
import matplotlib
matplotlib.use("agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
assert started_in == sys.argv[3], started_in
if sys.argv[4] == "without C.UTF-8":
    setlocale = locale.setlocale
    def without(category, name=None):
        if name == "C.UTF-8":
            raise locale.Error("unsupported locale setting")
        return setlocale(category, name)
    locale.setlocale = without
namespace = runpy.run_path(sys.argv[1], run_name="calculate_plot_oracle")
fig = namespace.get("fig") or plt.gcf()
png = io.BytesIO()
fig.savefig(png, format="png")
got = np.asarray(Image.open(png).convert("RGBA"))
want = np.asarray(Image.open(sys.argv[2]).convert("RGBA"))
if got.shape != want.shape:
    sys.exit("the size is %s, not %s" % (got.shape, want.shape))
if not np.array_equal(got, want):
    sys.exit("%d pixels differ" % int((got != want).any(axis=2).sum()))
if locale.setlocale(locale.LC_CTYPE) != ("C" if sys.argv[4] == "without C.UTF-8" else "C.UTF-8"):
    sys.exit("the script left the locale " + locale.setlocale(locale.LC_CTYPE))
"""


def check(case, name, system):
    env = dict(os.environ, LC_ALL=name)
    run = subprocess.run([sys.executable, "-I", "-c", CHILD, str(case / "fig.py"), str(case / "ref.png"), name, system],
                         env=env, capture_output=True, text=True)
    return case.name, run.returncode, run.stderr.strip().splitlines()[-1:] or [""]


def main(name):
    try:
        locale.setlocale(locale.LC_CTYPE, name)
    except locale.Error:
        sys.exit(f"the locale {name} is not installed")
    cases = sorted(c for c in FIGURES.iterdir() if (c / "ref.png").exists())
    wrong = 0
    for system in ["with C.UTF-8", "without C.UTF-8"]:
        failed = 0
        with concurrent.futures.ThreadPoolExecutor(8) as pool:
            for case, code, message in pool.map(lambda c: check(c, name, system), cases):
                if code != 0:
                    failed += 1
                    print(f"{case}: {message[0]}")
        print(f"{len(cases) - failed} of {len(cases)} scripts draw the reference under {name}, {system}")
        wrong += failed
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "es_ES.UTF-8")
