#!/usr/bin/env python3
"""Records how matplotlib's Qt canvas draws figures of the corpus on a screen: the device pixel ratio
(FigureCanvasBase._set_device_pixel_ratio: the figure's dpi becomes ratio * its original dpi), then a
canvas of W x H logical pixels (FigureCanvasQT.resizeEvent: set_size_inches(W * ratio / dpi,
H * ratio / dpi, forward=False)), then FigureCanvasAgg.draw. Writes tests/oracle/screen.json: for each
case and size, the buffer's width, height and the SHA-256 of its RGBA bytes.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_screen_oracle.py
(the corpus' fig.py scripts must exist: make_figures.py writes them)
"""
import hashlib
import json
import pathlib
import resource
import sys

resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_figures  # noqa: E402  (checks the environment, sets the Agg backend)
import numpy as np  # noqa: E402

CASES = ["corpus_legend_box", "annotate_basic", "aspect_box", "cl_basic", "layout_tight_engine", "secondary_axes",
         "corpus_subplots_grid", "scatter_basic", "dates_calls", "corpus_mathtext_labels"]
# (width, height, ratio): the mockups' canvases (desktop, phone at 2), the figure's own size, odd sizes and ratios
SIZES = [(752, 722, 1.0), (372, 673, 2.0), (640, 480, 1.0), (640, 480, 1.5), (333, 211, 1.25), (120, 90, 1.0)]


def screen(case, width, height, ratio):
    fig = make_figures.run_script(make_figures.FIGURES / case / "fig.py")
    fig._set_dpi(ratio * fig._original_dpi, forward=False)
    dpival = fig.dpi
    fig.set_size_inches(width * ratio / dpival, height * ratio / dpival, forward=False)
    fig.canvas.draw()
    rgba = np.asarray(fig.canvas.buffer_rgba())
    return {"width": int(rgba.shape[1]), "height": int(rgba.shape[0]),
            "sha256": hashlib.sha256(np.ascontiguousarray(rgba).tobytes()).hexdigest()}


def main():
    out = {"_note": "matplotlib 3.11.2 Qt canvas emulated (tools/oracle/make_screen_oracle.py)", "cases": []}
    for case in CASES:
        for width, height, ratio in SIZES:
            out["cases"].append({"case": case, "width": width, "height": height, "ratio": ratio,
                                 **{"buffer_" + k: v for k, v in screen(case, width, height, ratio).items()}})
    path = HERE.parents[1] / "tests" / "oracle" / "screen.json"
    path.write_text(json.dumps(out, indent=1) + "\n")
    print(f"{len(out['cases'])} screens -> {path}")


if __name__ == "__main__":
    main()
