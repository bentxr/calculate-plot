#!/usr/bin/env python3
"""Write data/fonts.json (the font list matplotlib 3.11.2's font manager builds from its bundled
fonts: name, style, variant, weight, stretch, size of each file, in the order it scores them) and
tests/oracle/findfont.json (the fonts _find_fonts_by_props picks for many font properties).

  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_fonts.py
"""
import json
import os
import pathlib
import random

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1")

import matplotlib
from matplotlib import font_manager
from matplotlib.font_manager import FontProperties, fontManager

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
BUNDLED = pathlib.Path(matplotlib.get_data_path(), "fonts", "ttf")

# Rebuilt rather than read from the cache, so that the list holds only the bundled fonts. Its order
# comes from a Python set (it changes with PYTHONHASHSEED); ties in scoring go to the earlier entry,
# but no lookup of the bundled fonts depends on it (checked with several seeds), so the file lists
# the fonts by file name.
fm = font_manager.FontManager()
entries = []
for f in sorted(fm.ttflist, key=lambda f: pathlib.Path(f.fname).name):
    path = pathlib.Path(f.fname)
    assert path.parent == BUNDLED, path
    entries.append({"file": path.name, "name": f.name, "style": f.style, "variant": f.variant,
                    "weight": f.weight, "stretch": f.stretch, "size": f.size})
(ROOT / "data" / "fonts.json").write_text(json.dumps({
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/font_manager.py (the font list FontManager",
        "builds from the bundled fonts in lib/matplotlib/mpl-data/fonts/ttf), written by",
        "tools/oracle/make_fonts.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "fonts": entries}, indent=0) + "\n")

random.seed(8)
families = ["sans-serif", "serif", "monospace", "cursive", "fantasy", "sans", "sans serif", "DejaVu Sans",
            "DejaVu Serif", "DejaVu Sans Mono", "DejaVu Sans Display", "STIXGeneral", "STIXNonUnicode",
            "STIXSizeOneSym", "cmr10", "cmtt10", "cmb10", "Last Resort High-Efficiency", "No Such Font",
            "dejavu sans", "Bitstream Vera Sans", "Arial", "Helvetica"]
styles = ["normal", "italic", "oblique"]
variants = ["normal", "small-caps"]
weights = list(font_manager.weight_dict) + [100, 200, 300, 400, 500, 600, 700, 800, 900, 450, 1000]
stretches = list(font_manager.stretch_dict) + [100, 500, 900, 650]
sizes = [None, 6.0, 10.0, 12.5, "small", "large", "xx-large"]
cases = []
for i in range(2000):
    fam = random.sample(families, random.randint(1, 3))
    style, variant = random.choice(styles), random.choice(variants)
    weight, stretch, size = random.choice(weights), random.choice(stretches), random.choice(sizes)
    prop = FontProperties(family=fam, style=style, variant=variant, weight=weight, stretch=stretch, size=size)
    found = fm._find_fonts_by_props(prop)
    names = []
    for p in found:
        p = pathlib.Path(str(getattr(p, "path", p)))
        assert p.parent == BUNDLED, p
        names.append(p.name)
    cases.append({"family": fam, "style": style, "variant": variant, "weight": weight, "stretch": stretch,
                  "size": size, "files": names})
(ROOT / "tests" / "oracle" / "findfont.json").write_text(json.dumps(cases, indent=0) + "\n")
print(len(entries), "fonts,", len(cases), "lookups")
