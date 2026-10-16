<div align="center">

# calculate-plot

**Figures identical to Matplotlib's, drawn in C++ with no Python at run time. <br> The plotting engine behind the [**calculate**](https://github.com/bentxr/calculate) project.**

[![ci](https://github.com/bentxr/calculate-plot/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/bentxr/calculate-plot/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)
![Tests](https://img.shields.io/badge/tests-GoogleTest-4285F4?logo=google&logoColor=white)
![Status](https://img.shields.io/badge/status-early%20development-orange)
![License](https://img.shields.io/badge/license-TBD-lightgrey)

</div>

```text
$ cplot draw squares.json squares.png --fonts fonts/ttf
$ cplot export-py squares.json squares.py
$ python squares.py      # Matplotlib 3.11.2 draws the same pixels
```

- **The same figure, to the pixel.** A PNG with the same RGBA pixels as Matplotlib 3.11.2's, and an SVG
  identical byte for byte.
- **No Python at run time.** Matplotlib's drawing code, ported to C++17 (Agg, the SVG backend, text layout,
  mathtext, ticks, legends, layouts), with the C libraries it relies on built in.
- **Checked against Matplotlib itself.** Every figure in the test suite is recorded from Matplotlib and
  compared call by call: what the renderer is asked to draw, the pixels, and the SVG bytes.
- **Native and in the browser.** The same results natively and in WebAssembly.

## Figure files

A figure is a list of Matplotlib calls, written as JSON:

```json
{"version": 1, "calls": [
  {"call": "subplots", "as": ["fig", "ax"]},
  {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]], "kwargs": {"label": "x²", "marker": "o"}},
  {"on": "ax", "call": "set_title", "args": ["Squares"]},
  {"on": "ax", "call": "legend"}]}
```

`cplot export-py` turns it into the plain Python script that draws it with Matplotlib:

```python
fig, ax = plt.subplots()
ax.plot([0, 1, 2, 3], [0, 1, 4, 9], label='x²', marker='o')
ax.set_title('Squares')
ax.legend()
```

| Part | Meaning |
|---|---|
| `call`, `on` | The method and the object it is called on (`ax`, `axs[0, 1]`, `ax.xaxis`); without `on`, a pyplot function |
| `args`, `kwargs` | The arguments, as JSON values |
| `as` | The name of the result, for the calls that follow |
| `style`, `rc` | A style sheet (or a list of them) and rcParams, applied before the calls |
| `{"$tuple": [...]}`, `{"$dict": {...}}` | A Python tuple, and a dict (`arrowprops`, say) |
| `{"$float": "nan"}` | The floats JSON cannot write (`nan`, `inf`, `-inf`) |
| `{"$ref": "name"}` | An object an earlier call made (a `Polygon` for `ax.add_patch`, a bar container for `ax.bar_label`) |

What a figure file asks for that is not supported yet is refused with a message saying so: a figure is
either drawn as Matplotlib draws it, or not at all.

## What it draws

| Kind | Calls |
|---|---|
| Figures and layout | `figure`, `subplots`, `add_subplot`, `add_axes`, `subplots_adjust`, `tight_layout`, `suptitle`, `text`, shared axes, `twinx`, `twiny`, `set_aspect` |
| Lines and markers | `plot` (with format strings), `step`, `scatter`, `errorbar`, `axhline`, `axvline`, `hlines`, `vlines` |
| Areas | `fill`, `fill_between`, `fill_betweenx`, `stairs`, `axhspan`, `axvspan`, `Polygon` |
| Statistics | `bar`, `barh`, `bar_label`, `hist` (every histogram type and bin rule), `boxplot`, `violinplot`, `stem` |
| Text | titles, axis labels, `text`, `annotate` (with every arrow and connection style), text boxes (`bbox`), mathtext, legends (including `loc='best'`) |
| Axes | linear and log scales, limits, grids, minor ticks, `tick_params`, `set`, `set_axis_off`, `NullFormatter` |
| Styles | the rcParams, Matplotlib's bundled style sheets and colour sequences |
| Output | PNG and SVG as `savefig` writes them (`bbox_inches='tight'` included), and the `.py` script |

## Build and test

Requires CMake ≥ 3.25, Ninja and a C++17 compiler. FreeType, HarfBuzz, SheenBidi, libraqm, zlib and
GoogleTest are fetched and pinned automatically.

```bash
cmake --preset native && cmake --build --preset native && ctest --preset native
```

<details>
<summary>WebAssembly</summary>

The tests run under Node.js.

```bash
source /path/to/emsdk/emsdk_env.sh
cmake --preset wasm && cmake --build --preset wasm && ctest --preset wasm
```

</details>

The references in `tests/` were recorded from Matplotlib 3.11.2. To record them again, the oracle in
`tools/oracle/` needs a Python environment with the versions pinned in `tools/oracle/requirements.lock`:

```bash
python -m venv tools/oracle/.venv && tools/oracle/.venv/bin/pip install -r tools/oracle/requirements.lock
LC_ALL=C.UTF-8 MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/make_figures.py build/native/cplot
```

## The command line

```bash
cplot draw FIGURE.json OUT.png|OUT.svg|OUT.json --fonts DIR
cplot export-py FIGURE.json OUT.py
```

| Command | Effect |
|---|---|
| `draw` | Draws a figure file as `savefig` would: a PNG, an SVG, or (`.json`) the calls the renderer receives |
| `export-py` | Writes the figure file as a Python script that draws it with Matplotlib |
| `replay`, `replay-svg` | Draws a trace recorded from Matplotlib's renderer, as a PNG or as SVG |
| `--licenses` | Prints the third-party notices and licence texts of this program |
| `--version` | Prints the version |

`--fonts` names the folder of the fonts Matplotlib ships (`fonts/ttf` in this repository).

## Using the library

C++:
```cpp
#include <calculate-plot/calculate-plot.hpp>

std::string error;
const std::string png = calculate_plot::drawPng(figureFile, "fonts/ttf", &error);    // the PNG's bytes
const std::string svg = calculate_plot::drawSvg(figureFile, "fonts/ttf", &error);    // the SVG text
const std::string script = calculate_plot::exportPython(figureFile, &error);          // the .py script
```

`drawScreen` draws for a screen at a device pixel ratio, `schema` describes every call and property a figure
file may use (and whether it is supported), and `licenceTexts` returns the notices to show.

CMake:
```cmake
include(FetchContent)
FetchContent_Declare(calculate-plot GIT_REPOSITORY https://github.com/bentxr/calculate-plot.git GIT_TAG <tag>)
FetchContent_MakeAvailable(calculate-plot)
target_link_libraries(your-app PRIVATE calculate-plot)
```

## Third-party code and licences

calculate-plot stands on other people's work, above all Matplotlib's: much of its code is Matplotlib's,
translated to C++. It also contains parts of NumPy, cycler and OpenBLAS translated the same way, Matplotlib's
copy of Anti-Grain Geometry, FDLIBM, Unicode data, colour schemes (ColorBrewer, Solarized), and the fonts
Matplotlib ships (DejaVu, STIX, BaKoMa, Last Resort). The libraries it builds in (FreeType, HarfBuzz, SheenBidi,
libraqm, zlib) keep their own licences. Every one of these licences is permissive.

I want to honour every one of them, and I have tried to do so carefully:

- every borrowed component is listed, with its version, licence and origin, in
  [`licensing/manifest.json`](licensing/manifest.json), and its licence text is kept unchanged in
  [`LICENSES/`](LICENSES);
- every file derived from someone else's code says where it comes from and under which licence, and the
  changes made to it are summarised in [`licensing/changes/`](licensing/changes);
- the notices are collected in [`NOTICE`](NOTICE) and built into the program (`cplot --licenses`);
- a test fails if any of this goes missing or out of date.

calculate-plot is an independent project. It is not affiliated with or endorsed by the Matplotlib project,
NumFOCUS or any of the projects above; their names are used only to say where the code comes from and what it
is compatible with.

**If you are an author or rights holder of any of this work, and something is missing, wrong, or not as you
would like it, please [open an issue](https://github.com/bentxr/calculate-plot/issues).** I will put it right.

## Status

WIP. The plotting section of the [**calculate**](https://github.com/bentxr/calculate) app is being built on it.

## License

Not chosen yet. Until one is added, all rights to the code written for this project are reserved. The
third-party parts keep their own licences, listed above.
