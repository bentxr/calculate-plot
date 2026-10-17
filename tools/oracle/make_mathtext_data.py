#!/usr/bin/env python3
"""Write data/mathtext_data.json: the tables matplotlib 3.11.2's mathtext reads besides tex2uni: the
parser's sets of symbols (binary operators, relations, arrows, punctuation, delimiters, over/under
symbols, function names, font names, accents, space widths) and the font tables of _mathtext_data
(the Computer Modern positions of TeX symbols, the virtual fonts of STIX and its glyph fixes).

  tools/oracle/.venv/bin/python -I tools/oracle/make_mathtext_data.py
"""
import json
import pathlib

import matplotlib
from matplotlib import _mathtext, _mathtext_data

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
P = _mathtext.Parser


def virtual(fonts):
    """stix_virtual_fonts: font -> ranges, or font -> class -> ranges; a range is (first, last, font, to)."""
    out = {}
    for name, mapping in fonts.items():
        if isinstance(mapping, dict):
            out[name] = {cls: [list(r) for r in ranges] for cls, ranges in mapping.items()}
        else:
            out[name] = [list(r) for r in mapping]
    return out


data = {
    "_provenance": [
        "Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (the sets of the Parser class) and",
        "lib/matplotlib/_mathtext_data.py (latex_to_bakoma, stix_virtual_fonts, stix_glyph_fixes), written by",
        "tools/oracle/make_mathtext_data.py.",
        "Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved",
        "Licence: LICENSES/Matplotlib.txt.",
    ],
    "binary_operators": sorted(P._binary_operators),
    "relation_symbols": sorted(P._relation_symbols),
    "arrow_symbols": sorted(P._arrow_symbols),
    "punctuation_symbols": sorted(P._punctuation_symbols),
    "overunder_symbols": sorted(P._overunder_symbols),
    "overunder_functions": sorted(P._overunder_functions),
    "dropsub_symbols": sorted(P._dropsub_symbols),
    "fontnames": sorted(P._fontnames),
    "function_names": sorted(P._function_names),
    "ambi_delims": sorted(P._ambi_delims),
    "left_delims": sorted(P._left_delims),
    "right_delims": sorted(P._right_delims),
    "small_greek": sorted(P._small_greek),
    "space_widths": dict(sorted(P._space_widths.items())),
    "accent_map": dict(sorted(P._accent_map.items())),
    "wide_accents": sorted(P._wide_accents),
    "latex_to_bakoma": {sym: [font, int(num)] for sym, (font, num) in sorted(_mathtext_data.latex_to_bakoma.items())},
    "stix_virtual_fonts": virtual(_mathtext_data.stix_virtual_fonts),
    "stix_glyph_fixes": {str(k): int(v) for k, v in sorted(_mathtext_data.stix_glyph_fixes.items())},
}
(ROOT / "data" / "mathtext_data.json").write_text(json.dumps(data, indent=0, ensure_ascii=True) + "\n")
print({k: len(v) for k, v in data.items() if k != "_provenance"})
