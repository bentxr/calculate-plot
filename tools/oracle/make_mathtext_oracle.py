#!/usr/bin/env python3
"""Write tests/oracle/mathtext.json: matplotlib 3.11.2's mathtext layout (MathTextParser('path').parse,
the vector output both RendererAgg and TextToPath use) for expressions over the whole grammar, at
several sizes and dpi, with every fontset and with the settings that reach mathtext; "error" is what
matplotlib says where it refuses an expression or breaks on it.

  LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_mathtext_oracle.py
"""
import json
import os
import pathlib
import random

if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
    raise SystemExit("set MPL_IGNORE_SYSTEM_FONTS=1")

import matplotlib
from matplotlib.font_manager import FontProperties
from matplotlib.mathtext import MathTextParser

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"


def h(x):
    return float.hex(float(x))


fixed = [r"$\mathdefault{10^{%d}}$" % n for n in range(-12, 13)]
fixed += [r"$\mathdefault{%s\times10^{%d}}$" % (m, n) for m in ("2", "5", "-3") for n in (-2, 0, 3)]
fixed += [r"$x^2$", r"$a_i$", r"$x_{ij}^{2}$", r"$10^{-3}$", r"$E=mc^2$", r"$a+b$", r"$-1$", r"$2.5$",
          r"$a\cdot b$", r"$f(x)$", r"x = $y^2$ m", r"$x$", r"$\mathdefault{x^{2}_{0}}$", r"$v_{max}$",
          r"$1-2$", r"$(a+b)^2$", r"$2^{2^{2}}$", r"$x_1^2+y_1^2$", r"$k=3$", r"$\mathdefault{-}$",
          # whitespace: skipped between tokens, but a space right after ^ or _ is the script
          r"$a b$", r"$a  b$", r"$a\cdot b$", r"$ a$", r"$a $", r"$x^ 2$", r"$x ^2$", r"$\mathdefault{ 1}$",
          r"$1 0$", r"$a+ b$", r"$x_ {i}$", r"$ - 1$", r"$\mathdefault{10^{ -2}}$"]
rnd = random.Random(4)
atoms = list("0123456789") + list("abxyzEFTk") + [r"\mathdefault{12}", "{a}", "(x)"]
ops = ["+", "-", "=", r"\times", r"\cdot", ""]


def expr(depth=0):
    parts = []
    for _ in range(rnd.randint(1, 3)):
        a = rnd.choice(atoms)
        if parts and parts[-1].startswith("\\"):  # a command name runs on through letters
            a = rnd.choice("0123456789")
        if depth < 2 and rnd.random() < 0.4:
            a += rnd.choice(["^", "_"]) + "{" + expr(depth + 1) + "}"
        parts.append(a)
        parts.append(rnd.choice(ops))
    return "".join(parts[:-1])


generated = ["$" + expr() + "$" for _ in range(200)]

# --- symbols: every TeX name, the spacing of operators, relations and punctuation, primes --------
from matplotlib._mathtext import Parser  # noqa: E402
from matplotlib._mathtext_data import tex2uni  # noqa: E402

symbols = []
symbols += ["$\\%s$" % name for name in sorted(tex2uni) if name not in "#$%"]  # each alone ...
symbols += ["$a\\%s b$" % name for name in sorted(tex2uni) if name not in "#$%"]  # ... and between letters
symbols += [r"$\#$", r"$\$$", r"$\%$", r"$\{x\}$", r"$\[x\]$", r"$\_$", r"$\|x\|$", r"$|x|$"]
symbols += ["$%s$" % c for c in "abcxyzABCXYZ0123456789+-*/<>=:,.;!?&@()[]|"]
symbols += ["$a%sb$" % c for c in "+-*/<>=:,.;!?&@|"]
# binary operators: no space at the start, after an opening delimiter or after a relation
symbols += [r"$-x$", r"$+x$", r"$\pm x$", r"$(-x)$", r"$[-x]$", r"$\{-x\}$", r"$=-x$", r"$a=-x$", r"$a\leq-x$",
            r"$a - -x$", r"${-x}$", r"$a^{-x}$", r"$a_{+}$", r"$ - x$", r"$a\to-b$", r"$\langle-x\rangle$",
            r"$a\times-b$", r"$\mathdefault{-}1$", r"$2\times 10^{-3}$", r"$a<b>c$", r"$a\in A$", r"$x\rightarrow\infty$"]
# punctuation: a space after it, but not in {,} nor for a decimal point
symbols += [r"$1,000$", r"$1{,}000$", r"$1.5$", r"$a.b$", r"$.5$", r"$5.$", r"$a,b;c!d$", r"$x, y$", r"$1 . 5$",
            r"$\ldotp a\cdotp b$", r"$f(x,y)$", r"$a:b$"]
# primes and apostrophes
symbols += [r"$x'$", r"$x''$", r"$x'''$", r"$f'(x)$", r"$x'^2$", r"$x_i'$", r"$x^2'$", r"$'$", r"$''$", r"$x^\prime$",
            r"$\prime$", r"$a'b'$", r"$\alpha'$", r"${x}'$", r"$x_1^2'$", r"$\mathrm{d}x'$"]
# characters typed directly, with and without a glyph in DejaVu Sans
symbols += ["$αβΓ$", "$± 1$", "$∞$", "$x²$", "$∂ f$", "$5°$", "$é$",
            "$中$", "$⨁$", "$\U0001d49c$", "$a≤b$", "$ℏℓ$"]
# every letter and digit in every font
letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
symbols += ["$\\math%s{%s}$" % (font, letters[i:i + 16]) for font in sorted(Parser._fontnames)
            for i in range(0, len(letters), 16)]
symbols += ["$\\math%s{\\alpha\\beta\\Gamma\\Omega\\partial\\infty+-=()}$" % font for font in sorted(Parser._fontnames)]
symbols += [r"$\rm a \it b \bf c \tt d \sf e \cal F$", r"${\rm abc}def$", r"$a{\bf b}c$"]


