#!/usr/bin/env python3
"""Write tests/oracle/lambda.json: the functions a figure file writes as {"$lambda": "<body>"} evaluated as
`lambda x: <body>` with x a float64 array, as NumPy 2.5.3 computes them (float.hex), and the bodies Python
refuses to evaluate (the name of the error it raises).

  tools/oracle/.venv/bin/python -I tools/oracle/make_lambda_oracle.py
"""
import json
import pathlib
import resource

import numpy as np

resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
ROOT = pathlib.Path(__file__).resolve().parents[2]
assert np.__version__ == "2.5.3"


def h(v):
    return float.hex(float(v))


rng = np.random.default_rng(1018)
ARRAYS = [
    [0.0, -0.0, 1.0, -1.0, 2.0, 0.5, -0.5, 10.0, 100.0, -273.15, 1e-300, 1e300, float("inf"), float("-inf"), float("nan")],
    [float(v) for v in rng.uniform(-10, 10, 40)],
    [float(v) for v in 10 ** rng.uniform(-30, 30, 30)],
    [float(v) for v in rng.uniform(-1, 1, 30)],
    [float(v) for v in rng.uniform(0, 1e4, 20)],
    # a non-finite first value (masked first); the edges of np.ma's arctanh domain (-1 + 1e-15, 1 - 1e-15)
    [float("nan"), float("inf"), -1 + 1e-15, -1 + 5e-16, -0.9999999999999999, 1 - 1e-15, 1 - 5e-16,
     0.9999999999999999, 1.0000000000000002, -1.0000000000000002, 2.0, -2.0],
]
TAKEN = [
    "x", "x * 1.8 + 32", "(x - 32) / 1.8", "-x", "+x", "- -x", "x * 2", "2 * x", "x / 3", "3 / x", "1 / x", "x - 1",
    "1 - x", "x + 0.1 + 0.2", "x + (0.1 + 0.2)", "0.1 * 3 * x", "x * 0.1 * 3", "x ** 2", "x**2.0", "x ** -1", "x ** -1.0",
    "x ** 0.5", "x ** (1 / 2)", "x ** (4 ** 0.5)", "x ** np.sqrt(4)", "x ** 3", "x ** 1.5", "x ** -2", "x ** 0", "x ** 1",
    "x ** (2 - 1)", "x ** (3 / 2)", "2 ** x", "10 ** x", "0.5 ** x", "np.e ** x", "x ** x", "-x ** 2", "(-x) ** 2",
    "2 ** -x", "2 ** 3 ** x", "x ** 2 ** 0.5", "2 ** 3 * x", "2 ** -1 * x", "(-2) ** 3 * x", "(-8) ** 2 * x", "7 / 2 * x",
    "x * (1 / 3)", "x * 10 ** 15", "x * 1e15", "x * 2 ** 52", "x * (1 + 2 * 3 - 4 / 5)", "x * 0.1 ** 2", "x * 1.5 ** -2",
    "x * 0 ** 0", "x * 0.0 ** 3", "x * (-0.0) ** 3", "x * (-1) ** 2 ** 50", "x * 1 ** -5", "x * (-1.0) ** 7.0",
    "np.sqrt(x)", "np.exp(x)", "np.exp2(x)", "np.expm1(x)", "np.log(x)", "np.log2(x)", "np.log10(x)", "np.log1p(x)",
    "np.cbrt(x)", "np.sin(x)", "np.cos(x)", "np.tan(x)", "np.arcsin(x)", "np.arccos(x)", "np.arctan(x)", "np.sinh(x)",
    "np.cosh(x)", "np.arcsinh(x)", "np.arctanh(x)", "np.abs(x)", "np.deg2rad(x)", "np.rad2deg(x)", "np.radians(x)",
    "np.degrees(x)", "x * np.pi / 180", "np.pi * x", "x / np.e", "np.sqrt(2) * x", "x * np.sqrt(2) ** 2",
    "np.sqrt(2) ** 2 * x", "x + np.log10(1000)", "x * np.exp(1) / np.e", "np.sqrt(x ** 2 + 1)", "1 / np.sqrt(x)",
    "np.log10(np.abs(x) + 1)", "np.sin(np.deg2rad(x)) * 2", "x * np.cos(0)", "x / np.sin(0)", "np.sqrt(-1) + x",
    "np.log(0) * x", "x * (np.sqrt(4) / 0)", "np.arctanh(1) + x", "x / 0", "x / 0.0", "x * np.tan(np.pi / 4)",
    "1e-320 * x", "x * 1e308 * 10", "x ** 0.25", "x ** -0.5", "(x + 1) ** 2", "(x ** 2) ** 0.5", "x**2*3-x/2+1",
    "  x  *  2  ", "x*.5", "x*5.", "x*1E3", "x*1e-3", "x*1.5e+2", "x * 007.5", "x * 0", "x * 00",
    "x * 10.0 ** -400", "x * 10.0 ** -310", "x * 2 ** -1074", "x * 2 ** -1100", "x * 2 ** 53", "x * -2 ** 53",
    "x + 9007199254740992", "x * 3.0 ** 1.0", "x + 0.0 ** 0.0", "x * (-2.0) ** 3.0", "x * (-2.0) ** 2.0",
    "x * (-1) ** 3", "x * (-1) ** 5", "x * (-1) ** 4", "(x)**2", "(-x)**3", "(x+1)**0.5",
]
ERRORS = ["x * (1 / 0)", "x * (1.0 / 0)", "x * 0 ** -1", "x * 0.0 ** -2", "x * 10.0 ** 400", "x * 2.0 ** 1100",
          "x * (1 // 0)"]
