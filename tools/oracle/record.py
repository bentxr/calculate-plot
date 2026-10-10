"""Record what matplotlib's Agg renderer is asked to draw.

A figure is drawn by matplotlib 3.11.2 as usual, but the C++ renderer object inside RendererAgg
is wrapped in a proxy that writes down every call, with every argument exactly as matplotlib's
C++ side receives it, before forwarding it. The result is a *trace*: replaying it through
calculate-plot's renderer must give the same pixels as matplotlib gave (tests/corpus).

Text is recorded twice: the glyph bitmaps reach the renderer as draw_text_image calls, and the
"text" and "glyphs" events beside them say which characters, fonts and positions produced them,
so the text pipeline can be checked on its own.

Numbers are written with Python's repr, which round-trips every double exactly; non-finite
numbers as the strings "nan", "inf" and "-inf"; byte images as base64.
"""
import base64
import json
import math

import numpy as np

import matplotlib
from matplotlib.backends.backend_agg import FigureCanvasAgg, RendererAgg
from matplotlib.font_manager import fontManager

TRACE_VERSION = 1


def number(x):
    x = float(x)
    if math.isfinite(x):
        return x
    return "nan" if math.isnan(x) else ("inf" if x > 0 else "-inf")


def numbers(values):
    return [number(v) for v in np.asarray(values, dtype=float).ravel()]


def array(values, dtype=float):
    a = np.asarray(values, dtype=dtype)
    return {"shape": list(a.shape), "data": numbers(a)}


def image(values):
    a = np.ascontiguousarray(values, dtype=np.uint8)
    return {"shape": list(a.shape), "base64": base64.b64encode(a.tobytes()).decode("ascii")}


def color(rgba):
    return None if rgba is None else numbers(rgba)


def affine(transform):
    """A transform as the C++ side reads it: the first two rows of its 3x3 matrix, or None."""
    if transform is None:
        return None
    m = np.asarray(transform, dtype=float)
    return numbers([m[0, 0], m[0, 1], m[0, 2], m[1, 0], m[1, 1], m[1, 2]])


def path(p):
    if p is None:
        return None
    codes = p.codes
    return {"vertices": numbers(p.vertices), "codes": None if codes is None else [int(c) for c in codes],
            "simplify": bool(p.should_simplify), "threshold": number(p.simplify_threshold)}


def dashes(offset_and_pattern):
    offset, pattern = offset_and_pattern
    return [number(offset), None if pattern is None else numbers(pattern)]


def bbox(b):
    return None if b is None else numbers(np.asarray(b, dtype=float))


def gc(g):
    """The graphics context, field by field as matplotlib's C++ GCAgg reads it."""
    clip_path, clip_trans = g.get_clip_path()
    return {
        "linewidth": number(g._linewidth),
        "alpha": number(g._alpha),
        "forced_alpha": bool(g._forced_alpha),
        "rgb": color(g._rgb),
        "antialiased": bool(g._antialiased),
        "capstyle": g._capstyle.name,
        "joinstyle": g._joinstyle.name,
        "dashes": dashes(g.get_dashes()),
        "cliprect": bbox(g._cliprect),
        "clippath": None if clip_path is None else {"path": path(clip_path), "transform": affine(clip_trans)},
        "snap": g.get_snap(),
        "hatch": path(g.get_hatch_path()),
        "hatch_color": color(g.get_hatch_color()),
        "hatch_linewidth": number(g.get_hatch_linewidth()),
        "sketch": None if g.get_sketch_params() is None else numbers(g.get_sketch_params()),
    }