# --- constructs: fractions, roots, accents, functions, delimiters, stacks, spaces ------------------
constructs = [
    # spaces
    r"$a\,b\;c\:d\>e\ f\quad g\qquad h\!i\/j\enspace k\thinspace l~m$", r"$a\hspace{0.5}b\hspace{-0.2}c\hspace{2}d$",
    r"$\,$", r"$a \quad$", r"$\hspace{1.5}$",
    # fractions
    r"$\frac{1}{2}$", r"$\frac{a+b}{c}$", r"$\dfrac{1}{2}$", r"$\frac{\frac{1}{2}}{3}$", r"$x=\frac{-b\pm\sqrt{b^2-4ac}}{2a}$",
    r"$\binom{n}{k}$", r"$\genfrac{(}{)}{0}{1}{a}{b}$", r"$\genfrac{}{}{1.5}{0}{a}{b}$", r"$\genfrac{[}{]}{0.5}{2}{x^2}{y_1}$",
    r"$\genfrac{}{}{0}{3}{a}{b}$", r"$\frac{1}{2}x^{\frac{3}{4}}$", r"$\frac{dy}{dx}$", r"$\tfrac{1}{2}$",
    r"$\frac{1}{ 2 }$", r"$\frac {1} {2}$", r"$1+\frac{1}{1+\frac{1}{x}}$", r"$\dfrac{\partial f}{\partial x}$",
    # roots and rules
    r"$\sqrt{2}$", r"$\sqrt{x+y}$", r"$\sqrt[3]{x}$", r"$\sqrt[n]{\frac{a}{b}}$", r"$\sqrt{\sqrt{x}}$", r"$\sqrt{x^2+\frac{1}{4}}$",
    r"$\overline{x}$", r"$\overline{abc}+\underline{def}$", r"$\underline{x_1}$", r"$\overline{\frac{1}{2}}$", r"$\sqrt{}$",
    r"$\sqrt[3] {x}$", r"$\sqrt {x}$",
    # accents
    r"$\hat{x}$", r"$\bar{y}$", r"$\vec{v}$", r"$\tilde{a}$", r"$\dot{x}\ddot{y}\dddot{z}\ddddot{w}$", r"$\acute{e}\grave{a}\breve{u}$",
    r"$\widehat{abc}$", r"$\widetilde{xyz}$", r"$\widebar{AB}$", r"$\overrightarrow{AB}$", r"$\overleftarrow{CD}$", r"$\mathring{A}$",
    r"$\hat x$", r"$\hat{x}^2$", r"$\hat{\imath}$", r"$\vec{F}=m\vec{a}$", r"$\bar{x}_i$", r"$\tilde{\alpha}$", r"$\hat{\hat{x}}$",
    r'$\"u \`a \'e \~n \.z \^o$', r"$\hat{ab}$",
    # functions and operator names
    r"$\sin x$", r"$\sin(x)$", r"$\cos^2\theta$", r"$\log_{10} x$", r"$\lim_{x\to0}f(x)$", r"$\max_i a_i$", r"$\exp(-x)$",
    r"$\tan\alpha+\cot\beta$", r"$\operatorname{erf}(x)$", r"$\operatorname{sgn} x$", r"$\operatorname{arg\,max}_x f$",
    r"$\ln 2$", r"$\sin$", r"$a\sin b$", r"$\sinh{x}$", r"$\Pr(A)$", r"$\liminf_{n} x_n$", r"$\sup_x$", r"$\det A$",
    r"$\operatorname{foo}^2 x$", r"$\sin ^2 x$", r"$\min _x$",
    # big operators and limits
    r"$\sum_{i=1}^{n} i$", r"$\prod_{k} a_k$", r"$\int_0^1 x\,dx$", r"$\oint_C f$", r"$\sum x$", r"$\sum^n$", r"$\sum_i$",
    r"$\bigcup_{i} A_i$", r"$\int f$", r"$\iint_D$", r"$\sum_{i=1}^{n}\frac{1}{i^2}$", r"$\bigoplus_k^N$", r"$\int^b_a$",
    # delimiters
    r"$\left(\frac{a}{b}\right)$", r"$\left[x\right]$", r"$\left\{x\right\}$", r"$\left|x\right|$", r"$\left.\frac{df}{dx}\right|_0$",
    r"$\left\langle a\middle|b\right\rangle$", r"$\left(x\right)$", r"$\left(\sum_i x_i\right)^2$", r"$\left\lfloor x\right\rfloor$",
    r"$\left\lceil\frac{x}{2}\right\rceil$", r"$\left\|v\right\|$", r"$\left/x\right\backslash$", r"$\left(\left(a\right)\right)$",
    r"$\left<x\right>$", r"$\left\uparrow x\right\downarrow$", r"$\left(\right)$", r"$\left.x\right.$",
    r"$\left( \frac{1}{2} \middle/ 3 \right)$", r"$\left\lbrace x\right\rbrace$", r"$\left\vert x\right\Vert$",
    # stacks, text, phantoms, bold
    r"$\overset{a}{b}$", r"$\underset{a}{b}$", r"$\overset{\mathrm{def}}{=}$", r"$\underset{x\to0}{\lim}$", r"$\text{speed (m/s)}$",
    r"$\text{a b}\ x$", r"$x\text{ if }y$", r"$\text{\{x\}}$", r"$\text{}$", r"$\sum_{\substack{i=1\\j=2}} a_{ij}$",
    r"$\substack{a\\b\\c}$", r"$a\phantom{bc}d$", r"$a\llap{b}c$", r"$a\rlap{b}c$", r"$\boldsymbol{x}$",
    r"$\boldsymbol{\alpha+\beta}$", r"$\boldsymbol{A1}$", r"$\phantom{\frac{1}{2}}x$", r"$\boldsymbol{x^2}$",
    # scripts in odd places
    r"$^2$", r"$_i$", r"${}^{14}C$", r"$x^{y^z}$", r"$x_{i_j}$", r"$\int_a^b f'(x)\,dx$", r"$x^\frac{1}{2}$", r"$e^{-x^2}$",
    r"$a^{b}_{c}$", r"$a_{c}^{b}$", r"$\mathrm{x}^2_i$", r"$(x)^2$", r"$\frac{1}{2}^3$", r"$\alpha_1^2$", r"$T_{\mathrm{eff}}$",
    r"$x^{2}{}^{3}$", r"$\Gamma^\mu_{\nu\lambda}$", r"$10^{-3}\,\mathrm{m\,s^{-1}}$", r"$\sigma_{\!x}$",
    # text around and several math parts
    r"Speed $v$ (m/s)", r"$\alpha$ and $\beta$", r"100\$ and $x$", r"no math", r"a $b$ c $d$ e", r"$a$$b$", r"$\$$",
    r"Temperature ($^\circ$C)", r"$\mathregular{H_2O}$", r"$\mathdefault{\mu}$m", r"$10^{\mathrm{th}}$",
]
# refused by matplotlib
constructs += [r"$\frac{1}$", r"$\sqrt$", r"$\left(x$", r"$x\right)$", r"$\hat$", r"$\frac{1}{}$", r"$\operatorname{x$",
               r"$x^$", r"$x_$", r"$$", r"$x$$", r"$\left x\right)$", r"$\genfrac{}{}{a}{0}{1}{2}$", r"$\hspace{x}$",
               r"$\overset{a}$", r"$\mathxx{a}$", r"$}$", r"$\text{a$", r"$a_b_c$", r"$a^b^c$", r'$"$', r"$#$", r"$\middle|$"]


rnd2 = random.Random(8)  # its own sequence: the sizes drawn below stay as they were


