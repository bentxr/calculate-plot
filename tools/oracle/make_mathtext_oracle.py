#!/usr/bin/env python3
"""Write tests/oracle/mathtext.json: matplotlib 3.11.2's mathtext layout (MathTextParser('path').parse,
the vector output both RendererAgg and TextToPath use) for expressions of the subset calculate-plot
supports so far, at several sizes and dpi, with the default fontset (dejavusans).

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_mathtext_oracle.py
"""
import json
import os
import pathlib
import random

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1")

import matplotlib
from matplotlib.font_manager import FontProperties
from matplotlib.mathtext import MathTextParser

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


fixed = [r"$\mathdefault{10^{%d}}$" % n for n in range(-12, 13)]
fixed += [r"$\mathdefault{%s\times10^{%d}}$" % (m, n) for m in ("2", "5", "-3") for n in (-2, 0, 3)]
fixed += [r"$x^2$", r"$a_i$", r"$x_{ij}^{2}$", r"$10^{-3}$", r"$E=mc^2$", r"$a+b$", r"$-1$", r"$2.5$",
          r"$a\cdot b$", r"$f(x)$", r"x = $y^2$ m", r"$x$", r"$\mathdefault{x^{2}_{0}}$", r"$v_{max}$",
          r"$1-2$", r"$(a+b)^2$", r"$2^{2^{2}}$", r"$x_1^2+y_1^2$", r"$k=3$", r"$\mathdefault{-}$",
          # whitespace: skipped between tokens, but a space right after ^ or _ is the script
          r"$a b$", r"$a  b$", r"$a\cdot b$", r"$ a$", r"$a $", r"$x^ 2$", r"$x ^2$", r"$\mathdefault{ 1}$",
          r"$1 0$", r"$a+ b$", r"$x_ {i}$", r"$ - 1$", r"$\mathdefault{10^{ -2}}$"]
rnd = random.Random(4)
atoms = list("0123456789") + list("abxyzEFTk") + [r"\mathdefault{12}", "{a}", "(x)"]
ops = ["+", "-", "=", r"\times", r"\cdot", ""]


def expr(depth=0):
    parts = []
    for _ in range(rnd.randint(1, 3)):
        a = rnd.choice(atoms)
        if parts and parts[-1].startswith("\\"):  # a command name runs on through letters
            a = rnd.choice("0123456789")
        if depth < 2 and rnd.random() < 0.4:
            a += rnd.choice(["^", "_"]) + "{" + expr(depth + 1) + "}"
        parts.append(a)
        parts.append(rnd.choice(ops))
    return "".join(parts[:-1])


generated = ["$" + expr() + "$" for _ in range(200)]
cases = []
parser = MathTextParser("path")
for i, s in enumerate(fixed + generated):
    size = rnd.choice([10.0, 10.0, 12.0, 7.0, 14.4, 100.0])
    dpi = rnd.choice([72.0, 100.0, 100.0, 150.0])
    prop = FontProperties(size=size)
    p = parser.parse(s, dpi, prop)
    cases.append({"s": s, "size": size, "dpi": dpi,
                  "width": h(p.width), "height": h(p.height), "depth": h(p.depth),
                  "glyphs": [{"font": pathlib.Path(f.fname).name, "size": h(fs), "num": int(num), "index": int(gi),
                              "ox": h(ox), "oy": h(oy)} for f, fs, num, gi, ox, oy in p.glyphs],
                  "rects": [[h(v) for v in r] for r in p.rects]})
(ROOT / "tests" / "oracle" / "mathtext.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "expressions")
