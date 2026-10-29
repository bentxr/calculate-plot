#!/usr/bin/env python3
"""Records matplotlib's interactive navigation on figures of the corpus, as its Qt canvas and
NavigationToolbar2 do it: the canvas at W x H logical pixels and a device pixel ratio (as
make_screen_oracle.py), mouse positions in logical pixels from the widget's top-left turned into event
coordinates as FigureCanvasQT.mouseEventCoords does (x * ratio, (get_width_height()[1] - y) * ratio), then
the toolbar's press_pan / drag_pan / release_pan or press_zoom / release_zoom. Writes
tests/oracle/navigation.json: for each gesture, the limits of every Axes after it (float.hex) and the
SHA-256 of the RGBA buffer drawn after it, or "ignored" when the toolbar did nothing.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_navigation_oracle.py
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
from matplotlib.backend_bases import MouseButton, MouseEvent, NavigationToolbar2  # noqa: E402

CASES = ["corpus_legend_box", "corpus_subplots_grid", "twin_axes", "log_both", "scale_symlog", "scale_logit",
         "aspect_box", "inset_axes", "secondary_axes", "dates_shared", "share_y_log", "polar_basic", "polar_wedges"]
CANVASES = [(640, 480, 1.0), (500, 420, 2.0)]


def setup(case, width, height, ratio):
    fig = make_figures.run_script(make_figures.FIGURES / case / "fig.py")
    canvas = fig.canvas
    canvas._set_device_pixel_ratio(ratio)
    fig.set_size_inches(width * ratio / fig.dpi, height * ratio / fig.dpi, forward=False)
    canvas.draw()
    return fig, canvas, NavigationToolbar2(canvas)


def event(canvas, name, point, button, key, buttons=None):
    ratio = canvas.device_pixel_ratio
    x = point[0] * ratio
    y = (canvas.get_width_height()[1] - point[1]) * ratio
    return MouseEvent(name, canvas, x, y, button=button, key=key, buttons=buttons)


def state(fig):
    canvas = fig.canvas
    canvas.draw()
    rgba = np.ascontiguousarray(np.asarray(canvas.buffer_rgba()))
    out = {"limits": [[float(v).hex() for v in (*ax.get_xlim(), *ax.get_ylim())] for ax in fig.get_axes()],
           "sha256": hashlib.sha256(rgba.tobytes()).hexdigest()}
    # (a polar Axes' radial labels can move without a pixel changing: on a wedge they stay at its start)
    rlabels = [float(ax.get_rlabel_position()).hex() for ax in fig.get_axes() if ax.name == "polar"]
    if rlabels:
        out["rlabels"] = rlabels
    return out


def polar_gestures(k, ax, height, ratio):
    """On a polar Axes: the radial labels dragged (button 1 on their angle), the radius scaled (button 3), a press
    away from the labels and the zoom box, which do nothing."""
    def at(theta, r):
        x, y = ax.transData.transform((theta, r))
        return (round(x / ratio, 2), round(height - y / ratio, 2))
    label = np.deg2rad(ax.get_rlabel_position())
    rmax = ax.get_rmax()
    lo, hi = ax.get_rmin(), rmax
    mid = lo + 0.6 * (hi - lo)
    off = ax.get_xlim()[0] + 0.5 * (ax.get_xlim()[1] - ax.get_xlim()[0])
    return [
        {"axes": k, "tool": "pan", "button": 1, "key": None, "from": at(label, mid), "to": at(label + 0.5, mid)},
        {"axes": k, "tool": "pan", "button": 1, "key": "x", "from": at(label, mid), "to": at(label - 0.3, lo + 0.3 * (hi - lo))},
        {"axes": k, "tool": "pan", "button": 1, "key": None, "from": at(off, mid), "to": at(off + 0.4, mid)},
        {"axes": k, "tool": "pan", "button": 3, "key": None, "from": at(off, mid), "to": at(off, lo + 0.9 * (hi - lo))},
        {"axes": k, "tool": "pan", "button": 3, "key": "control", "from": at(off, mid), "to": at(off + 0.2, lo + 0.35 * (hi - lo))},
        {"axes": k, "tool": "zoom", "button": 1, "key": None, "from": at(off, mid), "to": at(off + 0.3, lo + 0.9 * (hi - lo))},
    ]


def gestures(fig, height, ratio):
    """Gestures around each Axes' centre, in logical pixels from the top-left."""
    out = []
    for k, ax in enumerate(fig.get_axes()):
        if ax.name == "polar":
            out += polar_gestures(k, ax, height, ratio)
            continue
        x0, y0, x1, y1 = ax.bbox.extents
        cx, cy = (x0 + x1) / 2 / ratio, height - (y0 + y1) / 2 / ratio
        c = (round(cx, 2), round(cy, 2))
        for key in (None, "x", "y", "shift", "control"):
            out.append({"axes": k, "tool": "pan", "button": 1, "key": key, "from": c, "to": (c[0] + 37.5, c[1] - 21.25)})
        out.append({"axes": k, "tool": "pan", "button": 1, "key": "shift", "from": c, "to": (c[0] + 5, c[1] + 40)})
        out.append({"axes": k, "tool": "pan", "button": 3, "key": None, "from": c, "to": (c[0] + 30, c[1] + 20)})
        out.append({"axes": k, "tool": "pan", "button": 3, "key": "control", "from": c, "to": (c[0] - 50, c[1] + 10)})
        out.append({"axes": k, "tool": "zoom", "button": 1, "key": None, "from": (c[0] - 30, c[1] - 20), "to": (c[0] + 25, c[1] + 15)})
        out.append({"axes": k, "tool": "zoom", "button": 3, "key": None, "from": (c[0] - 30, c[1] - 20), "to": (c[0] + 25, c[1] + 15)})
        out.append({"axes": k, "tool": "zoom", "button": 1, "key": "x", "from": (c[0] - 30, c[1] - 2), "to": (c[0] + 25, c[1] + 1)})
        out.append({"axes": k, "tool": "zoom", "button": 1, "key": "y", "from": (c[0] - 2, c[1] - 20), "to": (c[0] + 1, c[1] + 15)})
        out.append({"axes": k, "tool": "zoom", "button": 1, "key": None, "from": c, "to": (c[0] + 3, c[1] + 30)})  # too small
        out.append({"axes": k, "tool": "zoom", "button": 1, "key": None, "from": c, "to": (c[0] + 4000, c[1] - 4000)})
    return out


