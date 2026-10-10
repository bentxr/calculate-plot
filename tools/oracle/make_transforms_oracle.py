#!/usr/bin/env python3
"""Write tests/oracle/transforms.json: the matrices of the transform chain an Axes builds
(figure inches -> pixels, axes position, view limits -> data, blended axis transforms) for random
figure sizes, dpis, positions and view limits, computed by matplotlib 3.11.2's own classes.

  tools/oracle/.venv/bin/python -I tools/oracle/make_transforms_oracle.py
"""
import json
import pathlib
import random

import matplotlib
from matplotlib.transforms import (Affine2D, Bbox, BboxTransformFrom, BboxTransformTo, IdentityTransform,
                                   ScaledTranslation, TransformedBbox, blended_transform_factory)

OUT = pathlib.Path(__file__).resolve().parents[2] / "tests" / "oracle" / "transforms.json"
assert matplotlib.__version__ == "3.11.2"
h = float.hex
random.seed(3)


def matrix(t):
    m = t.get_matrix()
    return [h(float(m[i, j])) for i in range(2) for j in range(3)]


cases = []
for _ in range(400):
    w, hh = random.uniform(1, 12), random.uniform(1, 9)
    dpi = random.choice([72.0, 100.0, 150.0, random.uniform(50, 300)])
    pos = [random.uniform(0, 0.3), random.uniform(0, 0.3)]
    pos += [random.uniform(0.2, 0.7), random.uniform(0.2, 0.7)]
    x0 = random.uniform(-1e3, 1e3)
    view = [x0, random.uniform(-1e3, 1e3), x0 + 10 ** random.uniform(-6, 6), 0]
    view[3] = view[1] + 10 ** random.uniform(-6, 6)
    if random.random() < 0.2:
        view[0], view[2] = view[2], view[0]  # inverted x axis
    dpi_scale = Affine2D().scale(dpi)
    bbox_inches = Bbox.from_bounds(0, 0, w, hh)
    fig_bbox = TransformedBbox(bbox_inches, dpi_scale)
    trans_figure = BboxTransformTo(fig_bbox)
    position = Bbox.from_bounds(*pos)
    ax_bbox = TransformedBbox(position, trans_figure)
    trans_axes = BboxTransformTo(ax_bbox)
    view_lim = Bbox.from_extents(*view)
    trans_scale = IdentityTransform()
    trans_limits = BboxTransformFrom(TransformedBbox(view_lim, trans_scale))
    trans_data = trans_scale + (trans_limits + trans_axes)
    xaxis = blended_transform_factory(trans_data, trans_axes)
    yaxis = blended_transform_factory(trans_axes, trans_data)
    offset = ScaledTranslation(0, -3.5 / 72, dpi_scale)
    tick_label = xaxis + offset
    point = [random.uniform(-1e3, 1e3), random.uniform(-1e3, 1e3)]
    cases.append({
        "figsize": [h(w), h(hh)], "dpi": h(dpi), "position": [h(v) for v in pos], "view": [h(v) for v in view],
        "fig_bbox": [h(float(v)) for v in fig_bbox.get_points().ravel()],
        "ax_bbox": [h(float(v)) for v in ax_bbox.get_points().ravel()],
        "transFigure": matrix(trans_figure), "transAxes": matrix(trans_axes), "transLimits": matrix(trans_limits),
        "transData": matrix(trans_data), "xaxis": matrix(xaxis), "yaxis": matrix(yaxis),
        "tickLabel": matrix(tick_label), "point": [h(v) for v in point],
        "pointData": [h(float(v)) for v in trans_data.transform(point)],
        "pointInverse": [h(float(v)) for v in trans_data.inverted().transform(point)],
    })
OUT.write_text(json.dumps(cases, separators=(",", ":")) + "\n")
print(len(cases), "cases")
