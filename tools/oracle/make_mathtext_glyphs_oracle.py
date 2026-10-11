#!/usr/bin/env python3
"""Write tests/oracle/mathtext_glyphs.json: the font and the glyph matplotlib 3.11.2's mathtext takes
for every symbol, in every fontset and with every font command (\\mathrm{...}, \\mathbb{...}, ...),
and for the custom fontset with each of its fallbacks. The symbols go in rows of forty; of each row
the file keeps the expression and, glyph by glyph, the font (an index into "fonts"), the character
code and the glyph index.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_mathtext_glyphs_oracle.py
"""
import json
import logging
import os
import pathlib
import re
import warnings

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1")

import matplotlib
from matplotlib._mathtext import Parser
from matplotlib._mathtext_data import tex2uni
from matplotlib.font_manager import FontProperties
from matplotlib.mathtext import MathTextParser

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
logging.disable(logging.CRITICAL)
warnings.simplefilter("ignore")

accents = set(Parser._accent_map) | set(Parser._wide_accents)
names = ["\\" + name + " " for name in sorted(tex2uni) if re.fullmatch("[A-Za-z]+", name) and name not in accents]
typed = list("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-*/<>=:,.;!?&@()[]|")
typed += [r"\%", r"\$", r"\{", r"\}", r"\_", r"\|", r"\#"]
typed += list("αβγδεζηθικλμνξοπρστυφχψωΓΔΘΛΞΠΣΦΨΩ±×÷∞∂∇∑∏∫√≤≥≠≈→←⇔⇒ℏℓℝℤé中ñßø⨁𝒜")
symbols = names + typed
ROW = 40
rows = [symbols[i:i + ROW] for i in range(0, len(symbols), ROW)]

configurations = [(fontset, {}) for fontset in ["dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"]]
configurations += [("custom", {"mathtext.fallback": fallback}) for fallback in ["stix", "stixsans", None]]
commands = [None] + sorted(Parser._fontnames)

fonts = []
cases = []
dropped = set()
for fontset, rc in configurations:
    for command in commands:
        for row in rows:
            row = [s for s in row if s not in dropped]
            while True:
                body = "".join(row)
                s = "$" + (body if command is None else "\\math%s{%s}" % (command, body)) + "$"
                try:
                    with matplotlib.rc_context(rc):
                        p = MathTextParser("path").parse(s, 100, FontProperties(size=10, math_fontfamily=fontset))
                    if len(p.glyphs) != len(row):
                        raise ValueError("the glyphs are not one for each symbol")
                    break
                except ValueError:
                    # a symbol the grammar takes for something else: find it and leave it out everywhere
                    for symbol in row:
                        one = "$" + symbol + "$"
                        try:
                            if len(MathTextParser("path").parse(one, 100, FontProperties(size=10)).glyphs) != 1:
                                raise ValueError
                        except ValueError:
                            dropped.add(symbol)
                    left = [symbol for symbol in row if symbol not in dropped]
                    assert len(left) < len(row), s
                    row = left
            glyphs = []
            for font, _size, num, index, _ox, _oy in p.glyphs:
                name = pathlib.Path(font.fname).name
                if name not in fonts:
                    fonts.append(name)
                glyphs.append([fonts.index(name), int(num), int(index)])
            case = {"fontset": fontset, "s": s, "glyphs": glyphs}
            if rc:
                case["rc"] = rc
            cases.append(case)
text = json.dumps({"fonts": fonts, "cases": cases}, ensure_ascii=False, separators=(",", ":"))
text = text.replace('{"fontset"', '\n{"fontset"')
(ROOT / "tests" / "oracle" / "mathtext_glyphs.json").write_text(text + "\n")
print(len(cases), "rows,", sum(len(c["glyphs"]) for c in cases), "glyphs,", len(fonts), "fonts, left out:", sorted(dropped))