def nested(depth=0):
    a = rnd2.choice(["x", "y", "2", r"\alpha", "n", "k", "ab", "1"])
    if depth >= 3:
        return a
    r = rnd2.random()
    if r < 0.14:
        return r"\frac{%s}{%s}" % (nested(depth + 1), nested(depth + 1))
    if r < 0.24:
        return r"\sqrt{%s}" % nested(depth + 1)
    if r < 0.30:
        return r"\sqrt[%s]{%s}" % (rnd2.choice("3n"), nested(depth + 1))
    if r < 0.40:
        return "%s^{%s}" % (a, nested(depth + 1))
    if r < 0.50:
        return "%s_{%s}" % (a, nested(depth + 1))
    if r < 0.58:
        return r"\left(%s\right)" % nested(depth + 1)
    if r < 0.64:
        return r"\%s{%s}" % (rnd2.choice(["hat", "bar", "vec", "tilde", "widehat", "overline"]), nested(depth + 1))
    if r < 0.70:
        return r"\%s %s" % (rnd2.choice(["sin", "cos", "log", "exp"]), nested(depth + 1))
    if r < 0.76:
        return r"\sum_{%s}^{%s} %s" % (nested(depth + 2), nested(depth + 2), nested(depth + 1))
    if r < 0.80:
        return r"\int_{%s}^{%s} %s" % (a, nested(depth + 2), nested(depth + 1))
    if r < 0.92:
        return "%s %s %s" % (nested(depth + 1), rnd2.choice(["+", "-", "=", r"\times", r"\leq", ",", ""]), nested(depth + 1))
    return a


constructs += ["$" + nested() + "$" for _ in range(300)]

# --- the other fontsets -----------------------------------------------------------------------------
shown = [r"$x^2+y_i$", r"$\alpha\beta\Gamma\Omega$", r"$\frac{a}{b}$", r"$\sqrt{x}$", r"$\sum_{i=0}^{n}$", r"$\int_a^b$",
         r"$\left(\frac{1}{2}\right)$", r"$\hat{x}\vec{y}\widehat{abc}$", r"$\sin\theta$", r"$a\leq b\neq c\to d$",
         r"$\mathcal{ABC}\mathbb{RZ}\mathfrak{g}\mathscr{L}$", r"$\mathrm{abc}\mathit{abc}\mathbf{abc}\mathsf{abc}\mathtt{abc}$",
         r"$\mathbfit{abc}$", r"$f'(x)$", r"$10^{-3}$", r"$\infty\partial\nabla\hbar\ell$", r"$\pm\times\div\cdot$",
         r"$\left\{\frac{\frac{a}{b}}{c}\right\}$", r"$\sqrt[3]{\frac{x}{y}}$", r"$\overline{AB}$", r"$\prod\coprod\bigcup\oint$",
         r"$a-b$", r"$-1$", r"$x'$", r"plain $x$ text", r"$\mathdefault{10^{3}}$", r"$\left\langle x\right\rangle$",
         r"$\left[\sum_i\right]$", r"$\Delta\Sigma\Pi\theta\vartheta\varphi$", r"$1.5\,\mathrm{\mu m}$", r"$\imath\jmath\aleph$",
         r"$<>|/\backslash$", r"$\{\}[]()$", r"$\widetilde{xy}\widebar{z}$", r"$\left\lfloor x\right\rceil$", r"$\oint\iint\iiint$",
         r"$\boldsymbol{\alpha x}$", r"$\text{abc}$", r"$\mathbb{1}\mathbb{k}$", r"$\mathcal{l}\mathfrak{AZ}\mathscr{az}$"]
shown += ["$\\math%s{%s}$" % (font, chunk) for font in ["rm", "it", "bf", "bfit", "sf", "tt", "cal", "bb", "frak", "scr"]
          for chunk in ["ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", "0123456789", r"\alpha\beta\gamma\Gamma\Delta"]]
shown += ["$" + nested() + "$" for _ in range(40)]
by_fontset = [(fontset, s) for fontset in ["cm", "stix", "stixsans", "dejavuserif", "custom"] for s in shown]

# --- settings: the font of plain math, the fallback of the custom fontset, its fonts ----------------
with_rc = []
probe = [r"$x^2+y_i$", r"$\alpha\beta\Gamma\Omega 12$", r"$\sum_{i=0}^{n}\frac{a}{b}$", r"$\left(\sqrt{x}\right)$",
         r"$a-b\times c\pm d$", r"$\mathrm{a}\mathit{b}\mathbf{c}\mathcal{D}\mathbb{R}\mathfrak{g}$", r"$\hat{x}\widehat{abc}\sin\theta'$",
         r"$\int_0^\infty\partial\nabla\hbar$"]
for default in ["rm", "cal", "bfit", "it", "tt", "sf", "bf", "default", "bb", "frak", "scr", "regular", "normal"]:
    for fontset in ["dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"]:
        with_rc += [{"s": s, "fontset": fontset, "rc": {"mathtext.default": default}} for s in probe[:3]]
sized = [r"$\left(\frac{a}{b}\right)$", r"$\left\{\frac{\frac{a}{b}}{c}\right\}$", r"$\sqrt{\frac{x}{y}}$", r"$\left[\sum_i\right]$",
         r"$\left\langle\int\right\rangle\widehat{abc}$", r"$\infty\partial\nabla\hbar\ell\aleph\imath$", r"$\prod\coprod\bigcup\oint\sum$"]
for fallback in ["cm", "stix", "stixsans", None]:
    with_rc += [{"s": s, "fontset": "custom", "rc": {"mathtext.fallback": fallback}} for s in probe + sized]
    with_rc += [{"s": s, "fontset": fontset, "rc": {"mathtext.fallback": fallback}}
                for fontset in ["dejavusans", "cm", "stix"] for s in sized[:2]]
custom_fonts = [{"mathtext.rm": "serif"}, {"mathtext.rm": "cmr10"}, {"mathtext.rm": "cmr10", "mathtext.fallback": None},
                {"mathtext.rm": "DejaVu Sans Mono", "mathtext.it": "serif:italic"},
                {"mathtext.rm": "STIXGeneral", "mathtext.it": "STIXGeneral:italic"},
                {"mathtext.rm": "sans:bold", "mathtext.bf": "serif:bold", "mathtext.fallback": "stix"},
                {"mathtext.it": "cmmi10", "mathtext.bf": "cmb10", "mathtext.cal": "cmsy10", "mathtext.tt": "cmtt10",
                 "mathtext.sf": "cmss10"},
                {"mathtext.cal": "cursive", "mathtext.sf": "serif", "mathtext.tt": "sans", "mathtext.bfit": "serif:italic:bold",
                 "mathtext.fallback": "stixsans"}]
for fonts in custom_fonts:
    with_rc += [{"s": s, "fontset": "custom", "rc": fonts}
                for s in probe + [r"$a\times b-c$", r"$\mathrm{2\times10^{-3}}$", r"$\mathtt{ab}\mathsf{cd}\mathbfit{ef}$"]]

