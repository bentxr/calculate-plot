"""Replay a trace through matplotlib's own C++ Agg renderer.

If the pixels equal the reference, the trace holds everything the renderer depends on: this is
the check that the recorder itself is complete (make_refs.py runs it for every case).
"""
import base64

import numpy as np

from matplotlib.backends._backend_agg import RendererAgg as CppRendererAgg
from matplotlib.path import Path


def value(x):
    return float(x) if not isinstance(x, str) else float(x)  # "nan", "inf", "-inf" parse too


def floats(values):
    return np.array([value(v) for v in values], dtype=float)


def array(a, dtype=float):
    return floats(a["data"]).astype(dtype).reshape(a["shape"])


def image(a):
    return np.frombuffer(base64.b64decode(a["base64"]), dtype=np.uint8).reshape(a["shape"]).copy()


def path(p):
    if p is None:
        return None
    vertices = floats(p["vertices"]).reshape(-1, 2)
    codes = None if p["codes"] is None else np.array(p["codes"], dtype=np.uint8)
    result = Path(vertices, codes, _interpolation_steps=1)
    result.should_simplify = p["simplify"]
    result.simplify_threshold = value(p["threshold"])
    return result


def affine(t):
    if t is None:
        return None
    sx, shx, tx, shy, sy, ty = (value(v) for v in t)
    return np.array([[sx, shx, tx], [shy, sy, ty], [0.0, 0.0, 1.0]])


def color(c):
    return None if c is None else tuple(value(v) for v in c)


class GC:
    """Duck-typed graphics context with exactly the fields the C++ side reads."""

    def __init__(self, g):
        self._linewidth = value(g["linewidth"])
        self._alpha = value(g["alpha"])
        self._forced_alpha = g["forced_alpha"]
        self._rgb = color(g["rgb"])
        self._antialiased = g["antialiased"]
        self._capstyle = g["capstyle"]
        self._joinstyle = g["joinstyle"]
        offset, pattern = g["dashes"]
        self._dashes = (value(offset), None if pattern is None else [value(v) for v in pattern])
        self._cliprect = None if g["cliprect"] is None else floats(g["cliprect"]).reshape(2, 2)
        clip = g["clippath"]
        # one Path per recorded identity: Agg keeps a clip path's mask while the same path object comes again
        # (render_clippath), and an alpha mask made again under another rasterizer state can differ
        if clip is not None and clip["id"] not in _CLIP_PATHS:
            _CLIP_PATHS[clip["id"]] = path(clip["path"])
        self._clip = (None, None) if clip is None else (_CLIP_PATHS[clip["id"]], affine(clip["transform"]))
        self._snap = g["snap"]
        self._hatch = path(g["hatch"])
        self._hatch_color = color(g["hatch_color"])
        self._hatch_linewidth = value(g["hatch_linewidth"])
        self._sketch = None if g["sketch"] is None else tuple(value(v) for v in g["sketch"])

    def get_dashes(self): return self._dashes
    def get_clip_path(self): return self._clip
    def get_snap(self): return self._snap
    def get_hatch_path(self): return self._hatch
    def get_hatch_color(self): return self._hatch_color
    def get_hatch_linewidth(self): return self._hatch_linewidth
    def get_sketch_params(self): return self._sketch


_CLIP_PATHS = {}


def replay(trace):
    _CLIP_PATHS.clear()
    r = CppRendererAgg(trace["width"], trace["height"], value(trace["dpi"]))
    for e in trace["events"]:
        op = e["op"]
        if op == "clear":
            r.clear()
        elif op == "draw_path":
            r.draw_path(GC(e["gc"]), path(e["path"]), affine(e["transform"]), color(e["face"]))
        elif op == "draw_markers":
            r.draw_markers(GC(e["gc"]), path(e["marker_path"]), affine(e["marker_transform"]),
                           path(e["path"]), affine(e["transform"]), color(e["face"]))
        elif op == "draw_text_image":
            r.draw_text_image(image(e["image"]), e["x"], e["y"], value(e["angle"]), GC(e["gc"]))
        elif op == "draw_image":
            r.draw_image(GC(e["gc"]), value(e["x"]), value(e["y"]), image(e["image"]))
        elif op == "draw_path_collection":
            r.draw_path_collection(
                GC(e["gc"]), affine(e["master_transform"]), [path(p) for p in e["paths"]],
                array(e["transforms"]), array(e["offsets"]), affine(e["offset_transform"]),
                array(e["facecolors"]), array(e["edgecolors"]), array(e["linewidths"]),
                [(value(o), None if p is None else [value(v) for v in p]) for o, p in e["dashes"]],
                array(e["antialiaseds"], np.uint8), None, None, hatchcolors=array(e["hatchcolors"]))
        elif op == "draw_quad_mesh":
            r.draw_quad_mesh(GC(e["gc"]), affine(e["master_transform"]), e["mesh_width"], e["mesh_height"],
                             array(e["coordinates"]), array(e["offsets"]), affine(e["offset_transform"]),
                             array(e["facecolors"]), e["antialiased"], array(e["edgecolors"]))
        elif op == "draw_gouraud_triangles":
            r.draw_gouraud_triangles(GC(e["gc"]), array(e["points"]), array(e["colors"]), affine(e["transform"]))
        elif op in ("text", "glyphs"):
            pass  # annotations; their pixels arrive as draw_text_image
        else:
            raise ValueError(f"unknown op {op}")
    return np.asarray(r).copy()
