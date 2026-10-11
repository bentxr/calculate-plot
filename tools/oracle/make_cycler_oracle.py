#!/usr/bin/env python3
"""Write tests/oracle/cycler.json: what matplotlib 3.11.2 makes of a property cycle written as text
(rcParams["axes.prop_cycle"] in a style sheet or a matplotlibrc: rcsetup.validate_cycler).

Each case is [text, null] where the text is refused (any exception) or [text, {property: [values]}]
with the cycle it makes, property by property (Cycler.by_key()), the properties in alphabetical
order. Values are written as a figure file writes them: a tuple is a list, a float that is not
finite {"$float": "nan" | "inf" | "-inf"}, an enumeration its name.

The texts are the cycles of the style sheets that come with matplotlib and a list of forms: every
way of calling cycler(), the algebra of cyclers (+, *, slices, concat), every property a cycle may
hold, and what is refused.

  tools/oracle/.venv/bin/python -I tools/oracle/make_cycler_oracle.py
"""
import json
import math
import pathlib
import warnings

import matplotlib
from matplotlib import rcsetup

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")

FORMS = [
    # one property: label and values, or a keyword
    "cycler('color', ['r', 'g', 'b'])", 'cycler("color", ["r", "g", "b"])', "cycler(color=['r', 'g', 'b'])",
    "cycler('color', 'rgb')", "cycler(color='rgb')", "cycler(c='rgb')", "cycler('c', 'rgb')", "cycler('c', ['r'])",
    "cycler(color='r, g, b')", "cycler(color='red, green')", "cycler(color='red')", "cycler(color=['red'])",
    "cycler(color=('r', 'g'))", "cycler('color', ('r', 'g'))", "cycler(color=[])", "cycler(color='')", "cycler(color=())",
    "cycler('color', ['1f77b4', 'ff7f0e', '2ca02c'])", "cycler('color', ['#1f77b4', '#FF7F0E'])", "cycler(color=['0.2', '0.8'])",
    "cycler(color=['0.00', '0.40', '0.60', '0.70'])", "cycler(color=[(1, 0, 0), (0, 1, 0)])", "cycler(color=[(1, 0, 0, 0.5)])",
    "cycler(color=[[1, 0, 0], [0, 0.5, 0]])", "cycler(color=[(1.0, 0.0, 0.0), 'b'])", "cycler(color=['none', 'k'])",
    "cycler(color=['None'])", "cycler(color=['tab:blue', 'xkcd:sky blue'])", "cycler(color=['Red', 'BLUE'])",
    "cycler(color=['#fff', '#ffff'])", "cycler(color=['#ff00ff80'])", "cycler(color=[('red', 0.5)])", "cycler(color=['C0'])",
    "cycler(color=['C0', 'r'])", "cycler(color='C0')", "cycler(color=['nocolor'])", "cycler(color=[1, 2, 3])",
    "cycler(color=[0.5])", "cycler(color=[None])", "cycler(color=None)", "cycler(color=1)", "cycler(color=0.5)",
    "cycler(color=True)", "cycler(color=['r', 1])", "cycler(color=[(1, 0)])", "cycler(color=[(1, 0, 0, 0, 0)])",
    "cycler(color=[(2, 0, 0)])", "cycler(color='bgrcmyk')", "cycler(color='r g b')", "cycler(color='red green')",
    "cycler(color=' r , g ')", "cycler(color='r,,g')", "cycler(color=',')", "cycler(color=['r', 'r', 'r'])",
    "cycler(color='(1, 0, 0)')", "cycler(color=['(1, 0, 0)'])", "cycler(color='1, 0, 0')", "cycler(color='0.5')",
    "cycler(color='0.5, 0.7')", "cycler(color='abc')", "cycler(color='abcdef')", "cycler(color='w')",
    # the other properties
    "cycler(lw=[1, 2, 3])", "cycler(linewidth=[1, 2.5])", "cycler('linewidth', [1, 2])", "cycler(lw='1, 2, 3')",
    "cycler(lw='1')", "cycler(lw=1)", "cycler(lw=2.5)", "cycler(lw=[1, 'a'])", "cycler(lw=['1', '2.5'])", "cycler(lw=[None])",
    "cycler(lw=[True])", "cycler(lw=[-1, 0])", "cycler(lw=(1, 2))", "cycler(lw='a')", "cycler(lw=[1e300, 1e-300])",
    "cycler(lw=[1_000])", "cycler(lw=[01])", "cycler(lw=[0x10])", "cycler(lw=[1.])", "cycler(lw=[.5])", "cycler(lw=[1e3])",
    "cycler(ls=['-', '--', ':', '-.'])", "cycler(linestyle=['solid', 'dashed'])", "cycler('linestyle', ['-', '--'])",
    "cycler(ls='-')", "cycler(ls='--')", "cycler(ls='-, --')", "cycler(ls=['Solid'])", "cycler(ls=['none', 'None', ' ', ''])",
    "cycler(ls=[(0, (1, 2))])", "cycler(ls=[(0, (1, 2)), '-'])", "cycler(ls=[[1, 2]])", "cycler(ls=['---'])", "cycler(ls=[1])",
    "cycler(ls=['dashdot', 'dotted'])", "cycler(ls=[(0, (1, 2, 3))])", "cycler(ls=[(0, ())])", "cycler(ls=[(1.5, (3, 1))])",
    "cycler(marker=['o', 's'])", "cycler(marker='os^')", "cycler(marker='o')", "cycler(marker='o, s')", "cycler(marker=['', 'None'])",
    "cycler(marker=[1, 2])", "cycler(marker=['$x$'])", "cycler(marker=['nomarker'])", "cycler(marker=[None])",
    "cycler(marker=['.', ',', 'x', '+', '*', 'D'])", "cycler(marker=[11])", "cycler(marker=[1.5])", "cycler(marker=[True])",
    "cycler(ms=[3, 6])", "cycler(markersize=[3.5])", "cycler(ms='3, 6')", "cycler(ms=['a'])",
    "cycler(mew=[1, 2])", "cycler(markeredgewidth=[0.5])", "cycler(mec=['r', 'b'])", "cycler(markeredgecolor='rb')",
    "cycler(mec=['auto'])", "cycler(mfc=['r', 'none'])", "cycler(markerfacecolor=['C1'])", "cycler(mfc=['auto'])",
    "cycler(mfc='rg')", "cycler(alpha=[0.5, 1])", "cycler(alpha=[2])", "cycler(alpha=[-1])", "cycler(alpha='0.5')",
    "cycler(alpha=[None])", "cycler(fc=['r', 'g'])", "cycler(facecolor=['r'])", "cycler(ec=['r', 'g'])",
    "cycler(edgecolor=['C2'])", "cycler(edgecolor=['face'])", "cycler(joinstyle=['miter', 'round'])", "cycler(joinstyle=['Miter'])",
    "cycler(capstyle=['butt', 'projecting'])", "cycler(capstyle=['flat'])", "cycler(fillstyle=['full', 'left'])",
    "cycler(fillstyle=['half'])", "cycler(markevery=[1, 2])", "cycler(markevery=[None, 0.1, (0, 3)])", "cycler(hatch=['/', 'x'])",
    "cycler(hatch=['a'])", "cycler(dashes=[(1, 2), (3, 1)])", "cycler(dashes=[(1, 2, 3)])", "cycler(dashes=[[]])",
    # what is no property of a cycle
    "cycler(colour=['r'])", "cycler('colour', ['r'])", "cycler(zorder=[1, 2])", "cycler(label=['a'])", "cycler(Color=['r'])",
    "cycler(COLOR=['r'])", "cycler(linewidths=[1])", "cycler(aa=[True])", "cycler(antialiased=[True])", "cycler(ds=['steps'])",
    "cycler(mfcalt=['r'])", "cycler(1, ['r'])", "cycler(None, ['r'])", "cycler(['color'], ['r'])", "cycler(('color',), ['r'])",
    "cycler(' color', ['r'])", "cycler('', ['r'])",
    # several properties at once
    "cycler(color=['r', 'g'], lw=[1, 2])", "cycler(c='rg', lw=[1, 2], ls=['-', '--'])", "cycler(lw=[1, 2], color=['r', 'g'])",
    "cycler(color=['r', 'g'], lw=[1, 2, 3])", "cycler(c='rgb', color='gb')", "cycler(c='rg', color='rg')",
    "cycler(color='rg', c='rg')", "cycler(lw=[1], linewidth=[2])", "cycler(color='rg', marker='os', ms=[3, 4], alpha=[0.5, 1])",
    "cycler(color=[], lw=[])", "cycler(color='r', lw=[1])", "cycler(color='rg', lw=[1])",
    # how cycler() may be called
    "cycler()", "cycler('color')", "cycler('color', 'r', 'g')", "cycler('color', ['r'], lw=[1])", "cycler(['r'])",
    "cycler('rgb')", "cycler(1)", "cycler(None)", "cycler(cycler(color='rg'))", "cycler(cycler('c', 'rg'))",
    "cycler(cycler(color='rg'), cycler(lw=[1, 2]))", "cycler(cycler(color='rg') + cycler(lw=[1, 2]))",
    "cycler(color=cycler(color='rg'))", "cycler('color', cycler(color='rg'))", "cycler(color=['r', 'g'],)",
    "cycler('color', ['r', 'g'],)", "cycler(color=['r', 'g',],)", "cycler(,)", "cycler(color=)", "cycler(=['r'])",
    "cycler(color=['r'] lw=[1])", "cycler(**{'color': ['r']})", "cycler(*['color', ['r']])", "cycler('color', *[['r']])",
    "cycler(color=['r'], color=['g'])",
    # the algebra
    "cycler('color', 'rgb') + cycler('lw', [1, 2, 3])", "cycler(color='rgb') + cycler(lw=[1, 2, 3]) + cycler(ls=['-', '--', ':'])",
    "cycler('color', 'rgb') + cycler('lw', [1, 2])", "cycler('color', 'rgb') + cycler('c', 'gbr')",
    "cycler(color='rg') + cycler(color='gb')", "cycler('color', 'rgb') * cycler('linestyle', ['-', '--'])",
    "cycler(ls=['-', '--']) * cycler(color='rgb')", "cycler(color='rg') * cycler(lw=[1, 2]) * cycler(ls=['-', ':'])",
    "cycler(color='rg') * cycler(color='gb')", "cycler(color='rg') * cycler(c='gb')",
    "cycler(color='rg') + cycler(lw=[1, 2]) * cycler(ls=['-', ':'])", "(cycler(color='rg') + cycler(lw=[1, 2])) * cycler(ls=['-', ':'])",
    "cycler(color='rg') * cycler(lw=[1, 2]) + cycler(ls=['-', ':', '--', '-.'])",
    "cycler(color='rg') * (cycler(lw=[1, 2]) + cycler(ls=['-', ':']))", "(cycler(color='rg'))", "((cycler(color='rg')))",
    "(cycler(color='rg')) + (cycler(lw=[1, 2]))", "cycler(color='rg') * 2", "2 * cycler(color='rg')", "cycler(color='rg') * 1",
    "cycler(color='rg') * 0", "cycler(color='rg') * -1", "cycler(color='rg') * True", "cycler(color='rg') * 2.0",
    "cycler(color='rg') * 'a'", "cycler(color='rg') * [1]", "cycler(color='rg', lw=[1, 2]) * 3", "3 * cycler(color='rg', lw=[1, 2])",
    "cycler(color='rg') * 2 * cycler(lw=[1, 2])", "cycler(color='rg') + 1", "1 + cycler(color='rg')",
    "cycler(color='rg') + ['a', 'b']", "cycler(color='rg') + [1]", "cycler(color='rg') + None", "cycler(color='rg') + 'ab'",
    "cycler(color='rg') - cycler(lw=[1, 2])", "cycler(color='rg') / 2", "cycler(color='rg') ** 2", "cycler(color='rg') @ cycler(lw=[1])",
    "cycler(color='rg') % 2", "cycler(color='rg') // 2", "cycler(color='rg') | cycler(lw=[1, 2])", "-cycler(color='rg')",
    "+cycler(color='rg')", "cycler(color='rg') if True else cycler(color='gb')", "cycler(color='rg') and cycler(color='gb')",
    "cycler(color='rg') == cycler(color='rg')", "cycler(color='rgb')[:2]", "cycler(color='rgb')[1:]", "cycler(color='rgb')[::2]",
    "cycler(color='rgb')[::-1]", "cycler(color='rgb')[:]", "cycler(color='rgb')[0:0]", "cycler(color='rgb')[5:]",
    "cycler(color='rgb')[-1:]", "cycler(color='rgb')[:-1]", "cycler(color='rgb')[1:3:1]", "cycler(color='rgb')[0]",
    "cycler(color='rgb')[-1]", "cycler(color='rgb')['a':]", "cycler(color='rgb')[None:2]", "cycler(color='rgb')[1.0:]",
    "cycler(color='rgb')[True:]", "cycler(color='rgb')[::0]", "cycler(color='rgb', lw=[1, 2, 3])[1:]", "cycler(color='rgb')[:2][1:]",
    "(cycler(color='rgb') + cycler(lw=[1, 2, 3]))[::2]", "cycler(color='rgb')[:2] + cycler(lw=[1, 2])",
    "cycler('color', ['1f77b4', '#ff7f0e'])[:1]", "cycler(color='rgb')[1:2, 0:1]", "cycler(color='rgb')[...]",
    "concat(cycler(color='rg'), cycler(color='b'))", "concat(cycler(color='rg'), cycler(c='b'))",
    "concat(cycler(color='rg'), cycler(lw=[1]))", "concat(cycler(color='rg'))", "concat()", "concat(cycler(color='r'), cycler(color='g'), cycler(color='b'))",
    "concat(cycler(color='rg', lw=[1, 2]), cycler(lw=[3], color='b'))", "concat(left=cycler(color='r'), right=cycler(color='g'))",
    "concat(cycler(color='r'), right=cycler(color='g'))", "concat('a', 'b')", "concat([1], [2])", "concat(cycler(color='r'), 1)",
    "concat(cycler(color='rg'), cycler(color='b')) * cycler(lw=[1, 2])", "cycler(color='rg').concat(cycler(color='b'))",
    "cycler(color='rg').by_key()", "cycler.cycler(color='rg')", "matplotlib.cycler(color='rg')", "Cycler(color='rg')",
    "CYCLER(color='rg')", "Cycler('color', 'rg')", "ccycler('color', 'rg')", "list('rg')", "len('rg')", "print(1)",
    "__import__('os')", "cycler(color=list('rg'))", "cycler(color=[c for c in 'rg'])", "cycler(color=['r'] * 2)",
    "cycler(color=['r'] + ['g'])", "cycler(lw=[1 + 1])", "cycler(lw=[-1])", "cycler(lw=[+1])", "cycler(lw=[1, -2.5])",
    "cycler(color='r' 'g')", "cycler(color='r' + 'g')", "cycler(color='rg' * 2)", "cycler(lw=[2] * 3)", "cycler(lw=(1, 2) + (3,))",
    "cycler(color={'r', 'g'})", "cycler(color={'r': 1})", "cycler(color=b'rg')", "cycler(lw=[1j])", "cycler(lw=range(3))",
    "cycler(lw=[1, 2][:1])", "cycler(color='rgb'[:2])", "cycler(color=('r'))", "cycler(color=('r',))", "cycler(color=[('r')])",
    "cycler(lw=[(1)])", "cycler(lw=((1), (2)))",
    # what is no cycler at all
    "", " ", "cycler", "color", "None", "True", "1", "1.5", "'r'", "['r', 'g']", "('color', ['r'])", "{'color': ['r']}",
    "red", "rgb", "r, g, b", "['r', 'g'] + ['b']", "1 + 2", "'a' * 3", "[1] * 2", "(", ")", "cycler(", "cycler)", "cycler(]",
    "cycler('color', ['r', 'g']", "cycler('color', ['r', 'g')", "cycler('color, ['r'])", "cycler('color', ['r'])) ",
    "cycler('color', ['r']) cycler('lw', [1])", "cycler('color', ['r']); cycler('lw', [1])", "cycler('color', ['r']),",
    "cycler('color', ['r']), cycler('lw', [1])", "x = cycler('color', ['r'])", "lambda: cycler(color='r')", "cycler(color='r')()",
    "[cycler(color='r')]", "(cycler(color='r'),)", "cycler(color='r') +", "+ cycler(color='r')", "* cycler(color='r')",
    "cycler(color='r') * * 2", "cycler(color='rg') *2", "cycler(color='rg')*2", "2*cycler(color='rg')",
    "cycler(color='rg')+cycler(lw=[1,2])", "cycler(color='rg')*cycler(lw=[1,2])",
    # spaces, breaks, comments and other spellings Python reads
    " cycler(color='rg')", "cycler(color='rg') ", "  cycler( 'color' , [ 'r' , 'g' ] )  ", "cycler ('color', ['r'])",
    "cycler\t('color',\t['r'])", "\tcycler(color='rg')", "cycler(color='rg')\n", "\ncycler(color='rg')", "cycler(color='rg') # the colours",
    "cycler(color='rg')# c", "# cycler(color='rg')", "cycler(\n'color',\n['r', 'g']\n)", "cycler(color='rg') \\\n+ cycler(lw=[1, 2])",
    "cycler(color='rg')\n+ cycler(lw=[1, 2])", "(cycler(color='rg')\n+ cycler(lw=[1, 2]))", "cycler(color = 'rg')",
    "cycler(color= 'rg' )", "cycler( color='rg' , lw=[ 1 , 2 ] )", 'cycler(color="rg")', "cycler(color='''rg''')",
    'cycler(color="""rg""")', "cycler(color=r'rg')", "cycler(color=u'rg')", "cycler(color=f'rg')", "cycler(color=b'r')",
    "cycler(color='r\\x67')", "cycler(color='\\x72g')", "cycler(color=['r\\n'])", "cycler(color=\"it's\")", "cycler(color='a\"b')",
    "cycler('c\\x6flor', ['r'])", "cycler(color='rg');", "cycler(color='rg'),", "cycler(color='réd')", "cycler(color=['é'])",
    "cycler(cölor=['r'])", "cyclér(color=['r'])", "cycler(color=['r']) ", " cycler(color=['r'])",
    "cycler(color=['r', 'g'])", "cycler(color=[‘r’])", "cycler(lw=[٣])", "cycler(lw=[1， 2])",
    "cycler(color=['r'], lw=[1e400])", "cycler(lw=[1e400])", "cycler(lw=[-1e400])", "cycler(lw=[float('nan')])", "cycler(lw=[nan])",
    "cycler(lw=[inf])", "cycler(lw=['nan'])", "cycler(lw=['inf', '-inf'])", "cycler(lw='nan')", "cycler(alpha=['nan'])",
    "cycler(lw=[123456789012345678901234567890])", "cycler(lw=[0b11, 0o17])", "cycler(lw=[1_0.5])", "cycler(lw=[1E2, 1e-2])",
    "cycler(lw=[1, 2, ])", "cycler(lw=[1, 2,, 3])", "cycler(lw=[1 2])", "cycler(lw=[1;2])",
    # more of the same, one text for each turn the reading can take
    "cycler(lw=[1]) * -(1)", "cycler(color='rg') * +2", "- 1 * cycler(color='rg')", "cycler(color='rg') * - 1",
    "cycler(color='rgb')[1:2:3:4]", "cycler(color='rgb')[1:2:1]", "cycler(color='rgb')[::]", "cycler(color='rgb')[:2:]",
    "cycler(color='rgb')[False:2]", "cycler(color='rgb')[[1]:]", "cycler(color='rgb')[:(2)]", "cycler(color='rgb')[-3:-1]",
    "cycler(color=['r'], ['g'])", "cycler(cycler(color='r'), ['r'])", "cycler('color', ['r'], ['g'])",
    "concat(cycler(color='r'), left=cycler(color='g'))", "concat(cycler(color='r'), other=cycler(color='g'))",
    "concat(right=cycler(color='g'), left=cycler(color='r'))", "concat(left=cycler(color='r'))",
    "cycler(color='rg') + (1, 2)", "cycler(color='rg') + (1,)", "['a', 'b'] + cycler(color='rg')", "'ab' + cycler(color='rg')",
    "cycler(color='rg') + True", "cycler(color='rg') + 1.5", "cycler(lw=[1] + (2,))", "cycler(lw=(1,) + [2])",
    "cycler(color='r' + ['g'])", "cycler(lw=[1] + 2)", "cycler(lw=3 * [2])", "cycler(lw=[1] * -1 + [2])", "cycler(lw=[1] * 0)",
    "cycler(lw=[1] * True)", "cycler(lw=[1] * 2.0)", "cycler(lw=[1] * [2])", "cycler(lw=[1] * None)", "cycler(color=2 * 'rg')",
    "cycler(lw=(1, 2) * 2)", "cycler(lw=[1]) * (2 * 1)", "cycler(lw=[1]) * (1 + 1)", "cycler(lw=[1, 2] * 2 + [3])",
    "cycler(lw=(1, 2, 3)[1:])", "cycler(lw=5[1:])", "cycler(lw=None[1:])", "cycler(lw=[1, 2, 3][::-1])", "cycler(lw=[1, 2, 3][::0])",
    "cycler(lw=[1, 2, 3][1])", "cycler(color='rgb'[::-1])", "cycler(color='rg')\r\n", "cycler(color='rg')\f", "cycler(color='rg') ?",
    "cycler(color='rg') $", "cycler(color='rg') !", "cycler(color='rg') \\ ", "cycler(color='rg') \\", "cycler(color='rg') ` `",
    "cycler(color='rg') / cycler(lw=[1, 2])", "cycler(color='rg') % cycler(lw=[1, 2])", "cycler(color='rg') & cycler(lw=[1, 2])",
    "cycler(color='rg') ^ cycler(lw=[1, 2])", "cycler(color='rg') < cycler(lw=[1, 2])", "cycler(color='rg') > cycler(lw=[1, 2])",
    "cycler(color='rg') ~ cycler(lw=[1, 2])", "cycler(color='rg') = cycler(lw=[1, 2])", "cycler(color='rg') : cycler(lw=[1, 2])",
    "cycler(color='rg') . cycler", "cycler(color='rg') cycler(lw=[1, 2])", "cycler(color='rg') 2", "cycler(color='rg') 'a'",
    "cycler(color='rg') [1, 2]", "cycler(color='rg') (1, 2)", "cycler(color='rg') {1}", "cycler(color='rg') None",
    "cycler(color='rg'))", "cycler(color='rg')]", "cycler(color='rg')}", "(cycler(color='rg')", "(cycler(color='rg')]",
    "(cycler(color='rg'), cycler(lw=[1, 2]))", "(cycler(color='rg') cycler(lw=[1, 2]))", "()", "[]", "(1)", "((1))", "-1", "+1", "-(1)",
    "cycler(color=None)", "cycler(color=True)", "cycler(lw=True)", "cycler(lw=False)", "cycler(lw=none)", "cycler(lw=true)",
    "cycler(color=['C10'])", "cycler(color=['C'])", "cycler(color=['CC'])", "cycler(color=['C1', 'C2'])", "cycler(color='C1, C2')",
    "cycler(color='C12')", "cycler(mec=['C0', 'C1'])", "cycler(color=['c0'])", "cycler(color='tab30')", "cycler(color='TAB10')",
    "cycler(color=' tab10')", "cycler(color=['tab10'])", "cycler(mfc='tab10')", "cycler(ec='Set1')", "cycler(lw='tab10')",
] + ["cycler(color='%s')" % name for name in sorted(matplotlib.color_sequences)]