# --- corners: what coverage, mutants and random expressions showed was not looked at -----------------
corners = [
    # the number of \hspace
    r"$a\hspace{.5}b$", r"$a\hspace{+.5}b$", r"$a\hspace{-0.3}b$", r"$a\hspace{2.}b$", r"$a\hspace{ 1 }b$", r"$a\hspace {1}b$",
    r"$a\hspace{.}b$", r"$a\hspace{1.5.2}b$", r"$a\hspace{-}b$", r"$a\hspace{}b$", r"$a\hspace{1e3}b$", r"$a\hspace{0}b$",
    # \text: nested braces, escapes
    r"$\text{a{b}c}$", r"$\text{a\}b}$", r"$\text{a\\b}$", r"$\text{{{a}}}$", r"$\text {a}$", r"$\text{a\{b}$", r"$\text{a b  c}$",
    r"$\text{é中}$", r"$\text{a}b}$", r"$\text{a{b}$", r"$\text a$", r"$\text{\alpha}$", r"$\text{a^2_b}$", r"$\text{-+'}$",
    # \genfrac: every style, with and without the rule
    r"$\genfrac{}{}{0}{0}{a}{b}$", r"$\genfrac{}{}{0}{2}{a}{b}$", r"$\genfrac{}{}{1}{0}{a}{b}$", r"$\genfrac{}{}{1}{2}{a}{b}$",
    r"$\genfrac{}{}{1}{3}{a}{b}$", r"$\genfrac{(}{]}{0}{0}{a}{b}$", r"$\genfrac{.}{.}{1}{1}{a}{b}$", r"$\genfrac{}{}{}{0}{a}{b}$",
    r"$\genfrac{}{}{1}{4}{a}{b}$", r"$\genfrac{}{}{1}{}{a}{b}$", r"$\genfrac{|}{\}}{.5}{1}{x^2}{y}$", r"$\genfrac {}{} {2.} {1} {a} {b}$",
    r"$\genfrac{}{}{-1}{1}{a}{b}$", r"$\genfrac{\langle}{\rangle}{0}{3}{a}{b}$", r"$\genfrac{}{}{1}{1}{}{b}$", r"$\genfrac{}{}{1}{1}{a}{ }$",
    # the root of \sqrt
    r"$\sqrt[]{x}$", r"$\sqrt[3{x}$", r"$\sqrt[3{x}]{y}$", r"$\sqrt[a]b]{y}$", r"$\sqrt[ 3 ]{x}$", r"$\sqrt[\frac{1}{2}]{x}$",
    r"$\sqrt[3]x$", r"$\sqrt[3] [x]$", r"$\sqrt[a b]{x}$", r"$\sqrt[^2]{x}$", r"$\sqrt [ ] {x}$", r"$\sqrt[[]{x}$",
    # apostrophes with scripts
    "$x^{a}'$", "$x^a'$", "$x_{b}^{a}''$", "$x^{ab}'''$", r"$\sum^{a}'$", "$x^{}'$", "$x '$", "$x' '$", "$x_a '$", r"$\int'$",
    r"$\int_a'$", r"$f^\prime'$", "$'x'$", "$''^2$", "$'_a$",
    # \middle, and delimiters that size nothing
    r"$\left(a\middle.b\right)$", r"$\left(a\middle.b\middle|c\right)$", r"$\left(\middle|\right)$", r"$\left(a\middle|\middle|b\right)$",
    r"$\left(a\middle.\middle.b\right)$", r"$\left.a\middle.b\right.$", r"$\left(\left[a\right]\middle|\left\{b\right\}\right)$",
    r"$\left(.\middle.x\right)$", r"$\left(a\middle.\middle|b\right)$", r"$\left(a\middle|b\middle.\right)$", r"$\left(\middle.\right)$",
    r"$\left(a\middle x\right)$", r"$\left(a\middle\right)$", r"$\left (a\middle | b\right )$", r"$\left\lbrace a \middle\vert b \right\rbrace$",
    r"$\left(\frac{1}{2}\middle\|\frac{\frac{1}{2}}{3}\right)$", r"$\left.\middle|a\right.$", r"$\left(\right.$", r"$\left.\right)$",
    r"$\left(a\right$", r"$\left a\right)$", r"$\left(a\right x$", r"$\left\{\right\}$", r"$\left(a$",
    # \substack: pyparsing's nested braces and quoted strings
    r"$\substack{{}a}$", r"$\substack{{}\\a}$", r"$\substack{a\\{}\\b}$", r"$\substack{'}$", r"$\substack{''}$", r"$\substack{'a'}$",
    r"$\substack{a'b'}$", r"$\substack{a\\'b'}$", "$\\substack{'''}$", "$\\substack{''''}$", r"$\substack{a{b}}$", r"$\substack{{a}}$",
    r"$\substack{}$", r"$\substack{ }$", r"$\substack{{}}$", r"$\substack{{{}}}$", r"$\substack{a\\\\b}$", r"$\substack{a\\ \\ b}$",
    r"$\substack{'a\'b'}$", r"$\substack{'a\xb'}$", r"$\substack{'a\x1b'}$", r"$\substack{'a\xg'}$", r"$\substack{'a} x'}$",
    r"$\substack{a} x'$", r"$\substack{a\\{b}}$", r"$\substack{{}{}a}$", r"$\substack{a{}}$", r"$\substack{a\\b\\}$", r"$\substack{\\a}$",
    r"$\substack{a$", r"$\substack a$", r'$\substack{"a"}$', r'$\substack{"a}$', r"$\substack{'a''b'}$", r"$\substack{'a''}$",
    r"$\substack{ 'a' }$", r"$\substack{a 'b'}$", r"$\substack{x^2\\y_i\\\frac{1}{2}}$", r"$\substack{\rm a\\b}$",
    # a script on a list that holds only a space: an IndexError inside matplotlib, which pyparsing takes
    # for a failed match (once the action has run before, as it has here)
    r"${\,}^2$", r"${\,}'$", r"$\mathrm{\,}^2$", r"${ }^2$", r"${\,}_a^b x$", r"$x{\,}'$", r"${\,}^2_3^4$", r"$\sqrt{{\,}^2}$",
    r"$\operatorname{}'$", r"$\operatorname{ }''$", r"$\operatorname{}_x$", r"$\operatorname{}^2 x^3$", r"$a\operatorname{}b$",
    r"$\left.\,\right.^2$", r"$\left.\,\right._a^b$", r"$\left.\,\right.'$", r"$\left.\right.^2$", r"$\left(\,\right.^2$",
    r"$a\left.\,\right.^2 b$", r"$\left(\left.\,\right.^2\right)$", r"$\left(a\left.\quad\right._b\right)$",
    r"$\left[\left.\hspace{1}\right.'\middle|x\right]$", r"$\sin\left.\,\right.^2 x$", r"$\left.\rm\,\right.^2 x$", r"$\left.\,\,\right.^2 x$",
    r"$\left.{\,}\right.^2 x$", r"$\frac{\left.\,\right.^2}{b}$", r"$\sqrt[\left.\,\right.^2]{b}$", r"$\substack{\left.\,\right.^2}$",
    r"$\operatorname{\left.\,\right.^2}$", r"$x^{\left.\,\right.^2}$", r"$\mathrm{{\,}^2}$", r"$\mathbb{{\: }_\iiint}$",
    # groups that do not close
    r'${"}$', r'$\mathrm{a"}$', r"${a$", r"$\mathrm{a$", r"${a}}$", r"$\mathrm {a}$", r"$\math rm{a}$", r"$\mathrm{a}{b$", r"${{a}$",
    r"$\frac{a}{b$", r"$x^{a$", r"$\sqrt{a$", r"${a\\b}$",
    # accents and their argument
    r"$\hat  x$", r"$\hat\alpha$", r"$\hat {x}$", r"$\vec\imath$", r"$\'e$", r"$\' e$", r"$\hat^2$", r"$\widehat x$", r"$\hat\hat x$",
    r"$\hat\frac{1}{2}$", r"$\hat\left(x\right)$", r"$\hat x^2$", r"$\hat{}$", r"$\vec {ab}_i$", r"$\mathring x$", r"$\widetilde\phantom{x}$",
    r"$\overrightarrow x$", r"$\dot\sum$", r"$\bar\int_a$", r"$\hat ' $", r"$\^ o$", r'$\"{u}$', r"$\.x\~{n}\`a$",
    # the space after \operatorname and functions, and what it counts
    r"$\operatorname{ \,o^2}$", r"$\operatorname{  ^2}x^2$", r"$\operatorname{a}^2$", r"$\operatorname{ab}_x y$", r"$\operatorname{}$",
    r"$\operatorname{}^2$", r"$\operatorname{a }(x)$", r"$\operatorname{a}  (x)$", r"$\sin  ^2 x$", r"$\sin\,^2 x$", r"$\sin{}^2$",
    r"$\lim  _{x} y$", r"$\operatorname{  ^2}a+b$", r"$\operatorname{  ^2}\sum_a b$", r"$\operatorname{a^2}^2$", r"$\operatorname {ab} x$",
    r"$\operatorname{a}|x|$", r"$\operatorname{a}\|x\|$", r"$\operatorname{a}[x]$", r"$\operatorname{a}\{x\}$", r"$\operatorname{a}\langle x$",
    r"$\operatorname{a}/b$", r"$\operatorname{a}<b$", r"$\operatorname{a}.b$", r"$\sin|x|$", r"$\sin\langle x\rangle$", r"$\sin.$",
    r"$\operatorname{\alpha\,b}x$", r"$\operatorname{x_1}y$", r"$\operatorname{a$", r"$\operatorname a$", r"$\operatorname{\frac{1}{2}}$",
    r"$\lim^a_b x$", r"$\max'$", r"$\sin^2_3$", r"$\limsup_{x}^{y}z$", r"$\operatorname{lim}_a$", r"$\sin_a\cos^b\tan$",
    # font switches and their reach
    r"$a\rm b{\it c}d$", r"${\bf a}b$", r"$\mathrm{a\it b}c$", r"$\rm a^{\it b}c$", r"$\frac{\rm a}{b}c$", r"$\sqrt{\bf x}y$",
    r"$\left(\rm a\right)b$", r"$\cal A\bb B\frak C\scr D$", r"$\regular a\default b$", r"$\mathregular{a}\mathdefault{b}\mathnormal{c}$",
    r"$\rm$", r"$\rm\it$", r"$\mathrm{}$", r"$\mathxx{a}$", r"$\mathrm$", r"$\mathrm a$", r"$\rmx$", r"$\it1$",
    # phantoms and laps
    r"$a\phantom{\frac{1}{2}}b$", r"$\phantom{x}^2$", r"$a\llap{\frac{1}{2}}b$", r"$a\rlap{xy}^2b$", r"$\phantom{}$", r"$\phantom x$",
    r"$\phantom{a\phantom{b}c}d$", r"$\sqrt{\phantom{x}}$", r"$\overline{\phantom{ab}}$", r"$\phantom{\overline{ab}}c$", r"$\llap{}\rlap{}$",
    # symbols at the edges of the grammar
    r"$\#\#$", r"$a\#b$", r"$\#a$", r"$\ $", r"$\ a$", r"$a\ $", r"$~$", r"$a~~b$", r"$\,\;\:\>\!\/$", r"$\backslash$", r"$\slash$",
    r"$a^b^c$", r"$a_b_c$", r"$a^b_c^d$", r"$^$", r"$_$", r"$a^$", r"$a^ $", r"$a^^b$", r"$a_^b$", r"$%$", r"$a%b$", r"$\%$",
    r"$\1$", r"$\\$", "$\\$", r"$a\$", r"$\alpha1$", r"$\alphax$", r"$\alpha_1$", r"$\alpha{}x$", r"$\alpha x$", r"$\ALPHA$",
    r"$\sum\int\prod$", r"$\sum_a\int_b\prod_c$", r"$\int\limits_a$", r"$-$", r"$--$", r"$a--b$", r"$a-{-b}$", r"$a=-b$", r"$(-b)$",
    r"$1.5$", r"$1 .5$", r"$a.5$", r"$1.a$", r"$.$", r"$1.$", r"$1,5$", r"$1{,}5$", r"$1;2$", r"$1!$", r"$a!b$", r"$a\,.\,b$",
]
corners = [{"s": s} for s in corners]
# roots lower than their body (STIX), at the sizes where the fill has to shrink
low_roots = [r"$\sqrt{\hat bm}$", r"$\sqrt{a^{\iiint}}$", r"$\sqrt[4]{:^{\bigtriangleup}}$", r"$\sqrt{o^hV+^{J^t}}$", r"$\sqrt{\oint}$",
             r"$\sqrt{\frac{\int}{\sum}}$", r"$\sqrt{x}$", r"$\sqrt{\hat{A}}$"]
