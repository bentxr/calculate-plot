#!/usr/bin/env python3
"""Records the text matplotlib's navigation toolbar shows for the pointer (NavigationToolbar2.mouse_move:
_mouse_event_to_message, which is the Axes' format_coord of the event's data coordinates) over a grid of
positions on figures of the corpus, on an emulated Qt canvas as make_navigation_oracle.py. Writes
tests/oracle/coordinates.json: for each case, canvas and position (logical pixels from the top-left), the text
("" outside every Axes).

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_coordinates_oracle.py
"""
import json
import pathlib
import resource
import sys

resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_navigation_oracle as nav  # noqa: E402
from matplotlib.backend_bases import MouseEvent  # noqa: E402

CASES = ["corpus_legend_box", "corpus_subplots_grid", "twin_axes", "log_both", "scale_symlog", "scale_logit",
         "aspect_box", "dates_shared", "share_y_log", "axis_null_formatter", "boxplot_basic", "rc_unicode_minus_false",
         "scale_function", "inset_axes", "secondary_axes", "polar_basic", "polar_settings", "polar_wedges"]
CANVASES = [(640, 480, 1.0), (500, 420, 2.0)]


def main():
    out = {"_note": "matplotlib 3.11.2 NavigationToolbar2._mouse_event_to_message (tools/oracle/make_coordinates_oracle.py)",
           "cases": []}
    for case in CASES:
        for width, height, ratio in CANVASES:
            fig, canvas, toolbar = nav.setup(case, width, height, ratio)
            points = [(round(x * width / 16 + 3.3, 2), round(y * height / 12 + 1.7, 2)) for y in range(12) for x in range(16)]
            texts = []
            for p in points:
                event = nav.event(canvas, "motion_notify_event", p, None, None)
                texts.append(toolbar._mouse_event_to_message(event))
            out["cases"].append({"case": case, "width": width, "height": height, "ratio": ratio,
                                 "points": points, "texts": texts})
    path = HERE.parents[1] / "tests" / "oracle" / "coordinates.json"
    path.write_text(json.dumps(out, indent=0, ensure_ascii=False) + "\n")
    print(sum(len(c["texts"]) for c in out["cases"]), "positions,",
          sum(t == "" for c in out["cases"] for t in c["texts"]), "outside ->", path)


if __name__ == "__main__":
    main()