def stored(v):
    """A value of a cycle, as the oracle keeps it."""
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


def outcome(text):
    try:
        made = rcsetup.validate_cycler(text)
    except Exception:  # noqa: BLE001
        return None
    by_key = made.by_key()
    return {key: [stored(x) for x in by_key[key]] for key in sorted(by_key)}


texts = []
styles = pathlib.Path(matplotlib.get_data_path()) / "stylelib"
for sheet in sorted(styles.glob("*.mplstyle")) + [pathlib.Path(matplotlib.get_data_path()) / "matplotlibrc"]:
    for line in sheet.read_text().splitlines():
        line = line.lstrip("#")
        if line.startswith("axes.prop_cycle"):
            # (as matplotlib reads the line: the value after the colon, without its comment)
            value = line.split(":", 1)[1]
            texts.append(matplotlib.cbook._strip_comment(value) if hasattr(matplotlib.cbook, "_strip_comment") else value.strip())
texts += FORMS
cases, seen = [], set()
for text in texts:
    if text not in seen:
        seen.add(text)
        cases.append([text, outcome(text)])

out = ROOT / "tests" / "oracle" / "cycler.json"
document = {
    "_provenance": "matplotlib 3.11.2 (cycler %s), rcsetup.validate_cycler; tools/oracle/make_cycler_oracle.py" % __import__("cycler").__version__,
    "cases": cases,
}
out.write_text(json.dumps(document, ensure_ascii=False, indent=None, separators=(",", ":")).replace("],[", "],\n[") + "\n")
taken = sum(1 for _, made in cases if made is not None)
print(f"{out.relative_to(ROOT)}: {len(cases)} texts, {taken} taken, {len(cases) - taken} refused")