corners += [{"s": s, "fontset": fontset, "size": size, "dpi": dpi} for s in low_roots for fontset in ["stix", "stixsans"]
            for size, dpi in [(10.0, 100.0), (12.0, 72.0), (7.0, 150.0), (14.4, 100.0), (100.0, 72.0), (9.5, 96.0), (33.3, 200.0)]]

# every construct with something refused inside it, and with a plain thing inside it
wrappers = [r"\phantom{%s}", r"\llap{%s}", r"\rlap{%s}", r"\hat{%s}", r"\hat %s", r"\widehat{%s}", r"{%s}", r"\mathrm{%s}",
            r"\frac{%s}{b}", r"\frac{a}{%s}", r"\dfrac{%s}{b}", r"\binom{a}{%s}", r"\genfrac{}{}{1}{1}{%s}{b}",
            r"\genfrac{}{}{1}{1}{a}{%s}", r"\overset{%s}{b}", r"\overset{a}{%s}", r"\underset{%s}{b}", r"\sqrt{%s}",
            r"\sqrt[%s]{x}", r"\sqrt[3]{%s}", r"\overline{%s}", r"\underline{%s}", r"\boldsymbol{%s}", r"\operatorname{%s}",
            r"\substack{%s}", r"\substack{a\\%s}", r"\left(%s\right)", r"\left(a\middle|%s\right)", r"x^{%s}", r"x_{%s}",
            r"x^%s", r"x_a^{%s}", r"a%s", r"%s b", r"\sum_{%s}", r"\sin %s", r"\rm %s", r"\text{a}%s"]