def run(case, width, height, ratio, g):
    fig, canvas, toolbar = setup(case, width, height, ratio)
    button = MouseButton(g["button"])
    press = event(canvas, "button_press_event", g["from"], button, g["key"])
    release = event(canvas, "button_release_event", g["to"], button, g["key"])
    before = state(fig)
    if g["tool"] == "pan":
        toolbar.pan()
        toolbar.press_pan(press)
        if toolbar._pan_info is None:
            return None
        toolbar.drag_pan(event(canvas, "motion_notify_event", g["to"], None, g["key"], buttons={button}))
        toolbar.release_pan(release)
    else:
        toolbar.zoom()
        toolbar.press_zoom(press)
        if toolbar._zoom_info is None:
            return None
        toolbar.release_zoom(release)
    after = state(fig)
    return None if after == before else after


def main():
    out = {"_note": "matplotlib 3.11.2 NavigationToolbar2 on an emulated Qt canvas (tools/oracle/make_navigation_oracle.py)",
           "cases": []}
    for case in CASES:
        for width, height, ratio in CANVASES:
            fig, _, _ = setup(case, width, height, ratio)
            for g in gestures(fig, height, ratio):
                result = run(case, width, height, ratio, g)
                out["cases"].append({"case": case, "width": width, "height": height, "ratio": ratio, **g,
                                     **({"ignored": True} if result is None else result)})
    path = HERE.parents[1] / "tests" / "oracle" / "navigation.json"
    path.write_text(json.dumps(out, indent=0) + "\n")
    print(f"{len(out['cases'])} gestures, {sum('ignored' in c for c in out['cases'])} ignored -> {path}")


if __name__ == "__main__":
    main()
