#!/usr/bin/env python3
"""Write tests/oracle/pyparse.json: what Python's float() and int() make of strings (the hexadecimal
form of the float, the integer as text, or null where they raise ValueError).

  tools/oracle/.venv/bin/python -I tools/oracle/make_pyparse_oracle.py
"""
import json
import pathlib
import random

ROOT = pathlib.Path(__file__).resolve().parents[2]

texts = ["0", "1", "12", "-12", "+12", "12.5", "12.", ".5", "-.5", "+.5", ".", "", " ", "1e3", "1E3", "1e+3", "1e-3", "1e", "e3",
         "1.e3", ".5e1", "1e3.5", "1_000", "1__000", "_1", "1_", "1_.5", "1._5", "1.5_5", "1e1_0", "1e_10", "1_e10", "0x10",
         "0b1", "0o7", "inf", "-inf", "+inf", "Inf", "INF", "infinity", "Infinity", "-INFINITY", "infinit", "in", "nan", "NaN",
         "-nan", "+nan", "nana", " 12 ", "\t12\n", "\x0c12\x0b", "12 3", "1 2", "- 12", "--12", "+-12", "12-", "12+", "1,5",
         "1.5.5", "1e5e5", "007", "-007", "0.0", "-0.0", "-0", "+0", "1e400", "-1e400", "1e-400", "12abc", "abc", "0.1",
         "123456789012345678901234567890", "1.7976931348623157e308", "1.7976931348623159e308", "4.9e-324", "2.4e-324",
         "0.30000000000000004", "9007199254740993", "1_0.0_1e1_0", "١٢", "１２", "1٫5", "²", "½", "1e١", " 12 ",
         "​12", "12　", "1 000", "1'000", "1e 3", "1 e3", ". 5", "5 .", "0_0", "0_", "00", "+", "-", "++1", "1.0f", "1d0",
         "1.0e", "1.0e+", "1.0e-", "1.0E5", "nan(1)", "infinity ", " -Infinity", "1_2_3", "1_2_", "9" * 400, "0." + "0" * 400 + "1"]
rnd = random.Random(21)
alphabet = "0123456789" * 3 + "+-._eE " + "infa_x"
texts += ["".join(rnd.choice(alphabet) for _ in range(rnd.randint(1, 8))) for _ in range(1500)]
texts = list(dict.fromkeys(texts))


def attempt(function, text):
    try:
        return function(text)
    except ValueError:
        return None


cases = []
for text in texts:
    f, i = attempt(float, text), attempt(int, text)
    cases.append({"text": text, "float": None if f is None else float.hex(f), "int": None if i is None else str(i)})
(ROOT / "tests" / "oracle" / "pyparse.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "strings,", sum(c["float"] is not None for c in cases), "floats,", sum(c["int"] is not None for c in cases), "integers")