# Python evaluates these, calculate-plot does not (yet)
UNSUPPORTED = ["x * 2 ** 60", "x * 9007199254740993", "x * (-8) ** (1 / 3)", "x // 2", "x % 2", "np.tanh(x)",
               "np.floor(x)", "abs(x)", "x if x else 1", "x * 1_000", "x * 1j", "x * 0x10", "x * np.inf",
               "x * 1e400", "y", "2", "np.pi", "math.sqrt(x)", "x[0]", "np.sqrt(x, 2)", "x * (2", "x *", "lambda x: x",
               "x ** 2 ** 60", "x * 012", "x + 4294967296 * 4294967296",
               "x * (9007199254740992 + 1)"]

cases = []
for body in TAKEN:
    f = eval("lambda x: " + body, {"np": np})
    results, masked = [], []
    for arr in ARRAYS:
        with np.errstate(all="ignore"):
            r = f(np.array(arr, dtype=np.float64))
            # as a scale's FuncTransform meets scatter's offsets: a masked array (its non-finite values masked),
            # filled with NaN afterwards
            m = np.ma.masked_invalid(np.array(arr, dtype=np.float64))
            rm = np.ma.filled(np.ma.asarray(f(m)), np.nan)
        assert isinstance(r, np.ndarray) and r.dtype == np.float64, body
        results.append([h(v) for v in r])
        masked.append([h(v) for v in rm])
    cases.append({"body": body, "results": results, "masked": masked})
errors = []
for body in ERRORS:
    try:
        with np.errstate(all="ignore"):
            eval("lambda x: " + body, {"np": np})(np.array([1.0]))
    except (ArithmeticError, SyntaxError, ValueError) as e:
        errors.append({"body": body, "error": type(e).__name__})
    else:
        raise SystemExit(body + " did not raise")
doc = {"_note": "NumPy 2.5.3, tools/oracle/make_lambda_oracle.py", "arrays": [[h(v) for v in a] for a in ARRAYS],
       "taken": cases, "errors": errors, "unsupported": UNSUPPORTED}
(ROOT / "tests" / "oracle" / "lambda.json").write_text(json.dumps(doc) + "\n")
print(len(cases), "functions,", len(errors), "errors,", len(UNSUPPORTED), "not supported")
