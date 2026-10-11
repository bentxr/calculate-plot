#!/usr/bin/env python3
"""Write tests/oracle/fontconfig.json: what matplotlib 3.11.2 makes of fontconfig patterns (the
values of rcParams["mathtext.rm"] and the like): whether parse_fontconfig_pattern takes the pattern
("invalid" otherwise, as the rcParams refuse it), the FontProperties it gives ("broken" with the
error where FontProperties(pattern) raises), and the font findfont picks for them among the bundled
fonts.

  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_fontconfig_oracle.py
"""
import json
import logging
import os
import pathlib
import random
import warnings

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1")

import matplotlib
from matplotlib import font_manager
from matplotlib._fontconfig_pattern import _CONSTANTS, parse_fontconfig_pattern
from matplotlib.font_manager import FontProperties, findfont

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
logging.disable(logging.CRITICAL)
warnings.simplefilter("ignore")

patterns = [
    # what the fontsets and the default rcParams use
    "sans", "sans:italic", "sans:bold", "sans:italic:bold", "cursive", "monospace", "serif", "DejaVu Sans",
    "DejaVu Sans:italic", "DejaVu Sans:weight=bold", "DejaVu Sans:italic:bold", "DejaVu Sans Display", "DejaVu Serif:italic",
    "DejaVu Sans Mono", "cmex10", "cmr10", "cmmi10", "STIXGeneral", "STIXGeneral:italic", "STIXGeneral:weight=bold",
    "STIXGeneral:italic:bold", "STIXNonUnicode", "STIXNonUnicode:italic", "STIXNonUnicode:weight=bold", "STIXSizeOneSym",
    "STIXSizeTwoSym", "STIXSizeThreeSym", "STIXSizeFourSym", "STIXSizeFiveSym",
    # the family: empty, several, escaped characters, blanks
    "", " ", ":bold", "serif,sans", "no such font,serif", "serif, sans", " serif", "serif ", "  DejaVu  Serif  ", "sans-serif",
    r"sans\-serif", r"a\:b", r"a\,b", r"a\\b", "a\\b", "a\\", r"\-", "serif,", ",serif", ",", "a,b,c:bold", "Serif", "SANS",
    "sans\tserif", "sans\n", "\nsans", "é", "中文字体", "sans;bold", "sans=bold", "sans_serif", "sans.serif", "sans/serif",
    # sizes
    "sans-12", "sans-12.5", "sans-.5", "sans-12.", "sans-12,14", "sans-12:bold", "-12", "sans-", "sans-12-14", "sans-a",
    "sans-1e3", "sans-0", "sans-0.5", "sans - 12", "sans-12 ,14", "sans-12, 14", "sans--12", "sans-12pt", "serif-100:italic",
    # the constants
    *["sans:" + name for name in sorted(_CONSTANTS)],
    "sans:bold:italic", "sans:italic:oblique", "sans:oblique:italic", "sans:bold:light", "sans:light:bold", "sans:roman:italic",
    "sans:Bold", "sans:boldx", "sans:xbold", "sans:bold ", "sans: bold", "sans :bold", "sans:bold:", "sans::bold", "sans:",
    "sans:semibold", "sans:extrabold", "sans:extra bold", "sans:demi", "sans:heavy:black", "serif:condensed:bold",
    "sans:bolditalic", "sans:italicbold", "sans:normal", "sans:regular:roman",
    # name=value
    "sans:weight=bold", "sans:weight=700", "sans:weight=7.5", "sans:weight=heavyish", "sans:weight=bold,light",
    "sans:weight=light,bold", "sans:weight=bold:weight=light", "sans:weight=", "sans:weight", "sans:weight=1001",
    "sans:weight= bold", "sans:weight=bold ", "sans:weight =bold", "sans:weight=extra bold", "sans:weight=-1", "sans:weight=0",
    "sans:style=italic", "sans:style=oblique", "sans:style=slanted", "sans:slant=italic", "sans:slant=upright",
    "sans:style=italic:slant=oblique", "sans:slant=oblique:style=italic", "sans:italic:style=normal", "sans:style=normal:italic",
    "sans:variant=small-caps", r"sans:variant=small\-caps", "sans:variant=normal", "sans:variant=caps",
    "sans:stretch=condensed", "sans:stretch=500", "sans:stretch=wide", r"sans:stretch=semi\-condensed",
    "sans:stretch=ultra-condensed", "sans:size=12", "sans:size=large", "sans:size=huge", "sans:size=0.5", "sans-10:size=20",
    "sans:size=20,10", "sans:family=serif", "sans:name=serif", ":family=serif", ":name=serif", "sans:file=x.ttf",
    "sans:file=", "sans:width=condensed", "sans:foo=bar", "sans:math=cm", "sans:math_fontfamily=cm", "sans:fontconfig=x",
    "sans:Weight=bold", "sans:weight2=bold", "sans:=bold", "sans:weight==bold", "sans:weight=a=b", r"sans:weight=a\=b",
    r"sans:weight=bo\ld", r"sans:weight=bold\:x", "sans:weight=bold_x", r"sans:weight=bold\_x", r"sans:weight=bold\,light",
    "sans:weight=bold,", "sans:weight=,bold", "sans:weight=bold,,light", "sans:size=", "sans:size=1e1", "sans:size= 12",
    "serif:italic:weight=bold:size=14", "monospace:bold:slant=oblique", "cursive:weight=light:stretch=condensed",
    "DejaVu Serif:style=italic:weight=bold", "STIXGeneral:weight=bold:italic", "cmr10:bold", "cmb10", "cmss10:italic",
    "DejaVu Sans Mono:bold:oblique", "serif:weight=book", "serif:weight=roman", "serif:weight=demi", "serif:weight=black",
    "serif:book", "serif:medium:italic", "sans:thin", "sans:ultralight:italic",
]
rnd = random.Random(12)
pieces = ["sans", "serif", "DejaVu Serif", "monospace", "cmr10", "STIXGeneral", "", "x", " ", ",", ":", "-", "12", "1.5", "bold",
          "italic", "light", "oblique", "condensed", "weight=", "style=", "size=", "slant=", "stretch=", "variant=", "file=",
          "700", "small-caps", "\\", "=", "_", "a"]
patterns += ["".join(rnd.choice(pieces) for _ in range(rnd.randint(1, 6))) for _ in range(600)]
patterns = list(dict.fromkeys(patterns))

cases = []
for pattern in patterns:
    case = {"pattern": pattern}
    try:
        parse_fontconfig_pattern(pattern)
    except ValueError:
        case["invalid"] = True
        cases.append(case)
        continue
    try:
        prop = FontProperties(pattern)
    except (AttributeError, ValueError, TypeError) as err:
        case["broken"] = type(err).__name__ + ": " + str(err)
        cases.append(case)
        continue
    case["family"] = list(prop.get_family())
    case["style"] = prop.get_style()
    case["variant"] = prop.get_variant()
    case["weight"] = prop.get_weight()
    case["stretch"] = prop.get_stretch()
    case["size"] = float.hex(float(prop.get_size()))
    case["file"] = prop.get_file()
    if prop.get_file() is None:
        case["font"] = pathlib.Path(findfont(prop)).name
    cases.append(case)
(ROOT / "tests" / "oracle" / "fontconfig.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "patterns,", sum("invalid" in c for c in cases), "invalid,", sum("broken" in c for c in cases), "broken,",
      sum(c.get("file") is not None for c in cases), "with a file")