class RecordingRenderer:
    """Stands in for the C++ _RendererAgg object: records each call, then forwards it."""

    def __init__(self, inner, events):
        self._inner = inner
        self._events = events

    def __getattr__(self, name):  # buffer access, copy_from_bbox, ...
        return getattr(self._inner, name)

    def __buffer__(self, flags):
        return memoryview(self._inner)

    def draw_path(self, g, p, transform, rgb_face=None):
        self._events.append({"op": "draw_path", "gc": gc(g), "path": path(p),
                             "transform": affine(transform), "face": color(rgb_face)})
        return self._inner.draw_path(g, p, transform, rgb_face)

    def draw_markers(self, g, marker_path, marker_trans, p, trans, rgb_face=None):
        self._events.append({"op": "draw_markers", "gc": gc(g), "marker_path": path(marker_path),
                             "marker_transform": affine(marker_trans), "path": path(p),
                             "transform": affine(trans), "face": color(rgb_face)})
        return self._inner.draw_markers(g, marker_path, marker_trans, p, trans, rgb_face)

    def draw_text_image(self, img, x, y, angle, g):
        self._events.append({"op": "draw_text_image", "image": image(img), "x": int(x), "y": int(y),
                             "angle": number(angle), "gc": gc(g)})
        return self._inner.draw_text_image(img, x, y, angle, g)

    def draw_image(self, g, x, y, img):
        self._events.append({"op": "draw_image", "gc": gc(g), "x": number(x), "y": number(y),
                             "image": image(img)})
        return self._inner.draw_image(g, x, y, img)

    def draw_path_collection(self, g, master_transform, paths, all_transforms, offsets, offset_trans,
                             facecolors, edgecolors, linewidths, linestyles, antialiaseds, urls,
                             offset_position, *, hatchcolors=None):
        # hatchcolors is keyword-only, as in matplotlib's binding: matplotlib probes for it and
        # takes another drawing path when the probe raises TypeError.
        paths = list(paths)
        if hatchcolors is None:
            hatchcolors = np.empty((0, 4))
        self._events.append({
            "op": "draw_path_collection", "gc": gc(g), "master_transform": affine(master_transform),
            "paths": [path(p) for p in paths],
            "transforms": array(np.asarray(all_transforms, float).reshape(-1, 3, 3) if len(all_transforms) else np.empty((0, 3, 3))),
            "offsets": array(np.asarray(offsets, float).reshape(-1, 2) if np.size(offsets) else np.empty((0, 2))),
            "offset_transform": affine(offset_trans),
            "facecolors": array(np.asarray(facecolors, float).reshape(-1, 4) if np.size(facecolors) else np.empty((0, 4))),
            "edgecolors": array(np.asarray(edgecolors, float).reshape(-1, 4) if np.size(edgecolors) else np.empty((0, 4))),
            "linewidths": array(np.asarray(linewidths, float).reshape(-1)),
            "dashes": [dashes(d) for d in linestyles],
            "antialiaseds": array(np.asarray(antialiaseds, np.uint8).reshape(-1)),
            "hatchcolors": array(np.asarray(hatchcolors, float).reshape(-1, 4) if np.size(hatchcolors) else np.empty((0, 4))),
        })
        return self._inner.draw_path_collection(g, master_transform, paths, all_transforms, offsets,
                                                offset_trans, facecolors, edgecolors, linewidths,
                                                linestyles, antialiaseds, urls, offset_position,
                                                hatchcolors=hatchcolors)

    def draw_quad_mesh(self, g, master_transform, mesh_width, mesh_height, coordinates, offsets,
                       offset_trans, facecolors, antialiased, edgecolors):
        self._events.append({
            "op": "draw_quad_mesh", "gc": gc(g), "master_transform": affine(master_transform),
            "mesh_width": int(mesh_width), "mesh_height": int(mesh_height),
            "coordinates": array(coordinates),
            "offsets": array(np.asarray(offsets, float).reshape(-1, 2) if np.size(offsets) else np.empty((0, 2))),
            "offset_transform": affine(offset_trans),
            "facecolors": array(np.asarray(facecolors, float).reshape(-1, 4) if np.size(facecolors) else np.empty((0, 4))),
            "antialiased": bool(antialiased),
            "edgecolors": array(np.asarray(edgecolors, float).reshape(-1, 4) if np.size(edgecolors) else np.empty((0, 4))),
        })
        return self._inner.draw_quad_mesh(g, master_transform, mesh_width, mesh_height, coordinates,
                                          offsets, offset_trans, facecolors, antialiased, edgecolors)

    def draw_gouraud_triangles(self, g, points, colors, transform):
        self._events.append({"op": "draw_gouraud_triangles", "gc": gc(g), "points": array(points),
                             "colors": array(colors), "transform": affine(transform)})
        return self._inner.draw_gouraud_triangles(g, points, colors, transform)

    def clear(self):
        self._events.append({"op": "clear"})
        return self._inner.clear()

    def restore_region(self, *args):
        raise NotImplementedError("restore_region is not recorded yet")


