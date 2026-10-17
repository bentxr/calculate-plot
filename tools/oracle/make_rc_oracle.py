#!/usr/bin/env python3
"""Write tests/oracle/rc_validate.json: what matplotlib 3.11.2's rcParams make of a pool of values,
key by key. Each key has a row of outcomes, one per value of the pool: [] where the key's validator
refuses the value (any exception), [normal] where it takes it, "normal" being what it stores.
Keys with the same row share it.

Values and stored values are written as a figure file writes them: a tuple is {"$tuple": [...]},
a float that is not finite {"$float": "nan" | "inf" | "-inf"}, and a float always has a decimal
point or an exponent (so 1 is the integer and 1.0 the float). In stored values a tuple is a list,
an enumeration its name and a cycler {"$cycler": {property: [values]}}.

  tools/oracle/.venv/bin/python -I tools/oracle/make_rc_oracle.py
"""
import json
import math
import pathlib
import warnings

import matplotlib
from cycler import Cycler
from matplotlib import rcsetup

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")
nan, inf = float("nan"), float("inf")

POOL = [
    None, True, False,
    0, 1, 2, 3, 4, 5, 10, 11, -1, 42, 300, 700, 1000, 1001, 2 ** 70,
    0.0, 1.0, 2.0, 0.5, -0.5, -0.4999, -0.51, 1.5, 2.5, 0.999, 1.0000001, 72.0, 1e-3, 1e300, 3.7, -0.0, nan, inf, -inf,
    # what a bool may be
    "t", "T", "y", "Y", "yes", "YES", "on", "On", "true", "True", "TRUE", "1", "f", "n", "no", "off", "OFF", "false", "False",
    "0", "2", "", " ", " true", "true ", "line", "Line",
    # numbers as text
    "5", " 5 ", "\t5\n", "1.5", " 1.5 ", "1e3", "1E3", "-1", "+2", "- 1", "0x10", "1_000", "1__0", "_1", "nan", "NaN", "inf",
    "-inf", "Infinity", "infinity", "1,5", "1.5.2", "five", ".5", "5.", "1e", "0b1", "1.0", "2.0", "01", "1e400", "-0", "1 000",
    "٣", "５", "5 ", "1.5e", "e5", "++1", "0.1e-2", "700", "700.0", "1000", "1001", "-0.5", "0.3", "10", "11",
    # names that stand for None or for a special value
    "none", "None", "NONE", "nOnE", "auto", "Auto", "inherit", "Inherit", "edge", "linecolor", "mfc", "mec", "markerfacecolor",
    "markeredgecolor", "figure", "Figure", "tight", "standard", "Tight", "STANDARD",
    # colours
    "red", "Red", "RED", "r", "k", "w", "C0", "C9", "C10", "C01", "c1", "tab:blue", "TAB:BLUE", "xkcd:sky blue", "#fff", "#ffff",
    "#ff00ff", "#FF00FF", "#ff00ff80", "#ff00f", "#gggggg", "ff00ff", "ff00ff80", "FF00FF", "abcdef", "abcdefgh", "123456",
    "12345678", "1234567", "0.5", "1.5", "-0.1", "(0.1, 0.2, 0.3)", "(0.1, 0.2, 0.3, 0.4)", "[0.1, 0.2, 0.3]", "0.1, 0.2, 0.3",
    "(1, 0, 0)", "('red', 0.5)", "(0.1, 0.2)", "(0.1, 0.2, 0.3", "( 0.1 , 0.2 , 0.3 )", ("red", 0.5), ["red", 0.5],
    ("red", 1.5), ("#ff0000", 0.25), ((0.1, 0.2, 0.3), 0.5), [0.1, 0.2, 0.3], [0.1, 0.2, 0.3, 0.4], (0.1, 0.2, 0.3),
    (0.1, 0.2, 0.3, 0.4), [1, 2, 3], [1, 0, 0], [0.1, 0.2], [0.1, 0.2, 0.3, 0.4, 0.5], [0.1, 0.2, nan], [0.1, 0.2, 1.5],
    [True, False, True], "nocolor", "grey", "gray", "bgrcmyk", "rgb", "r g b", "tab10", "Pastel1", "petroff10", "",
    "orange", "indigo", "tab:cyan", "darkblue", "Orange", "'red'", "'#fff'", "C12345678901234567890", "C", "C-1", "C 1",
    ("C1", 0.5), ("red", None), ("red", True), ("red", "0.5"), ("red", nan), ("red", -0.1), ((0.1, 0.2, 0.3, 5.0), 0.5),
    ((0.1, 0.2, 5.0), 0.5), (("red", 0.5), 0.5), ("none", 0.5), ("nocolor", 0.5), ([0.1, 0.2, 0.3], 0.5), ("red", [0.5]),
    "\u0660.\u0665", "\u00e9", "#\uff10\uff10\uff10", "r\u00e9d", "(0.1, 0.2, 0.3) # c", "0x1", "(0.1, 0.2, 0x0)",
    # line styles
    "-", "--", "-.", ":", "solid", "dashed", "dashdot", "dotted", "Solid", "DASHED", "---", (0, (1, 2)), [0, [1, 2]],
    [1, 2], [1.5, 2.5], [1, 2, 3], [1, 2, 3, 4], (0, [1, 2, 3]), (0, (1, 2, 3, 4)), "(0, (1, 2))", "[1, 2]", "(1.5, (3, 1))",
    [], (), (1.5, (3, 1)), ("a", (1, 2)), [0, [1, "a"]], (0, ()), [0, []], ((1, 2), (3, 4)), [[1, 2], [3, 4]], (True, (1, 2)),
    (0, (True, 2)), (None, (1, 2)), "(0, (1, 2, 3))", "1, 2", "(1, 2",
    # markers
    "o", "s", "x", "+", "^", "$x$", ".", ",",
    # font sizes, weights and stretches
    "xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large", "smaller", "larger", "LARGE", "Large", "huge",
    "ultralight", "light", "normal", "regular", "book", "roman", "semibold", "demibold", "demi", "bold", "heavy", "extra bold",
    "black", "Bold", "abc", "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded",
    "extra-expanded", "ultra-expanded", "Condensed", 700.5, "italic", "oblique", "small-caps",
    # join and cap styles, places
    "miter", "round", "bevel", "butt", "projecting", "Miter", "ROUND",
    (0.5, 0.5), [0.5, 0.5], "(0.5, 0.5)", "[0.5, 0.5]", "upper right", "Upper Right", " upper right", (1, 2, 3), ("a", "b"),
    (1, 2), (0.5,), "(1, 2)", "(1,)", "0.5, 0.5", (True, False), (0.5, nan),
    # lists
    [1.0, 2.0], [8.0, 3.0], ["1", "2"], "1,2,3", "1, 2, 3", " 1 , 2 ", "1,", ",1", ",", "1,,2", ["a", "b"], "a, b", "a,,b",
    ["a", "", "b"], [1, "a"], [""], [None], [None, None], "DejaVu Sans, Arial", "DejaVu Sans", ["DejaVu Sans"], [-7, 7],
    (-7, 7), ["-7", "7"], "-7, 7", [1.5, 2], [1], [1, 2, 3, 4, 5, 6], [[1, 2], 3], [1, [2]], [True, False], [0, 0],
    [6.4, 4.8], [6.4], [6.4, "4.8"], [6.4, None], ["6.4", "4.8"], "6.4, 4.8", [3.7, 1.6], [6.4, 1.6, 1, 1.6],
    # fontconfig patterns
    "sans", "sans:bold", "serif:italic:bold", "DejaVu Sans:weight=bold", "sans:foo=bar", "a:b:c=", "sans-12", ":", "=",
    # sketches
    (1, 100, 2), [1, 100, 2], "1, 100, 2", "(1, 100, 2)", " (1, 100, 2) ", "(1, 100)", [1.0, 100.0, 2.0], (1.0, 100.0),
    # names of other things
    "es", "C.UTF-8", "path", "portrait", "landscape", "pdf", "svg", "png", "agg", "Agg", "module://x", "html5", "data",
    "round_numbers", "both", "major", "minor", "x", "y", "in", "out", "inout", "left", "right", "center", "top", "bottom",
    "baseline", "center_baseline", "full", "default", "no_autohint", "cm", "stix", "stixsans", "dejavusans", "custom", "STIX",
    "rm", "it", "bf", "regular", "viridis", "equal", "upper", "lower", "letter", "A4", "a4", "type3", "truetype", "Type3", 42,
    "42", "3", 41, "ghostscript", "xpdf", "toolbar2", "Toolbar2", "toolmanager", "sturges", "fd", "latex", "xelatex",
    "1970-01-01T00:00:00", "2000-01-01", "not a date", "UTC", "mpl2014", "azel", "concise", "system",
    # cyclers
    "cycler('color', ['r', 'g', 'b'])", "cycler(color='rgb')", "cycler('color', 'rgb') + cycler('lw', [1, 2, 3])",
    "cycler('color', ['C0'])", "cycler('colour', ['r'])", "cycler('color', 'rgb') * cycler('linestyle', ['-', '--'])",
    "cycler(c='rgb', color='gb')", "cycler('color', ['1f77b4', '#ff7f0e'])[:1]", "cycler(", "cycler", "color",
]


