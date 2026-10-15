#!/usr/bin/env python3
"""Write matplotlib 3.11.2's built-in colour sequences (matplotlib.color_sequences, from _cm.py) as
calculate-plot reads them: each sequence's colours as RGB(A) numbers, exactly matplotlib's floats.

  data/color_sequences.json             tab10, tab20, tab20b, tab20c (Vega's categorical palettes),
                                        okabe_ito, petroff6, petroff8, petroff10 (Matplotlib's licence)
  data/color_sequences_colorbrewer.json Accent, Dark2, Paired, Pastel1, Pastel2, Set1, Set2, Set3
                                        (ColorBrewer: Apache-2.0, LICENSES/ColorBrewer.txt)

  tools/oracle/.venv/bin/python -I tools/oracle/make_color_sequences.py
"""
import json
import pathlib

import matplotlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
COLORBREWER = {"Accent", "Dark2", "Paired", "Pastel1", "Pastel2", "Set1", "Set2", "Set3"}
MATPLOTLIB_PROVENANCE = [
    "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
    "Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md",
]


def colours(name):
    out = []
    for c in matplotlib.color_sequences[name]:
        assert len(c) in (3, 4) and all(isinstance(v, float) for v in c), (name, c)
        out.append(list(c))
    return out


def main():
    names = sorted(matplotlib.color_sequences)
    assert COLORBREWER <= set(names)
    ours = {
        "_provenance": [
            "Derived from Matplotlib 3.11.2: lib/matplotlib/_cm.py and lib/matplotlib/colors.py",
            "(ColorSequenceRegistry's built-in sequences but ColorBrewer's), written by",
            "tools/oracle/make_color_sequences.py.",
        ] + MATPLOTLIB_PROVENANCE,
        "sequences": {n: colours(n) for n in names if n not in COLORBREWER},
    }
    brewer = {
        "_provenance": [
            "Derived from ColorBrewer as in Matplotlib 3.11.2: lib/matplotlib/_cm.py (Cynthia Brewer's qualitative",
            "colour schemes, https://colorbrewer2.org/), written by tools/oracle/make_color_sequences.py.",
            "Copyright (c) 2002 Cynthia Brewer, Mark Harrower, and The Pennsylvania State University.",
            "Licence: LICENSES/ColorBrewer.txt (Apache-2.0, LICENSES/Apache-2.0.txt).",
        ] + MATPLOTLIB_PROVENANCE,
        "sequences": {n: colours(n) for n in names if n in COLORBREWER},
    }
    (ROOT / "data" / "color_sequences.json").write_text(json.dumps(ours, indent=1) + "\n")
    (ROOT / "data" / "color_sequences_colorbrewer.json").write_text(json.dumps(brewer, indent=1) + "\n")
    print(len(ours["sequences"]), "+", len(brewer["sequences"]), "sequences")


if __name__ == "__main__":
    main()
