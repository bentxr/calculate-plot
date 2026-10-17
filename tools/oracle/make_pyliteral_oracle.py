#!/usr/bin/env python3
"""Write tests/oracle/pyliteral.json: what Python's ast.literal_eval makes of strings (the rcParams
read tuples and lists written as text with it). Of each string the file keeps "v": [value] where
the string is a literal made of None, booleans, integers, floats, strings, tuples and lists, and []
where literal_eval raises or the literal has anything else in it (bytes, a complex number, a set, a
dictionary, an Ellipsis). Tuples are {"$tuple": [...]}, floats that are not finite {"$float": ...},
and floats always have a decimal point or an exponent.

  tools/oracle/.venv/bin/python -I tools/oracle/make_pyliteral_oracle.py
"""
import ast
import json
import math
import pathlib
import random
import sys
import warnings

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert sys.version_info[:2] == (3, 14), sys.version
warnings.simplefilter("ignore")


class Other(Exception):
    pass


def written(v):
    if isinstance(v, tuple):
        return {"$tuple": [written(x) for x in v]}
    if isinstance(v, list):
        return [written(x) for x in v]
    if isinstance(v, float) and not math.isfinite(v):
        return {"$float": "nan" if v != v else "inf" if v > 0 else "-inf"}
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    raise Other


HAND = [
    "", " ", "1", " 1", "\t1", "1 ", "\n1", "1\n", "-1", "+1", "- 1", "--1", "-+1", "+-1", "-(1)", "(-1)", "-True", "-None", "-'a'",
    "0", "00", "000", "01", "0.0", "00.5", "1.", ".5", ".", "1e3", "1E3", "1e+3", "1e-3", "1e", "1e+", "e3", "1.5e3", "1.e3", ".5e3",
    "1_000", "1__0", "_1", "1_", "1_.5", "1._5", "1e1_0", "0x10", "0X10", "0o7", "0b1", "0xg", "1j", "1J", "1.5j", "1+2j", "1-2j",
    "1+2", "1-2", "1*2", "1/2", "1 2", "1e400", "-1e400", "1e-400", "123456789012345678901234567890", "9223372036854775807",
    "9223372036854775808", "-9223372036854775808", "-9223372036854775809", "0.1", "0.30000000000000004", "1.7976931348623157e308",
    "None", "True", "False", "none", "true", "NONE", "Ellipsis", "...", "x", "abc", "nan", "inf", "-inf", "float('nan')",
    "'a'", '"a"', "''", '""', "'a", "a'", "'a\"", "'a' 'b'", "'a''b'", "'a' \"b\"", "'a', 'b'", "'''a'''", '"""a"""', "''''", "'' ''",
    "'a\\n'", "'a\\'b'", "'\\\\'", "'\\x41'", "'\\u00e9'", "r'a'", "R'a'", "u'a'", "b'a'", "f'a'", "rb'a'", "br'a'", "'é'", "'a' 'é'",
    "'#'", "'a' # c", "1 # c", "#", "1#", "(1) # c", "'a\tb'", "'a b'", "' '", "'(1, 2)'", "'[1'",
    "()", "( )", "(,)", "(1)", "((1))", "(1,)", "(1, )", "( 1 , )", "(1, 2)", "(1,2)", "(1 2)", "(1,,2)", "(,1)", "(1, 2", "1, 2)",
    "1,", "1, 2", "1,2,", "1,,", ",", ",1", "1, 2, 3", "0.1, 0.2, 0.3", "'r', 0.5", "(0.1, 0.2, 0.3)", "(0.1, 0.2, 0.3, 0.4)",
    "((0.1, 0.2, 0.3), 0.5)", "('red', 0.5)", "(0, (1, 2))", "(0, [1, 2])", "[0, (1, 2)]", "(0, (1, 2, 3, 4))", "(1.5, (3, 1))",
    "[]", "[ ]", "[,]", "[1]", "[1,]", "[1, 2]", "[1 2]", "[1, [2, [3]]]", "[[]]", "[()]", "([],)", "[1, 2", "1, 2]", "[1, 2)",
    "(1, 2]", "[1]]", "((1)", "(1))", "[(1, 2), (3, 4)]", "(1, 'a', None, True, 2.5)", "[None, True, False]", "(-1, +2, -0.5)",
    "(- 1, + 2)", "(1, -(2))", "(--1,)", "{}", "{1}", "{1: 2}", "{1, 2}", "({},)", "[{}]", "(1, {2})", "b''", "(b'a',)", "(1j,)",
    "(1, ...)", "[...]", "(1,\n2)", "(1,\r\n2)", "(1, \\\n2)", "1\\", "(1,\t2)", "(1,\f2)", "( 1 )", "(\t1\t)", " (1, 2)", "\t(1, 2)",
    "(1, 2) ", "(1, 2)\t", "(1, 2)\n", "\n(1, 2)", " 1", "1 ", "(1, 2)", "１", "(１, 2)", "٣", "é", "(é,)",
    "（1, 2）", "1;", "1; 2", "a = 1", "lambda: 1", "1 if True else 2", "not True", "~1", "[1][0]", "(1).real", "int(1)", "*1", "[*a]",
    "1,2;", "`1`", "$", "?", "@", "1 @ 2", "1 < 2", "1 == 1", "1 or 2", "1 and 2", "(1 for x in y)", "[x for x in y]", "1 in (1,)",
    "0_0", "0_1", "0e0", "0E5", "0.e1", "00e1", "00.1", "01.5", "09", "09.5", "1e05", "1.0e+05", "0x", "0b2", "0o8", "1L", "1l", "0xFFj",
    "1e3j", ".5j", "1_0.0_1", "1e5_0", "(1_0,)", "'a' 1", "1 'a'", "'a' ()", "() ()", "[] []", "(1)(2)", "'a'[0]", "-'a'", "+()",
    "-[]", "(+1)", "(-1.5e3)", "+ +1", "- -1", "-\t1", "-\n1",
]
rnd = random.Random(7)
TOKENS = ["(", ")", "[", "]", ",", ",", " ", " ", "1", "2", "0", "0.5", "1.5", "-", "+", "'a'", '"b"', "''", "None", "True", "False",
          "x", "1e3", ".5", "5.", "00", "01", "1_0", "0x1", "j", "#", "\\", "{", "}", ":", "'", '"', "..", "...", "é", "\t", "\n", "=",
          "r'a'", "b'a'", "e", "_", ".", "10", "-1", "(1, 2)", "[1]", "()", "[]", "3.25", "inf", "nan"]
