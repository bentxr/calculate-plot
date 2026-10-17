#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculate_plot {

// The library's version, "major.minor.patch".
std::string_view versionString();

// The third-party notices and licence texts that must travel with every copy of the library:
// NOTICE first, then the licence of every component compiled into it. Show them in an "About" or
// "Licences" view of any program that ships calculate-plot.
struct LicenceText {
    std::string_view path;  // where the text lives in the source tree, e.g. "LICENSES/FTL.txt"
    std::string_view text;
};
std::vector<LicenceText> licenceTexts();

// Drawing a figure file (the JSON call list `cplot export-py` turns into a matplotlib script). Each
// function returns nothing and says why in *error when the file cannot be read or drawn; what matplotlib
// 3.11.2 refuses is refused, and what calculate-plot does not do yet is refused as "not supported yet".
//
// The fonts are matplotlib's (fonts/ttf of the source tree): they are read from `fontDirectory`, or
// from the bytes useFontBytes gives for a file name.
//
// Every function may be called from several threads at once, and gives the same answer whatever locale the
// application set (numbers are read and written with a decimal point, as Python does).

// Where the fonts' bytes come from, for an application that holds the font files itself (as
// resources): the bytes of a file by its name, or empty to look in the font directory. Set it once,
// before drawing; the bytes must stay valid. nullptr goes back to the directory alone.
void useFontBytes(std::function<std::string_view(std::string_view fileName)> bytes);

// Figure.savefig as a PNG (rcParams savefig.*): the file's bytes.
std::string drawPng(std::string_view figureFile, const std::string& fontDirectory, std::string* error);

// Figure.savefig as an SVG: the document.
std::string drawSvg(std::string_view figureFile, const std::string& fontDirectory, std::string* error);

// The figure as a plain Python script that draws it with matplotlib.
std::string exportPython(std::string_view figureFile, std::string* error);

// The calls a figure file can make, as matplotlib defines them (data/schema.json, derived from
// matplotlib 3.11.2): for each call ("on": plt, figure, axes or axis), its parameters (name,
// positional, default) and the properties of the artist its other keywords set (name, aliases, valid
// values, kind: number, integer, bool, colour, choice with its choices, text or other). Each parameter
// and property has "supported": true when calculate-plot takes it, false when it refuses it as not
// supported yet, null when that could not be told (no sample value). JSON text; the fonts are needed
// for the calls that measure text.
std::string schema(const std::string& fontDirectory);

// An 8-bit RGBA image, rows from the top, 4 bytes per pixel, alpha not premultiplied.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// The figure on a screen with `devicePixelRatio` device pixels per logical pixel, as matplotlib's Qt
// canvas draws it: at the figure's dpi times the ratio, without the savefig settings.
Image drawScreen(std::string_view figureFile, double devicePixelRatio, const std::string& fontDirectory,
                 std::string* error);

// A widget the figure is shown in: its size in logical pixels and the screen's device pixels per logical pixel.
struct Canvas {
    double width = 0;
    double height = 0;
    double devicePixelRatio = 1;
};

// The figure in a widget, as matplotlib's Qt canvas draws it there: the figure's dpi times the ratio, the figure
// resized to the widget (Figure.set_size_inches(width * ratio / dpi, height * ratio / dpi)), then drawn without the
// savefig settings. A layout engine places the Axes again at that size; what tight_layout() or subplots_adjust()
// placed stays where it is in figure fractions. Sizes and ratios must be positive and finite.
Image drawCanvas(std::string_view figureFile, const Canvas& canvas, const std::string& fontDirectory, std::string* error);

// A gesture of matplotlib's navigation toolbar (NavigationToolbar2) on a canvas, from the press to the release, in
// logical pixels from the widget's top-left. Pan: the left button moves the view, the right one zooms it about the
// press point; Zoom: the rectangle from the press to the release, in with the left button, out with the right.
// Keys as matplotlib names them: "x" or "y" keep the gesture to one direction, "control" keeps the aspect (pan)
// and "shift" snaps a pan to an axis; others are no key.
struct Gesture {
    enum class Tool { Pan, Zoom } tool = Tool::Pan;
    int button = 1;   // 1 left, 3 right (2 does nothing, as in the toolbar)
    std::string key;  // "", "x", "y", "control", "shift"
    double fromX = 0, fromY = 0, toX = 0, toY = 0;
};

// The figure file after the gesture: the view each moved Axes is left with, as set_xlim / set_ylim calls at the end
// of its calls (by the names the file gives the Axes). The same file when the toolbar would do nothing (a press
// outside every Axes, a zoom of less than 5 pixels, the middle button).
std::string navigate(std::string_view figureFile, const Canvas& canvas, const Gesture& gesture, const std::string& fontDirectory,
                     std::string* error);

// What matplotlib's toolbar shows for the pointer at each point (logical pixels from the canvas' top-left): the
// coordinates under it as the Axes' format_coord writes them ("(x, y) = (1.25, 3.0)", the twins of an Axes
// joined by " | "), "" outside every Axes. Each call draws the figure once, so ask for several points at a time.
std::vector<std::string> pointerTexts(std::string_view figureFile, const Canvas& canvas, const std::vector<std::pair<double, double>>& points,
                                      const std::string& fontDirectory, std::string* error);

// Editing a figure file. Values are JSON text as figure files write them; whatever is not edited keeps its exact
// text. Each edit returns the new file, or "" and why when the result would not be a figure file (a call that is
// not one, an object used before a call names it).

// One call of a figure file: the object it is made on ("" for pyplot), its method, its arguments and keywords
// (JSON text, in order) and the names its result gets.
struct FigureCall {
    std::string on, method;
    std::vector<std::string> arguments;
    std::vector<std::pair<std::string, std::string>> keywords;
    std::vector<std::string> as;
};
std::vector<FigureCall> figureCalls(std::string_view figureFile, std::string* error);
// Call `call`'s keyword `name` set to `value` (in the place of an alias of the same property, else last), or removed
// with its aliases when `value` is empty.
std::string setKeyword(std::string_view figureFile, std::size_t call, const std::string& name, const std::string& value,
                       const std::vector<std::string>& aliases, std::string* error);
// Call `call`'s positional argument `position` set to `value` (one past the last appends it).
std::string setArgument(std::string_view figureFile, std::size_t call, std::size_t position, const std::string& value, std::string* error);
// A call (JSON text of the call's object) added after the last; a call removed.
std::string appendCall(std::string_view figureFile, const std::string& call, std::string* error);
std::string removeCall(std::string_view figureFile, std::size_t call, std::string* error);
// The figure's Axes (Figure.axes, in order) by the names the file gives them ("" for an Axes it does not name).
std::vector<std::string> axesNames(std::string_view figureFile, const std::string& fontDirectory, std::string* error);
// matplotlib's bundled style sheets by name (without "default", the defaults themselves).
std::vector<std::string> styleNames();
// The figure file's style (plt.style.use's argument: a name or a list of names as JSON text), or none when empty.
std::string setStyle(std::string_view figureFile, const std::string& style, std::string* error);
// The figure's calls run without drawing: what they refuse, or "" (what only a drawing finds is not seen).
std::string checkFigure(std::string_view figureFile, const std::string& fontDirectory);

}  // namespace calculate_plot