MPL_FONTS = __import__("pathlib").Path(matplotlib.get_data_path(), "fonts", "ttf")


def font_name(path):
    """A font file as calculate-plot finds it: its name in fonts/ttf (matplotlib's bundled fonts)."""
    path = __import__("pathlib").Path(str(getattr(path, "path", path)))
    if path.parent != MPL_FONTS:
        raise ValueError(f"{path} is not one of matplotlib's bundled fonts (set MPL_IGNORE_SYSTEM_FONTS=1)")
    return path.name


def font_paths(prop):
    return [font_name(f) for f in fontManager._find_fonts_by_props(prop)]


def record(fig):
    """Draws `fig` with Agg; returns (trace dict, RGBA numpy array of the result)."""
    events = []
    canvas = FigureCanvasAgg(fig)
    original_get_renderer = canvas.get_renderer

    def get_renderer():
        renderer = original_get_renderer()
        if not isinstance(renderer._renderer, RecordingRenderer):
            renderer._renderer = RecordingRenderer(renderer._renderer, events)
            renderer._update_methods()
        return renderer

    canvas.get_renderer = get_renderer

    original_text = RendererAgg.draw_text
    original_glyphs = RendererAgg._draw_text_glyphs_and_boxes

    def draw_text(self, g, x, y, s, prop, angle, ismath=False, mtext=None):
        events.append({"op": "text", "s": s, "x": number(x), "y": number(y), "angle": number(angle),
                       "ismath": ismath if isinstance(ismath, str) else bool(ismath),
                       "fonts": font_paths(prop), "size": number(prop.get_size_in_points()),
                       "features": None if mtext is None or mtext.get_fontfeatures() is None
                       else list(mtext.get_fontfeatures()),
                       "language": None if mtext is None else mtext.get_language(),
                       "antialiased": bool(g.get_antialiased())})
        return original_text(self, g, x, y, s, prop, angle, ismath, mtext)

    def draw_text_glyphs_and_boxes(self, g, x, y, angle, glyphs, boxes):
        glyphs, boxes = list(glyphs), list(boxes)
        events.append({"op": "glyphs", "x": number(x), "y": number(y), "angle": number(angle),
                       "dpi": number(self.dpi), "height": number(self.height),
                       "glyphs": [{"font": font_name(font.fname), "size": number(size), "index": int(index),
                                   "slant": number(slant), "extend": number(extend),
                                   "dx": number(dx), "dy": number(dy)}
                                  for font, size, index, slant, extend, dx, dy in glyphs],
                       "boxes": [numbers(b) for b in boxes]})
        return original_glyphs(self, g, x, y, angle, glyphs, boxes)

    RendererAgg.draw_text = draw_text
    RendererAgg._draw_text_glyphs_and_boxes = draw_text_glyphs_and_boxes
    try:
        canvas.draw()
    finally:
        RendererAgg.draw_text = original_text
        RendererAgg._draw_text_glyphs_and_boxes = original_glyphs
    renderer = canvas.get_renderer()
    rgba = np.asarray(canvas.buffer_rgba()).copy()
    trace = {"version": TRACE_VERSION, "matplotlib": matplotlib.__version__,
             "width": int(renderer.width), "height": int(renderer.height), "dpi": number(renderer.dpi),
             "events": events}
    return trace, rgba
