#!/usr/bin/env python3
"""Write the style sheets matplotlib 3.11.2 bundles (mpl-data/stylelib/*.mplstyle) as calculate-plot
reads them: each style's settings as the text the file gives (the line split at its first colon,
comment and blanks stripped, double quotes taken off), so that they go through the rcParams
validators as rc_params_from_file sends them. Keys in alphabetical order, as RcParams iterates them.
The texts are checked here: validated by matplotlib they are style.library[name].

  data/styles.json            every style but Solarize_Light2 (Matplotlib's licence)
  data/styles_solarized.json  Solarize_Light2 (its colours are Solarized's: MIT, LICENSES/Solarized.txt)

With them, tests/oracle/styles.json: what plt.style.use(spec) leaves in rcParams after
plt.rcdefaults(), for every style and for lists, aliases and names matplotlib refuses: the keys whose
value is not the default, in the form tools/oracle/make_rcdefaults.py writes values; or the error.

  MPLCONFIGDIR=$(mktemp -d) tools/oracle/.venv/bin/python -I tools/oracle/make_styles.py
"""
import json
import os
import pathlib
import warnings

import matplotlib
import matplotlib.pyplot as plt
from cycler import Cycler
from matplotlib import cbook, style

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
STYLELIB = pathlib.Path(matplotlib.get_data_path()) / "stylelib"
SOLARIZED = "Solarize_Light2"
MATPLOTLIB_PROVENANCE = [
    "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
    "Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md",
]


def texts(path):
    """The settings of a style file as rc_params_from_file reads them, before validation."""
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = cbook._strip_comment(line)
        if not stripped:
            continue
        key, colon, val = stripped.partition(":")
        assert colon, (path, line)
        key, val = key.strip(), val.strip()
        if val.startswith('"') and val.endswith('"'):
            val = val[1:-1]
        assert key not in out, (path, key)
        assert key in matplotlib.rcsetup._validators, (path, key)
        out[key] = val
    return dict(sorted(out.items()))


def value(v):
    if isinstance(v, Cycler):
        return {"$cycler": {k: [value(x) for x in vs] for k, vs in v.by_key().items()}}
    if isinstance(v, (list, tuple)):
        return [value(x) for x in v]
    if isinstance(v, float) and v != v:
        return {"$float": "nan"}
    if isinstance(v, float) and v in (float("inf"), float("-inf")):
        return {"$float": "inf" if v > 0 else "-inf"}
    if isinstance(v, (str, bool, int, float)) or v is None:
        return v
    if hasattr(v, "tolist"):
        return value(v.tolist())
    raise TypeError(f"{type(v)}: {v!r}")


def same_as_library(name, settings):
    validated = matplotlib.RcParams()
    for key, text in settings.items():
        validated[key] = text
    library = style.library[name]
    assert list(validated) == list(library), name
    for key in library:
        assert value(validated[key]) == value(library[key]), (name, key)


def outcome(spec):
    plt.rcdefaults()
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            style.use(spec)
    except Exception as e:  # noqa: BLE001: matplotlib's own exception, kept as text
        return {"spec": spec, "error": f"{type(e).__name__}: {e}"}
    changed = {}
    for key in sorted(matplotlib.rcParamsDefault):
        if key == "backend":
            continue
        now, default = value(matplotlib.rcParams[key]), value(matplotlib.rcParamsDefault[key])
        if now != default:
            changed[key] = now
    return {"spec": spec, "rcParams": changed}


def main():
    names = sorted(p.stem for p in STYLELIB.glob("*.mplstyle"))
    assert set(names) == set(style.library), set(style.library) ^ set(names)
    library = {}
    for name in names:
        library[name] = texts(STYLELIB / f"{name}.mplstyle")
        same_as_library(name, library[name])

    styles = {
        "_provenance": [
            "Derived from Matplotlib 3.11.2: lib/matplotlib/mpl-data/stylelib/*.mplstyle (the style sheets",
            "Matplotlib bundles, but Solarize_Light2), written by tools/oracle/make_styles.py.",
        ] + MATPLOTLIB_PROVENANCE,
        "styles": {n: s for n, s in library.items() if n != SOLARIZED},
    }
    solarized = {
        "_provenance": [
            "Derived from Solarized 1.0.0beta2 (its palette, https://ethanschoonover.com/solarized/) by way of",
            "Matplotlib 3.11.2: lib/matplotlib/mpl-data/stylelib/Solarize_Light2.mplstyle, written by",
            "tools/oracle/make_styles.py.",
            "Copyright (c) 2011 Ethan Schoonover",
            "Licence: LICENSES/Solarized.txt (MIT).",
        ] + MATPLOTLIB_PROVENANCE,
        "styles": {SOLARIZED: library[SOLARIZED]},
    }
    (ROOT / "data" / "styles.json").write_text(json.dumps(styles, indent=1, ensure_ascii=False) + "\n")
    (ROOT / "data" / "styles_solarized.json").write_text(json.dumps(solarized, indent=1, ensure_ascii=False) + "\n")

    specs = [*names, "default", "mpl20", "mpl15", "classic", [], ["ggplot", "dark_background"],
             ["dark_background", "default"], ["classic", "_classic_test_patch"], ["seaborn-v0_8", "seaborn-v0_8-talk"],
             ["default", "ggplot"], ["mpl15", "bmh"], ["ggplot"], ["ggplot", "ggplot"],
             "no-such-style", "seaborn", "Ggplot", "ggplot ", "", ".ggplot", "matplotlib.ggplot", "a.b.c",
             ["ggplot", "no-such-style"], "default.mplstyle", "ggplot.mplstyle"]
    cases = [outcome(spec) for spec in specs]
    oracle = {"styles": names, "cases": cases}
    (ROOT / "tests" / "oracle" / "styles.json").write_text(json.dumps(oracle, indent=1, ensure_ascii=False) + "\n")
    print(len(names), "styles,", len(cases), "cases,", sum("error" in c for c in cases), "refused")


if __name__ == "__main__":
    assert not list(pathlib.Path(matplotlib.get_configdir()).glob("stylelib/*")), "user styles would be listed"
    main()
