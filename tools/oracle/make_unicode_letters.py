#!/usr/bin/env python3
"""Write data/unicode_letters.json: which characters of the Basic Multilingual Plane are letters
(general category L*), which have a name beginning with GREEK CAPITAL, and which characters are
digits for str.isdigit, from the Unicode Character Database as Python's unicodedata has it in the
reference environment.

matplotlib's mathtext sets a letter in italics and anything else, and Greek capitals, upright; it
asks unicodedata.

  tools/oracle/.venv/bin/python -I tools/oracle/make_unicode_letters.py
"""
import json
import pathlib
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parents[2]


def ranges(predicate, limit=0x10000):
    out, start = [], None
    for c in range(limit + 1):
        inside = c < limit and predicate(chr(c))
        if inside and start is None:
            start = c
        if not inside and start is not None:
            out.append([start, c - 1])
            start = None
    return out


data = {
    "_provenance": [
        f"Derived from Unicode Character Database {unicodedata.unidata_version} (general categories, character names and",
        "numeric types), as read through Python's unicodedata by tools/oracle/make_unicode_letters.py.",
        "Copyright (C) Unicode, Inc. All rights reserved.",
        "Licence: LICENSES/Unicode-3.0.txt.",
    ],
    "unidata_version": unicodedata.unidata_version,
    "python": sys.version.split()[0],
    "letters": ranges(lambda ch: unicodedata.category(ch)[0] == "L"),
    "greek_capital": ranges(lambda ch: unicodedata.name(ch, "").startswith("GREEK CAPITAL")),
    "digits": ranges(str.isdigit, 0x110000),
}
(ROOT / "data" / "unicode_letters.json").write_text(json.dumps(data, separators=(",", ":")).replace('"letters"', '\n"letters"').replace('"greek_capital"', '\n"greek_capital"').replace('"digits"', '\n"digits"') + "\n")
print(data["unidata_version"], len(data["letters"]), "letter ranges,", len(data["greek_capital"]), "Greek capital ranges")