strings = HAND + ["".join(rnd.choice(TOKENS) for _ in range(rnd.randint(1, 8))) for _ in range(2500)]


def blank():
    return rnd.choice(["", "", "", " ", "  ", "\t"])


def literal(depth):
    """A literal as text, with blanks where Python allows them."""
    kind = rnd.random()
    if depth > 2 or kind < 0.45:
        atom = rnd.choice(["0", "1", "7", "10", "255", "0.5", "1.5", ".25", "3.", "1e3", "2.5E-3", "1e+2", "None", "True", "False",
                           "'a'", '"b c"', "''", "'r' 'ed'", "'#fff'", "0.0", "00", "12345678901"])
        sign = rnd.choice(["", "", "", "-", "+", "- ", "+\t"]) if atom[0] in "0123456789." else ""
        return sign + atom
    n = rnd.choice([0, 1, 1, 2, 2, 3, 3, 4])
    items = [literal(depth + 1) for _ in range(n)]
    body = (blank() + "," + blank()).join(items)
    trailing = rnd.random() < 0.2 and n > 0
    if kind < 0.75:
        if n == 1 and not trailing and rnd.random() < 0.7:
            trailing = True  # (a one-tuple; without the comma it is the item in parentheses)
        return "(" + blank() + body + (blank() + "," if trailing else "") + blank() + ")"
    if kind < 0.93:
        return "[" + blank() + body + (blank() + "," if trailing else "") + blank() + "]"
    return body + ("," if trailing or n == 1 else "")  # a tuple without parentheses


def mutated(s):
    if not s:
        return s
    i = rnd.randrange(len(s))
    kind = rnd.random()
    if kind < 0.35:
        return s[:i] + s[i + 1:]
    if kind < 0.7:
        return s[:i] + rnd.choice(list("()[],'\" .-+e_j01x#\\{}\n")) + s[i:]
    return s[:i] + rnd.choice(list("()[],'\" .-+e_")) + s[i + 1:]


for _ in range(2500):
    s = rnd.choice([" ", "\t", ""]) * (rnd.random() < 0.1) + literal(0) + rnd.choice([" ", "\t", ""]) * (rnd.random() < 0.1)
    strings.append(s)
    if rnd.random() < 0.6:
        strings.append(mutated(s))
strings = list(dict.fromkeys(strings))

cases = []
for s in strings:
    try:
        v = [written(ast.literal_eval(s))]
    except (Other, ValueError, TypeError, SyntaxError, MemoryError, RecursionError):
        v = []
    cases.append({"s": s, "v": v})
text = json.dumps(cases, ensure_ascii=False, separators=(",", ":")).replace('{"s"', '\n{"s"')
(ROOT / "tests" / "oracle" / "pyliteral.json").write_text(text + "\n")
print(len(cases), "strings,", sum(1 for c in cases if c["v"]), "literals")