refused_inside = [r"\nosuchsymbol", r"\frac{1}", r"a^b^c", r"\left(", r"{", r"\hspace{x}", r"\sqrt", r"\hat", r"x_",
                  r"\overset{a}", r"\genfrac{}{}{x}{0}{a}{b}", r"\middle|", r"\right)", r"}", r"\operatorname{", r"\text",
                  r"\substack", r"{\,}^2", r"\substack{{a}}", r"\phantom", r"\llap x", r"\rlap", r"\boldsymbol",
                  r"\boldsymbol{\frac{1}}", r"\phantom{\frac{1}}", r"\operatorname", r"\overline", r"\genfrac", r"\left"]
plain_inside = [r"x", r"\alpha", r"1+2", r"\frac{1}{2}", r"x^2_i", r"\,", r"", r" ", r"\sin", r"\left(a\right)", r"\sum", r"ab'"]
corners += [{"s": "$" + w % inner + "$"} for w in wrappers for inner in refused_inside + plain_inside]

# what mutants of the parser and of the fontsets showed was not looked at
corners += [{"s": s} for s in [
    r"$a\hspace{x5}b$", r"$a\hspace{-x5}b$", r"$a\hspace{ 5}b$", r"$a\hspace{5 }b$", r"$a\hspace{+}b$", r"$a\hspace{]1}b$",
    r"$\genfrac{}{}{]1}{2}{a}{b}$", r"$\genfrac{}{}{1}{2 }{a}{b}$", r"$\genfrac{}{}{1}{ 2}{a}{b}$", r"$\genfrac{}{}{1}{22}{a}{b}$",
    r"$\frac{}{b}$", r"$\binom{}{b}$", r"$\dfrac{}{b}$", r"$\frac{ }{b}$", r"$\underset{}{b}$", r"$\overset{a}{}$",
    r"$\overset{}{}$", r"$\underset{ }{ }$", r"$\text a{b}$", r"$\text{a}{b}$", r"$\text  {a b}$", r"$\textasciitide{}$",
    r"$\textx{a}$", r"$\fracx{1}{2}$", r"$\sqrtx{2}$", r"$\hatx$", r"$\leftx(a\right)$", r"$\sinx$", r"$\operatornamex{a}$",
    r"$\mathrmx{a}$", r"$\overlinex{a}$", r"$\substackx{a}$", r"$\phantomx{a}$", r"$\boldsymbolx{a}$", r"$\textasciitilde{}$",
    r"$\genfrac{}{}{0}{0}{,}{\"z}$", r"$\genfrac{}{}{0}{0}{y}{\hat{A}}$", r"$\genfrac{}{}{0}{1}{y_g}{\hat{A}^2}$",
    r"$\genfrac{}{}{0}{2}{,}{\"z}$", r"$\genfrac{}{}{0}{3}{g}{\hat{A}}$", r"$\binom{,}{\"z}$", r"$\genfrac{}{}{1}{0}{,}{\"z}$",
    r"$\frac{,}{\"z}$", r"$\substack{'\''}$", r"$\substack{'a'}5$", r"$\substack{'a''}5$", r"$\substack{'a\\'}$",
    r"$\substack{'\x0'}$", r"$\substack{'\x9'}$", r"$\substack{'\xa'}$", r"$\substack{'\xf'}$", r"$\substack{'\xA'}$",
    r"$\substack{'\xF'}$", r"$\substack{'\x/'}$", r"$\substack{'\x:'}$", r"$\substack{'\x@'}$", r"$\substack{'\xG'}$",
    r"$\substack{'\x`'}$", r"$\substack{'\xg'}$", r"$\substack{'\x'}$", r"$\substack{'\x1'}5$", r"$\substack{'a\x'}$",
    r"$\substack{'a\xfg'}$", r"$\substack{'ab''cd'}5$", r"$\substack{'ab'cd'}$", r"$\substack{'ab' 'cd'}$",
    r"$\sqrt{\left\}\right\langle}$", r"$\left(\right)$", r"$\left\{\right\rangle$", r"$\left|\right.$", r"$\left.\right|$",
    "$a\nb$", "$a\rb$", "a\n$b$", "$a$\n$b$", "$a\\\nb$", "$a\\$", "$a\\", "$a\\$b$", "\\$a$", "$a$\\$", "$", "a$", "$$a$",
    "a\\", "a\\b", "\\", "$a\\ b$", "$a$\\", "\\$", "\\$\\$", "$a$b$c$", "$a$ $b$ $c", "a $b",
    "$\x80$", "$\U0001ffff$", "$\U00020000$", "$\x7f$", "$a\x80b$", "$\U00010000$", "$￿$", "$\U0001d49c\U0001d7ff$",
    "a\tb $x$", "$a\tb$", "$\\text{a\tb}$", "ab\t$x^\t2$", "\t$x$", "a\n\tb$x$", "é\t$x$", "abcdefgh\ti$x$", "$x$\t$y$",
    r"$x^{x^{x^{x^{x^{x^{x}}}}}}$", r"$x_{x_{x_{x_{x_{x_{x}}}}}}$", r"$a^{\frac{1}{2}^{\hat{b}^{\sqrt{c}^{d\,e^{f^{g^{h}}}}}}}$",
    r"$\sum_{\sum_{\sum_{\sum_{\sum_{\sum_{i}}}}}}$", r"$x^{y_{z^{w_{v^{u_{t^{s}}}}}}}$", r"$\sqrt{x^{\sqrt{y^{\sqrt{z^{\sqrt{w}}}}}}}$",
    r"$a^{\left(b^{\left(c^{\left(d^{\left(e^{\left(f^{g}\right)}\right)}\right)}\right)}\right)}$",
    r"$x^{\overline{y^{\overline{z^{\overline{w^{\overline{v^{\overline{u^{t}}}}}}}}}}}$",
    r"$x^{a\,b^{c\,d^{e\,f^{g\,h^{i\,j^{k\,l}}}}}}$", r"$x^{\hat{y}^{\hat{z}^{\hat{w}^{\hat{v}^{\hat{u}^{\hat{t}}}}}}}$",
]]
# cases a random expression found, cut down to what matters, at the sizes where it does
corners += [{"s": s, "fontset": fontset, "size": size, "dpi": dpi} for s, fontset, size, dpi in [
    (r"$\left.\{\|_z\right\rfloor$", "dejavusans", 10.0, 72.0), (r"$\left|\{\|_z\right\rfloor$", "dejavusans", 10.0, 72.0),
    (r"$\widebar{\_}$", "stixsans", 10.0, 72.0), (r"$\widebar{\_}$", "stix", 10.0, 72.0), (r"$\widebar{\_}$", "stix", 12.0, 100.0),
    (r"$\genfrac{)}{}{0.0}{0}{,}{\"z}$", "cm", 100.0, 72.0), (r"$\genfrac{}{}{0}{0}{y}{\hat{A}}$", "cm", 100.0, 72.0),
    (r"$\genfrac{}{}{0}{0}{g}{\hat{A}}$", "dejavusans", 10.0, 100.0), (r"$\sqrt{\left\rbrace\right\langle }$", "cm", 10.0, 150.0),
    (r"$\left(\right)$", "cm", 10.0, 150.0), (r"$\left(\right)$", "stix", 10.0, 150.0), (r"$\left\{\right\rangle$", "stixsans", 12.0, 100.0),
    (r"$\left[\right]$", "dejavuserif", 14.4, 72.0), (r"$\left|\right|$", "custom", 10.0, 100.0),
]]
# fractions of every style and rule in every fontset (the font constants)
corners += [{"s": r"$\genfrac{}{}{%s}{%d}{%s}{%s}$" % (rule, style, num, den), "fontset": fontset}
            for fontset in ["dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"]
            for rule in ["0", "0.5", "-0.3", "3"] for style in range(4)
            for num, den in [("a", "b"), ("y_g", r"\hat{A}^2"), (r"\pm", r"\hookleftarrow")]]