def written(v):
    """A value as a figure file writes it."""
    if isinstance(v, tuple):
        return {"$tuple": [written(x) for x in v]}
    if isinstance(v, list):
        return [written(x) for x in v]
    if isinstance(v, float) and not math.isfinite(v):
        return {"$float": "nan" if v != v else "inf" if v > 0 else "-inf"}
    return v


def stored(v):
    """What a validator returns, as the oracle keeps it."""
    if isinstance(v, Cycler):
        return {"$cycler": {k: [stored(x) for x in vs] for k, vs in v.by_key().items()}}
    if isinstance(v, (list, tuple)):
        return [stored(x) for x in v]
    if isinstance(v, (matplotlib._enums.JoinStyle, matplotlib._enums.CapStyle)):
        return v.value
    if hasattr(v, "tolist"):
        return stored(v.tolist())
    if isinstance(v, float) and not math.isfinite(v):
        return {"$float": "nan" if v != v else "inf" if v > 0 else "-inf"}
    if isinstance(v, (str, bool, int, float)) or v is None:
        return v
    raise TypeError(f"{type(v)}: {v!r}")


def copy(v):
    return [copy(x) for x in v] if isinstance(v, list) else v  # (a validator may keep or change a list)


# every choice of every key that is a choice, and its capitals
pool = list(POOL)
for validator in rcsetup._validators.values():
    if isinstance(validator, rcsetup.ValidateInStrings):
        pool += [c for c in validator.valid.values() if isinstance(c, str)]
        pool += [c.upper() for c in validator.valid.values() if isinstance(c, str)]
unique, texts = [], set()
for value in pool:
    text = json.dumps(written(value), ensure_ascii=False)
    if text not in texts:
        texts.add(text)
        unique.append(value)
pool = unique

rows, keys = [], {}
for key in sorted(rcsetup._validators):
    validator = rcsetup._validators[key]
    row = []
    for value in pool:
        try:
            row.append([stored(validator(copy(value)))])
        except Exception:  # noqa: BLE001 (whatever the validator raises, the value is not set)
            row.append([])
    text = json.dumps(row, ensure_ascii=False)
    if text not in rows:
        rows.append(text)
    keys[key] = rows.index(text)

body = ['{"pool": ' + json.dumps([written(v) for v in pool], ensure_ascii=False) + ",", '"rows": [']
body += [row + ("," if i + 1 < len(rows) else "") for i, row in enumerate(rows)]
body += ["],", '"keys": ' + json.dumps(keys, indent=0) + "}"]
(ROOT / "tests" / "oracle" / "rc_validate.json").write_text("\n".join(body) + "\n")
taken = sum(json.loads(rows[i]).count([]) for i in keys.values())
print(len(pool), "values,", len(keys), "keys,", len(rows), "different rows;", len(pool) * len(keys) - taken, "taken,", taken, "refused")
