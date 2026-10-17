#!/usr/bin/env python3
"""Write data/schema.json: the calls calculate-plot draws, as matplotlib 3.11.2 defines them, for an
editor to offer: each call's parameters (from its signature: name, positional, default; the type its
docstring gives, and a kind) and the properties of
the artist its other keywords go to (from matplotlib's ArtistInspector: the setters, their aliases and
the valid values matplotlib documents, with a kind an editor can use: number, integer, bool, colour,
choice, text or other).

  tools/oracle/.venv/bin/python -I tools/oracle/make_schema.py
"""
import inspect
import json
import pathlib
import re
import warnings

import matplotlib

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import artist, axis, collections, legend, lines, patches, text  # noqa: E402
from matplotlib.axes import Axes  # noqa: E402
from matplotlib.figure import Figure  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")

# (where it is called, the call, the function, the artist class its other keywords set)
CALLS = [
    ("plt", "figure", plt.figure, None),
    ("plt", "subplots", plt.subplots, None),
    ("figure", "add_subplot", Figure.add_subplot, None),
    ("figure", "add_axes", Figure.add_axes, None),
    ("figure", "suptitle", Figure.suptitle, text.Text),
    ("figure", "text", Figure.text, text.Text),
    ("figure", "subplots_adjust", Figure.subplots_adjust, None),
    ("figure", "tight_layout", Figure.tight_layout, None),
    ("axes", "plot", Axes.plot, lines.Line2D),
    ("axes", "scatter", Axes.scatter, collections.PathCollection),
    ("axes", "fill_between", Axes.fill_between, collections.FillBetweenPolyCollection),
    ("axes", "fill_betweenx", Axes.fill_betweenx, collections.FillBetweenPolyCollection),
    ("axes", "errorbar", Axes.errorbar, lines.Line2D),
    ("axes", "axhline", Axes.axhline, lines.Line2D),
    ("axes", "axvline", Axes.axvline, lines.Line2D),
    ("axes", "axhspan", Axes.axhspan, patches.Rectangle),
    ("axes", "axvspan", Axes.axvspan, patches.Rectangle),
    ("axes", "bar", Axes.bar, patches.Rectangle),
    ("axes", "barh", Axes.barh, patches.Rectangle),
    ("axes", "hist", Axes.hist, patches.Rectangle),
    ("axes", "fill", Axes.fill, patches.Polygon),
    ("axes", "annotate", Axes.annotate, text.Annotation),
    ("axes", "bar_label", Axes.bar_label, text.Annotation),
    ("axes", "step", Axes.step, lines.Line2D),
    ("axes", "stairs", Axes.stairs, patches.StepPatch),
    ("axes", "stem", Axes.stem, None),
    ("axes", "boxplot", Axes.boxplot, None),
    ("axes", "violinplot", Axes.violinplot, None),
    ("axes", "pie", Axes.pie, None),
    ("axes", "hlines", Axes.hlines, collections.LineCollection),
    ("axes", "vlines", Axes.vlines, collections.LineCollection),
    ("axes", "text", Axes.text, text.Text),
    ("axes", "set_title", Axes.set_title, text.Text),
    ("axes", "set_xlabel", Axes.set_xlabel, text.Text),
    ("axes", "set_ylabel", Axes.set_ylabel, text.Text),
    ("axes", "set_xlim", Axes.set_xlim, None),
    ("axes", "set_ylim", Axes.set_ylim, None),
    ("axes", "set_xscale", Axes.set_xscale, None),
    ("axes", "set_yscale", Axes.set_yscale, None),
    ("axes", "grid", Axes.grid, lines.Line2D),
    ("axes", "legend", legend.Legend.__init__, None),
    ("axes", "loglog", Axes.loglog, lines.Line2D),
    ("axes", "semilogx", Axes.semilogx, lines.Line2D),
    ("axes", "semilogy", Axes.semilogy, lines.Line2D),
    ("axes", "tick_params", Axes.tick_params, None),
    ("axes", "twinx", Axes.twinx, None),
    ("axes", "twiny", Axes.twiny, None),
    ("axes", "inset_axes", Axes.inset_axes, None),
    ("axes", "indicate_inset", Axes.indicate_inset, patches.Rectangle),
    ("axes", "indicate_inset_zoom", Axes.indicate_inset_zoom, patches.Rectangle),
    ("axis", "set_major_formatter", axis.Axis.set_major_formatter, None),
    ("axis", "set_minor_formatter", axis.Axis.set_minor_formatter, None),
    ("axis", "set_tick_params", axis.Axis.set_tick_params, None),
]
# Properties no editor can set from a figure file (objects, callbacks, internal state)
HIDDEN = {"agg_filter", "animated", "clip_box", "clip_path", "figure", "gid", "in_layout", "mouseover", "path_effects",
          "picker", "pickradius", "rasterized", "sketch_params", "snap", "transform", "url", "data", "xdata", "ydata",
          "array", "clim", "cmap", "norm", "colorizer", "offset_transform", "offsets", "paths", "sizes", "verts",
          "verts_and_codes", "segments", "urls", "bbox", "fontproperties", "position", "x", "y", "text", "usetex",
          "parse_math", "wrap", "transform_rotates_text", "rotation_mode", "antialiased", "antialiaseds", "dashes",
          "markevery", "math_fontfamily", "data_bounds", "bounds", "xy", "width", "height", "angle"}


