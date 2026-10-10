#!/usr/bin/env python3
"""Write tests/oracle/text_layout.json: how matplotlib 3.11.2 lays out and draws Text artists (random
strings, fonts, sizes, alignments, rotations, rotation modes, line spacings) with the Agg renderer at
several dpi and with the SVG renderer: Text._get_layout's box and lines, the window extent, and the
draw_text calls (position, string, angle).

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_text_oracle.py
"""
import io
import json
import os
import pathlib
import random

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1" or os.environ.get("LC_ALL") != "C.UTF-8":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1 and LC_ALL=C.UTF-8")

import matplotlib

matplotlib.use("agg")
from matplotlib.backends.backend_agg import FigureCanvasAgg  # noqa: E402
from matplotlib.backends.backend_svg import RendererSVG  # noqa: E402
from matplotlib.figure import Figure  # noqa: E402
from matplotlib.text import Text  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


STRINGS = ["0.0", "lp", "Title", "x", "", "A\nB", "first line\nsecond", "a\n\nb", "  spaced  ", "café",
           "−0.5", "1e+05", "cost \\$5", "Wy", "gjpqy", "MMMM\ni", "long label text", "3\n33\n333",
           "fi fl ffi", "Δt (s)"]
HAS = ["left", "center", "right"]
VAS = ["top", "bottom", "center", "baseline", "center_baseline"]
ROTATIONS = [0, 0, 90, 45, -30, 270, 180, 33.3, 359.5, "vertical", "horizontal", None, 725]
MODES = [None, "default", "anchor", "xtick", "ytick"]
SPACINGS = [None, None, 1.2, 2.0, 0.8]
MALIGNS = [None, "left", "center", "right"]
SIZES = [6, 10, 10, 12, 17.28, 7.5, "large", "small"]
FAMILIES = [None, None, "serif", "monospace", "STIXGeneral", "DejaVu Sans Display", "cmr10"]
WEIGHTS = [None, "bold", 600]
STYLES = [None, "italic", "oblique"]
LANGUAGES = [None, None, None, "en", "tr", "es-ES"]


def make_case(rnd):
    props = {"text": rnd.choice(STRINGS), "x": round(rnd.uniform(0, 300), rnd.choice([0, 1, 3])),
             "y": round(rnd.uniform(0, 200), rnd.choice([0, 1, 3])), "ha": rnd.choice(HAS), "va": rnd.choice(VAS),
             "rotation": rnd.choice(ROTATIONS) if rnd.random() < 0.7 else round(rnd.uniform(-400, 400), rnd.choice([1, 3, 6])),
             "rotation_mode": rnd.choice(MODES), "language": rnd.choice(LANGUAGES),
             "linespacing": rnd.choice(SPACINGS), "multialignment": rnd.choice(MALIGNS),
             "fontsize": rnd.choice(SIZES), "family": rnd.choice(FAMILIES), "weight": rnd.choice(WEIGHTS),
             "style": rnd.choice(STYLES)}
    return props


def text_for(fig, p):
    kw = {"horizontalalignment": p["ha"], "verticalalignment": p["va"], "rotation": p["rotation"],
          "rotation_mode": p["rotation_mode"], "linespacing": p["linespacing"],
          "multialignment": p["multialignment"], "fontsize": p["fontsize"]}
    for k in ("family", "weight", "style"):
        if p[k] is not None:
            kw[k] = p[k]
    if p["language"] is not None:
        kw["language"] = p["language"]
    t = Text(p["x"], p["y"], p["text"], **kw)
    t.set_figure(fig)
    return t


def record(t, renderer):
    calls = []
    original = renderer.draw_text

    def draw_text(gc, x, y, s, prop, angle, ismath=False, mtext=None):
        calls.append({"x": h(x), "y": h(y), "s": s, "angle": h(angle), "ismath": bool(ismath),
                      "mtext": mtext is not None, "language": None if mtext is None else mtext.get_language(),
                      "size": h(prop.get_size_in_points())})

    renderer.draw_text = draw_text
    try:
        out = {}
        if t.get_text():
            bbox, info, (corner, size) = t._get_layout(renderer)
            out["bbox"] = [h(v) for v in bbox.bounds]
            out["lines"] = [{"s": line, "wad": [h(v) for v in wad], "xy": [h(v) for v in xy]}
                            for line, wad, xy in info]
            out["corner"] = [h(v) for v in corner] + [h(v) for v in size]
        out["extent"] = [h(v) for v in t.get_window_extent(renderer).bounds]
        t.draw(renderer)
        out["draw"] = calls
        return out
    finally:
        renderer.draw_text = original


rnd = random.Random(11)
cases = []
for i in range(600):
    p = make_case(rnd)
    figsize = (rnd.choice([3.2, 4.0, 6.4]), rnd.choice([2.4, 3.0, 4.8]))
    if i % 4 == 3:  # the SVG renderer, at 72 dpi, measuring with TextToPath
        fig = Figure(figsize=figsize, dpi=rnd.choice([72, 100]))
        fig.dpi = 72
        w, hgt = figsize[0] * 72, figsize[1] * 72
        renderer = RendererSVG(w, hgt, io.StringIO(), image_dpi=72)
        backend = "svg"
    else:
        dpi = rnd.choice([72, 100, 100, 150, 200])
        fig = Figure(figsize=figsize, dpi=dpi)
        renderer = FigureCanvasAgg(fig).get_renderer()
        backend = "agg"
    t = text_for(fig, p)
    case = {"backend": backend, "figsize": list(figsize), "dpi": fig.dpi, "props": p,
            "fonts": [pathlib.Path(str(getattr(f, "path", f))).name
                      for f in matplotlib.font_manager.fontManager._find_fonts_by_props(t.get_fontproperties())],
            "rotation": h(t.get_rotation())}
    case.update(record(t, renderer))
    cases.append(case)
(ROOT / "tests" / "oracle" / "text_layout.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "texts")
