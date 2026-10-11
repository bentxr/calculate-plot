#!/usr/bin/env python3
"""Regenerate the figure corpus: tests/figures/<case>/fig.json is a figure file (written by hand);
this writes beside it fig.py (exported by calculate-plot's `cplot export-py`, so the exporter is
tested too), then runs that script with matplotlib 3.11.2 and records trace.json, ref.png,
svg-trace.json and ref.svg, as make_refs.py does for the renderer corpus.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_figures.py \\
      build/native/cplot [case ...]
"""
import hashlib
import json
import os
import pathlib
import runpy
import subprocess
import sys

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1" or os.environ.get("LC_ALL") != "C.UTF-8":
    sys.exit("set MPL_IGNORE_SYSTEM_FONTS=1 and LC_ALL=C.UTF-8")

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import matplotlib  # noqa: E402

matplotlib.use("agg")
import matplotlib.pyplot as plt
from matplotlib.path import Path  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import record  # noqa: E402
import replay  # noqa: E402

FIGURES = HERE.parents[1] / "tests" / "figures"
SVG_METADATA = {"Creator": "calculate-plot", "Date": None}


def run_script(path):
    """Runs an exported script as a module (not __main__, so it does not show) and returns its figure."""
    plt.close("all")
    plt.rcdefaults()
    # The unit paths that Path makes on first use keep the simplification settings of the rcParams
    # of that moment: a script that runs alone makes them with its own.
    Path._unit_rectangle = Path._unit_circle = Path._unit_circle_righthalf = None
    Path._unit_regular_polygons.clear()
    Path._unit_regular_stars.clear()
    # to_rgba keeps what it has converted by (colour, alpha), and -0.0 finds what 0.0 left there
    matplotlib.colors._colors_full_map.cache.clear()
    namespace = runpy.run_path(str(path), run_name="calculate_plot_oracle")
    fig = namespace.get("fig") or plt.gcf()
    return fig


def main(cplot, names):
    assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
    for case in sorted(FIGURES.iterdir()) if not names else [FIGURES / n for n in names]:
        model = case / "fig.json"
        script = case / "fig.py"
        subprocess.run([cplot, "export-py", str(model), str(script)], check=True)

        fig = run_script(script)
        trace, rgba = record.record(fig)
        again = replay.replay(json.loads(json.dumps(trace)))
        if not np.array_equal(again, rgba):
            sys.exit(f"{case.name}: replaying the trace does not reproduce the figure")
        (case / "trace.json").write_text(json.dumps(trace, separators=(",", ":")) + "\n")
        Image.fromarray(rgba, "RGBA").save(case / "ref.png", optimize=True)
        (case / "ref.json").write_text(json.dumps({
            "width": int(rgba.shape[1]), "height": int(rgba.shape[0]),
            "rgba_sha256": hashlib.sha256(rgba.tobytes()).hexdigest()}, indent=1) + "\n")

        fig = run_script(script)
        matplotlib.rcParams["svg.hashsalt"] = "calculate-plot"
        svg_trace, svg = record.record_svg(fig, SVG_METADATA)
        (case / "svg-trace.json").write_text(json.dumps(svg_trace, separators=(",", ":")) + "\n")
        (case / "ref.svg").write_text(svg)
        print(f"{case.name}: {len(trace['events'])} events, {rgba.shape[1]}x{rgba.shape[0]}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