def kind(valid):
    v = valid.strip()
    if re.fullmatch(r"float( or None)?", v):
        return {"kind": "number"}
    if re.fullmatch(r"int( or None)?", v):
        return {"kind": "integer"}
    if v.startswith("bool"):
        return {"kind": "bool"}
    if v.startswith(":mpltype:`color`"):  # (a collection also takes one per item)
        return {"kind": "colour"}
    if v.startswith("float") or v == "array-like or float or None":
        return {"kind": "number"}
    m = re.search(r"\{([^}]*)\}", v)
    if m:
        choices = re.findall(r"'([^']*)'", m.group(1))
        if choices:
            return {"kind": "choice", "choices": choices}
    if v in ("str", "object"):
        return {"kind": "text"}
    return {"kind": "other"}


def plain(v):
    if v is inspect.Parameter.empty:
        return None
    if isinstance(v, (bool, int, float, str)) or v is None:
        return v
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return repr(v)


def documented_types(function):
    """name -> the type the numpydoc "Parameters" section gives it ("x, y : float" names two)."""
    out = {}
    for line in (inspect.getdoc(function) or "").splitlines():
        m = re.match(r"^(\w+(?:, \w+)*) : (.+)$", line.strip())
        if m:
            for name in m.group(1).split(", "):
                out.setdefault(name, m.group(2).strip())
    return out


def parameters(function):
    types = documented_types(function)
    out = []
    for p in inspect.signature(function).parameters.values():
        if p.name in ("self", "parent") or p.kind in (p.VAR_POSITIONAL, p.VAR_KEYWORD):
            continue
        entry = {"name": p.name, "positional": p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)}
        if p.name in types:
            entry["valid"] = types[p.name]
            entry.update(kind(types[p.name]))
        if p.default is not inspect.Parameter.empty:
            entry["default"] = plain(p.default)
        else:
            entry["required"] = True
        out.append(entry)
    return out


def properties(cls):
    ins = artist.ArtistInspector(cls)
    aliases = ins.aliasd
    out = []
    for name in sorted(ins.get_setters()):
        if name in HIDDEN:
            continue
        valid = ins.get_valid_values(name)
        entry = {"name": name, "valid": valid, **kind(valid)}
        if name in aliases:
            entry["aliases"] = sorted(aliases[name])
        out.append(entry)
    return out


def main():
    calls = []
    for on, name, function, cls in CALLS:
        entry = {"on": on, "call": name, "parameters": parameters(function)}
        if cls is not None:
            entry["artist"] = cls.__name__
            own = {p["name"] for p in entry["parameters"]}
            entry["properties"] = [p for p in properties(cls) if p["name"] not in own]
        calls.append(entry)
    document = {
        "_provenance": [
            "Derived from Matplotlib 3.11.2: the signatures of the calls and the setters, aliases and valid values",
            "of the artists (matplotlib.artist.ArtistInspector), written by tools/oracle/make_schema.py.",
            "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
            "Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md",
        ],
        "calls": calls,
    }
    (ROOT / "data" / "schema.json").write_text(json.dumps(document, indent=1, ensure_ascii=False) + "\n")
    n = sum(len(c["parameters"]) + len(c.get("properties", [])) for c in calls)
    print(len(calls), "calls,", n, "parameters and properties")


if __name__ == "__main__":
    main()
