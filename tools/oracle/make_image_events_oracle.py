#!/usr/bin/env python3
"""Write tests/oracle/image_events.json: the draw_image events matplotlib 3.11.2 sends to RendererAgg and RendererSVG
for figure images (fig.figimage) of a few shapes, alphas and origins, as tools/oracle/record.py records them.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_image_events_oracle.py
"""
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import matplotlib  # noqa: E402

matplotlib.use("agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

import record  # noqa: E402

assert matplotlib.__version__ == "3.11.2"
rng = np.random.default_rng(12)
cases = []
for k, (h, w, xo, yo, alpha, origin) in enumerate([(3, 5, 7, 11, None, None), (4, 2, 0, 0, 0.5, "upper"),
                                                   (1, 1, 30, 2, None, "lower"), (6, 9, 15.5, 3.25, None, None)]):
    plt.close("all")
    fig = plt.figure(figsize=(1.0, 0.8), dpi=50)
    rgba = rng.integers(0, 256, (h, w, 4)).astype(np.uint8)
    kwargs = {"xo": xo, "yo": yo, "resize": False}
    if alpha is not None:
        kwargs["alpha"] = alpha
    if origin is not None:
        kwargs["origin"] = origin
    fig.figimage(rgba, **kwargs)
    trace, _ = record.record(fig)
    svg_trace, _ = record.record_svg(fig, {"Creator": "calculate-plot", "Date": None})
    cases.append({
        "image": rgba.tolist(), "kwargs": kwargs,
        "agg": [e for e in trace["events"] if e["op"] == "draw_image"],
        "svg": [e for e in svg_trace["events"] if e["op"] == "draw_image"]})
(HERE.parents[1] / "tests" / "oracle" / "image_events.json").write_text(json.dumps(cases, indent=1) + "\n")
print(len(cases), "cases")