corners += [{"s": s, "fontset": fontset} for fontset in ["dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"]
            for s in [r"$x^2_i\int^a_b\sum^a_b f'$", r"$A^{x^2}_{y_i}\Gamma^a_b g^a_b$", r"$\frac{y}{\hat{A}}\dfrac{g_j}{\sqrt{A}}$",
                      r"$\Cap\subsetneqq\nlesssim\l\combiningdotabove$", r"$\mathbf{\mathbb{A}\Cap}\mathit{\Cap}\mathrm{\Cap}$"]]
# the fonts of the custom fontset as fontconfig patterns
with_rc += [{"s": s, "fontset": "custom", "rc": {key: pattern}}
            for key, pattern in [("mathtext.rm", "serif:bold"), ("mathtext.rm", "DejaVu Sans Mono-12:italic"),
                                 ("mathtext.it", "sans:weight=600"), ("mathtext.rm", "serif:light"), ("mathtext.rm", ":bold"),
                                 ("mathtext.rm", "no such font"), ("mathtext.it", "serif,sans"), ("mathtext.bf", "serif:weight=bold,light"),
                                 ("mathtext.rm", "sans:condensed"), ("mathtext.tt", "sans:foo=bar"), ("mathtext.sf", "sans:weight=7.5"),
                                 ("mathtext.cal", "serif:slant=oblique:style=italic"), ("mathtext.bfit", "serif:size=large:italic:bold"),
                                 ("mathtext.rm", "serif "), ("mathtext.it", r"DejaVu Sans Mono:slant=oblique"),
                                 ("mathtext.rm", "sans:name=serif"), ("mathtext.rm", "sans\tserif")]
            for s in [r"$\mathrm{ab}\mathit{cd}\mathbf{ef}\mathtt{gh}\mathsf{ij}\mathcal{KL}\mathbfit{mn}x+1$", "", r"$\,$"]]

# --- random expressions over the whole grammar, some of them damaged ---------------------------------
rnd3 = random.Random(31)
names = sorted(tex2uni)
ascii_letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
chars = (list(ascii_letters) + list("0123456789") + list("+-*/<>=:,.;!?&@()[]|'")
         + ["αβΓ", "±", "∞", "é", "中", "ℏ", "∑", "√", "→", "×", "·"])
all_accents = sorted(set(Parser._accent_map) | set(Parser._wide_accents))
word_accents = [a for a in all_accents if a.isalpha()]
sign_accents = [a for a in all_accents if not a.isalpha()]
function_names = sorted(Parser._function_names)
fontnames = sorted(Parser._fontnames)
delims = sorted(Parser._delims)
space_names = sorted(Parser._space_widths)
overunder = sorted(Parser._overunder_symbols)
dropsub = sorted(Parser._dropsub_symbols)


def ws():
    r = rnd3.random()
    return "" if r < 0.75 else " " if r < 0.93 else "  "


def rsym():
    r = rnd3.random()
    if r < 0.45:
        return rnd3.choice(chars)
    if r < 0.60:
        return rnd3.choice(ascii_letters) + rnd3.choice(ascii_letters)
    if r < 0.85:
        return "\\" + rnd3.choice(names) + (" " if rnd3.random() < 0.7 else "{}")
    if r < 0.90:
        return rnd3.choice(overunder) + " "
    if r < 0.93:
        return rnd3.choice(dropsub) + " "
    return rnd3.choice([r"\%", r"\$", r"\{", r"\}", r"\_", r"\|"])


def rnum():
    return rnd3.choice(["0", "1", "0.5", "1.5", ".5", "2.", "-0.3", "+1", "0.0", "3", "10"])


def rarg(d):
    return "{" + ws() + rexpr(d + 1) + ws() + "}"


def ratom(d):
    r = rnd3.random()
    if d >= 3 or r < 0.30:
        return rsym()
    if r < 0.36:
        return rarg(d)
    if r < 0.42:
        return rnd3.choice([r"\frac", r"\dfrac", r"\binom"]) + ws() + rarg(d) + ws() + rarg(d)
    if r < 0.45:
        def delim():
            return rnd3.choice(["", "", "(", ")", "[", "|", r"\{", r"\langle", ".", "/"])
        return (r"\genfrac" + ws() + "{" + delim() + "}" + ws() + "{" + delim() + "}" + ws() + "{" + rnum() + "}" + ws()
                + "{" + rnd3.choice("0123") + "}" + ws() + rarg(d) + ws() + rarg(d))
    if r < 0.50:
        return r"\sqrt" + ws() + (("[" + rexpr(d + 2) + "]" + ws()) if rnd3.random() < 0.4 else "") + rarg(d)
    if r < 0.53:
        return rnd3.choice([r"\overline", r"\underline"]) + ws() + rarg(d)
    if r < 0.60:
        t = rnd3.random()
        return "\\" + rnd3.choice(word_accents) + (rarg(d) if t < 0.6 else " " + ws() + rsym() if t < 0.9 else ws() + ratom(d + 1))
    if r < 0.62:
        return "\\" + rnd3.choice(sign_accents) + ws() + rnd3.choice([rnd3.choice(ascii_letters), rarg(d)])
    if r < 0.67:
        return "\\" + rnd3.choice(function_names) + rnd3.choice([" ", "{}", "(", ""])
    if r < 0.69:
        return (r"\operatorname" + ws() + "{" + "".join(
            rnd3.choice([rnd3.choice(ascii_letters), r"\,", " ", rnd3.choice(ascii_letters) + "^2", r"\alpha "])
            for _ in range(rnd3.randint(0, 4))) + "}")
    if r < 0.72:
        return rnd3.choice([r"\overset", r"\underset"]) + ws() + rarg(d) + ws() + rarg(d)
    if r < 0.75:
        return (r"\text" + ws() + "{" + "".join(
            rnd3.choice(list(ascii_letters) + [" ", " ", "(", ")", "/", r"\{", r"\}", "-", "1", "é", r"\\", "{a}"])
            for _ in range(rnd3.randint(0, 6))) + "}")
    if r < 0.77:
        return (r"\boldsymbol" + ws() + "{" + "".join(
            rnd3.choice([rnd3.choice(ascii_letters), r"\alpha ", "1", "+", " ", "x^2", r"\Gamma "])
            for _ in range(rnd3.randint(0, 4))) + "}")
    if r < 0.79:
        return r"\substack" + ws() + "{" + r"\\".join(rexpr(d + 2) for _ in range(rnd3.randint(1, 3))) + "}"
    if r < 0.86:
        mid = rexpr(d + 1)
        if rnd3.random() < 0.3:
            mid += ws() + r"\middle" + ws() + rnd3.choice(delims) + ws() + rexpr(d + 1)
        return r"\left" + ws() + rnd3.choice(delims) + ws() + mid + ws() + r"\right" + ws() + rnd3.choice(delims)
    if r < 0.89:
        return rnd3.choice([r"\phantom", r"\llap", r"\rlap"]) + ws() + rarg(d)
    if r < 0.93:
        return "\\math" + rnd3.choice(fontnames) + ws() + rarg(d)
    if r < 0.95:
        return ("\\" + rnd3.choice(["rm", "it", "bf", "tt", "sf", "cal", "bb", "frak", "scr", "bfit", "regular", "default"]) + " "
                + rexpr(d + 1))
    if r < 0.98:
        return rnd3.choice(space_names) + ("" if rnd3.random() < 0.5 else " ")
    return r"\hspace" + ws() + "{" + ws() + rnum() + ws() + "}"


def rscripted(d):
    a = ratom(d) if rnd3.random() < 0.99 else ""
    r = rnd3.random()

    def script():
        t = rnd3.random()
        return (rarg(d) if t < 0.6 else rnd3.choice(ascii_letters + "0123456789") if t < 0.85
                else " " + rsym() if t < 0.9 else ratom(d + 1))
    primes = "'" * (0 if rnd3.random() < 0.9 else rnd3.randint(1, 3))
    if r < 0.60:
        return a
    if r < 0.72:
        return a + primes + ws() + "^" + script()
    if r < 0.84:
        return a + ws() + "_" + script()
    if r < 0.92:
        return a + ws() + "^" + script() + ws() + "_" + script() + (primes if rnd3.random() < 0.5 else "")
    if r < 0.97:
        return a + "_" + script() + "^" + script() + (primes if rnd3.random() < 0.3 else "")
    return a + primes


def rexpr(d=0):
    return ws().join(rscripted(d) for _ in range(rnd3.randint(1, 2 if d else 3)))


def damaged(s):
    for _ in range(rnd3.randint(1, 2)):
        if not s:
            break
        i = rnd3.randrange(len(s))
        t = rnd3.random()
        if t < 0.4:
            s = s[:i] + s[i + 1:]
        elif t < 0.8:
            s = s[:i] + rnd3.choice(list("{}^_\\$ []'.") + ["\\left", "\\right", "\\middle"]) + s[i:]
        else:
            j = rnd3.randrange(len(s))
            i, j = min(i, j), max(i, j)
            s = s[:i] + s[j:]
    return s


random_cases = []
while len(random_cases) < 700:
    s = rexpr()
    if len(s) > 160:  # (keeps the file small; the long ones add little)
        continue
    r = rnd3.random()
    s = ("$" + s + "$ text $" + rexpr(2) + "$" if r < 0.1 else "pre $" + s + "$" + rnd3.choice(["", " post", r" 5\$"]) if r < 0.2
         else "$" + s + "$")
    if rnd3.random() < 0.2:
        s = damaged(s)
    case = {"s": s, "fontset": rnd3.choice(["dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"]),
            "size": rnd3.choice([10.0, 12.0, 7.0, 14.4, 100.0, 9.5, 33.3]), "dpi": rnd3.choice([72.0, 100.0, 150.0, 200.0, 96.0])}
    rc = {}
    if rnd3.random() < 0.25:
        if rnd3.random() < 0.7:
            rc["mathtext.default"] = rnd3.choice(['rm', 'cal', 'bfit', 'it', 'tt', 'sf', 'bf', 'bb', 'frak', 'scr', 'regular'])
        if rnd3.random() < 0.6:
            rc["mathtext.fallback"] = rnd3.choice(["cm", "stix", "stixsans", None])
        if case["fontset"] == "custom" and rnd3.random() < 0.6:
            rc["mathtext.rm"] = rnd3.choice(["serif", "sans", "cmr10", "DejaVu Sans Mono", "STIXGeneral", "sans:bold"])
            if rnd3.random() < 0.5:
                rc["mathtext.it"] = rnd3.choice(["serif:italic", "sans:italic", "cmmi10", "STIXGeneral:italic"])
    if rc:
        case["rc"] = rc
    random_cases.append(case)


def layout(case):
    """matplotlib's layout of one case, in a parser that starts as a new process would have it."""
    fontset = case.get("fontset")
    prop = (FontProperties(size=case["size"]) if fontset is None
            else FontProperties(size=case["size"], math_fontfamily=fontset))
    try:
        with matplotlib.rc_context(case.get("rc", {})):
            parser = MathTextParser("path")  # (its cache of layouts does not look at rcParams)
            if MathTextParser._parser is not None:
                # matplotlib clears this flag after an expression it accepts only: a refused one
                # would leave it to the next
                MathTextParser._parser._needs_space_after_subsuper = False
            p = parser.parse(case["s"], case["dpi"], prop)
    except ValueError as err:  # matplotlib refuses the expression
        case["error"] = str(err).strip().splitlines()[-1]
        return case
    except (TypeError, AttributeError, IndexError) as err:  # matplotlib breaks on it
        case["error"] = type(err).__name__ + ": " + str(err)
        return case
    case.update({"width": h(p.width), "height": h(p.height), "depth": h(p.depth),
                 "glyphs": [{"font": pathlib.Path(f.fname).name, "size": h(fs), "num": int(num), "index": int(gi),
                             "ox": h(ox), "oy": h(oy)} for f, fs, num, gi, ox, oy in p.glyphs],
                 "rects": [[h(v) for v in r] for r in p.rects]})
    return case


cases = []
for s, fontset in [(s, None) for s in fixed + generated + symbols + constructs] + [(s, f) for f, s in by_fontset]:
    case = {"s": s, "size": rnd.choice([10.0, 10.0, 12.0, 7.0, 14.4, 100.0]), "dpi": rnd.choice([72.0, 100.0, 100.0, 150.0])}
    if fontset is not None:
        case["fontset"] = fontset
    cases.append(layout(case))
rnd4 = random.Random(5)
for case in with_rc + corners:
    case.setdefault("size", rnd4.choice([10.0, 12.0, 7.0, 14.4, 100.0, 9.5]))
    case.setdefault("dpi", rnd4.choice([72.0, 100.0, 150.0, 96.0]))
    cases.append(layout(dict(case)))
cases += [layout(case) for case in random_cases]
(ROOT / "tests" / "oracle" / "mathtext.json").write_text(json.dumps(cases, indent=0, ensure_ascii=False) + "\n")
print(len(cases), "expressions,", sum("error" in c for c in cases), "refused,", sum("fontset" in c for c in cases),
      "with a fontset,", sum("rc" in c for c in cases), "with settings,", sum(bool(c.get("rects")) for c in cases), "with rules")
