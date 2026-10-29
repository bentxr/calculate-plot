// Derived from Matplotlib 3.11.2: lib/matplotlib/lines.py (Line2D), lib/matplotlib/markers.py
// (MarkerStyle) and lib/matplotlib/cbook.py (the step functions of STEP_LOOKUP_MAP).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "support/pynum.hpp"
#include "engine/lines.hpp"

#include "engine/axes.hpp"
#include "support/pyformat.hpp"
#include "support/raised.hpp"
#include "svg/svg_renderer.h"
#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <map>
#include <mutex>

namespace calculate_plot::engine {

namespace {

// The marker paths markers.py makes when it is imported (with the default rcParams). The unit
// circle, rectangle, polygons and star are made by Path the first time they are asked for, under the
// rcParams of then: the figure's (MarkerStyle::set gives them those).
const Path& pathOf(std::vector<double> vertices, std::vector<unsigned char> codes, bool closed, int which) {
    static std::map<int, Path> cache;
    static std::mutex lock;  // (drawings on several threads; a path never changes once made)
    std::lock_guard<std::mutex> making(lock);
    auto it = cache.find(which);
    if (it == cache.end()) it = cache.emplace(which, Path::make(std::move(vertices), std::move(codes), rc::Params::defaults(), closed)).first;
    return it->second;
}

const Path& tickvertPath() { return pathOf({-0.0, 0.0, -0.0, 1.0}, {}, false, 1); }
const Path& tickhorizPath() { return pathOf({0.0, 0.0, 1.0, 0.0}, {}, false, 2); }
const Path& lineMarkerPath() { return pathOf({0.0, -1.0, 0.0, 1.0}, {}, false, 3); }

// Path._create_closed: the first vertex repeated, closed.
std::vector<double> closedVertices(std::vector<double> v) {
    v.push_back(v[0]);
    v.push_back(v[1]);
    return v;
}
std::vector<double> scaledBy(std::vector<double> v, double d) {
    for (double& x : v) x = x / d;
    return v;
}

const Path& trianglePath() { return pathOf(closedVertices({0, 1, -1, -1, 1, -1}), {}, true, 4); }
const Path& triPath() {
    return pathOf({0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.8, 0.5, 0.0, 0.0, -0.8, 0.5}, {MOVETO, LINETO, MOVETO, LINETO, MOVETO, LINETO},
                  false, 5);
}
const Path& caretPath() { return pathOf({-1.0, 1.5, 0.0, 0.0, 1.0, 1.5}, {}, false, 6); }
const Path& caretPathBase() { return pathOf({-1.0, 0.0, 0.0, -1.5, 1.0, 0}, {}, false, 7); }
const Path& plusPath() { return pathOf({-1.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 1.0}, {MOVETO, LINETO, MOVETO, LINETO}, false, 8); }
const Path& xPath() { return pathOf({-1.0, -1.0, 1.0, 1.0, -1.0, 1.0, 1.0, -1.0}, {MOVETO, LINETO, MOVETO, LINETO}, false, 9); }
const Path& plusFilledPath() {
    return pathOf(closedVertices(scaledBy({-1, -3, +1, -3, +1, -1, +3, -1, +3, +1, +1, +1, +1, +3, -1, +3, -1, +1, -3, +1,
                                           -3, -1, -1, -1},
                                          6)),
                  {}, true, 10);
}
const Path& xFilledPath() {
    return pathOf(closedVertices(scaledBy({-1, -2, 0, -1, +1, -2, +2, -1, +1, 0, +2, +1, +1, +2, 0, +1, -1, +2, -2, +1, -1, 0,
                                           -2, -1},
                                          4)),
                  {}, true, 11);
}

// The halves markers.py makes when it is imported: of the triangle, and of the filled plus and x
const Path& triangleUPath() { return pathOf(closedVertices({0, 1, -3.0 / 5, -1.0 / 5, 3.0 / 5, -1.0 / 5}), {}, true, 13); }
const Path& triangleDPath() { return pathOf(closedVertices({-3.0 / 5, -1.0 / 5, 3.0 / 5, -1.0 / 5, 1, -1, -1, -1}), {}, true, 14); }
const Path& triangleLPath() { return pathOf(closedVertices({0, 1, 0, -1, -1, -1}), {}, true, 15); }
const Path& triangleRPath() { return pathOf(closedVertices({0, 1, 0, -1, 1, -1}), {}, true, 16); }
const Path& plusFilledTopPath() {
    return pathOf(closedVertices(scaledBy({+3, 0, +3, +1, +1, +1, +1, +3, -1, +3, -1, +1, -3, +1, -3, 0}, 6)), {}, true, 17);
}
const Path& xFilledTopPath() {
    return pathOf(closedVertices(scaledBy({+1, 0, +2, +1, +1, +2, 0, +1, -1, +2, -2, +1, -1, 0}, 4)), {}, true, 18);
}

// Path.unit_circle_righthalf: four cubic Bezier eighth-arcs and the line back.
const Path& unitCircleRightHalf() {
    const double MAGIC = 0.2652031, SQRTHALF = std::sqrt(0.5), MAGIC45 = SQRTHALF * MAGIC;
    std::vector<double> v{0.0, -1.0, MAGIC, -1.0, SQRTHALF - MAGIC45, -SQRTHALF - MAGIC45, SQRTHALF, -SQRTHALF,
                          SQRTHALF + MAGIC45, -SQRTHALF + MAGIC45, 1.0, -MAGIC, 1.0, 0.0, 1.0, MAGIC,
                          SQRTHALF + MAGIC45, SQRTHALF - MAGIC45, SQRTHALF, SQRTHALF, SQRTHALF - MAGIC45, SQRTHALF + MAGIC45,
                          MAGIC, 1.0, 0.0, 1.0, 0.0, -1.0};
    std::vector<unsigned char> codes(14, CURVE4);
    codes.front() = MOVETO;
    codes.back() = CLOSEPOLY;
    return pathOf(std::move(v), std::move(codes), false, 19);
}

// Path.unit_circle: eight cubic Bezier quarter-arcs.
const Path& unitCircle() {
    const double MAGIC = 0.2652031, SQRTHALF = std::sqrt(0.5), MAGIC45 = SQRTHALF * MAGIC;
    std::vector<double> v{0.0, -1.0, MAGIC, -1.0, SQRTHALF - MAGIC45, -SQRTHALF - MAGIC45, SQRTHALF, -SQRTHALF,
                          SQRTHALF + MAGIC45, -SQRTHALF + MAGIC45, 1.0, -MAGIC, 1.0, 0.0, 1.0, MAGIC,
                          SQRTHALF + MAGIC45, SQRTHALF - MAGIC45, SQRTHALF, SQRTHALF, SQRTHALF - MAGIC45, SQRTHALF + MAGIC45,
                          MAGIC, 1.0, 0.0, 1.0, -MAGIC, 1.0, -SQRTHALF + MAGIC45, SQRTHALF + MAGIC45, -SQRTHALF, SQRTHALF,
                          -SQRTHALF - MAGIC45, SQRTHALF - MAGIC45, -1.0, MAGIC, -1.0, 0.0, -1.0, -MAGIC,
                          -SQRTHALF - MAGIC45, -SQRTHALF + MAGIC45, -SQRTHALF, -SQRTHALF, -SQRTHALF + MAGIC45,
                          -SQRTHALF - MAGIC45, -MAGIC, -1.0, 0.0, -1.0, 0.0, -1.0};
    std::vector<unsigned char> codes(26, CURVE4);
    codes.front() = MOVETO;
    codes.back() = CLOSEPOLY;
    return pathOf(std::move(v), std::move(codes), false, 12);
}

}  // namespace

const Path& unitCirclePath() { return unitCircle(); }

namespace {

// lines.py's line style names and their long forms
std::string shortLinestyle(const std::string& ls) {
    if (ls == "solid") return "-";
    if (ls == "dashed") return "--";
    if (ls == "dashdot") return "-.";
    if (ls == "dotted") return ":";
    return ls;
}

// The unit polygons and star NumPy's sin and cos make (data/marker_paths.json).
const Path& markerData(const std::string& name) {
    static std::map<std::string, Path> paths = [] {
        std::map<std::string, Path> out;
        const auto doc = support::parseJson(embedded::file("data/marker_paths.json"), nullptr);
        if (!doc || !doc->find("paths")) return out;
        for (const auto& [key, value] : doc->find("paths")->members()) {
            std::vector<double> v;
            for (const auto& x : value.find("vertices")->items()) v.push_back(std::strtod(x.asString().c_str(), nullptr));
            std::vector<unsigned char> codes;
            for (const auto& c : value.find("codes")->items()) codes.push_back(static_cast<unsigned char>(c.asInteger()));
            out.emplace(key, Path::make(std::move(v), std::move(codes), rc::Params::defaults()));
        }
        return out;
    }();
    return paths.at(name);
}

}  // namespace

const Path& unitStarPath() { return markerData("star5_hatch"); }

const Path& unitPolygon20Path() { return markerData("polygon20"); }

namespace {

// markers.py's constant from NumPy's cos (data/marker_paths.json)
double markerConstant(const std::string& name) {
    static const std::map<std::string, double> constants = [] {
        std::map<std::string, double> out;
        const auto doc = support::parseJson(embedded::file("data/marker_paths.json"), nullptr);
        if (doc && doc->find("constants"))
            for (const auto& [key, value] : doc->find("constants")->members())
                out.emplace(key, std::strtod(value.asString().c_str(), nullptr));
        return out;
    }();
    return constants.at(name);
}

// Path(verts) of the points of a marker path at these indices, and of given points ({x, y} pairs)
struct HalfBuilder {
    const Path& from;
    std::vector<double> v;
    HalfBuilder& at(std::initializer_list<int> indices) {
        for (int i : indices) v.push_back((*from.vertices)[std::size_t(2 * i)]), v.push_back((*from.vertices)[std::size_t(2 * i + 1)]);
        return *this;
    }
    HalfBuilder& range(int begin, int end) {  // verts[begin:end]
        for (int i = begin; i < end; ++i) at({i});
        return *this;
    }
    HalfBuilder& point(double x, double y) {
        v.push_back(x), v.push_back(y);
        return *this;
    }
    Path make(const rc::Params& rc) const { return Path::make(v, {}, rc); }
};

}  // namespace

bool markerFromJson(const support::Json& value, std::string* marker, std::string* error) {
    static const char* const codes[12] = {"tickleft",  "tickright",  "tickup",        "tickdown",
                                          "caretleft", "caretright", "caretup",       "caretdown",
                                          "caretleftbase", "caretrightbase", "caretupbase", "caretdownbase"};
    if (value.isString()) {
        for (const char* c : codes)
            if (value.asString() == c) return *error = "Unrecognized marker style " + svg::pythonStrRepr(c), false;
        *marker = value.asString();
        return true;
    }
    double v = 0;
    if (value.isBool()) v = value.asBool() ? 1 : 0;
    else if (value.isNumber()) v = value.asNumber();
    else return *error = "marker must be a string or a number from 0 to 11", false;
    if (v == std::floor(v) && v >= 0 && v <= 11) {
        *marker = codes[static_cast<int>(v)];
        return true;
    }
    *error = "Unrecognized marker style " + (value.isInteger() ? value.numberText() : support::pythonRepr(v));
    return false;
}

bool MarkerStyle::set(const std::string& marker, const std::optional<std::string>& fillstyle, const rc::Params& rc,
                      std::string* error) {
    fillstyle_ = fillstyle ? *fillstyle : rc.text("markers.fillstyle");
    marker_ = marker;
    // _recache
    path_ = Path();  // _empty_path
    transforms::Affine2D t;
    snapThreshold_.reset();
    joinstyle_ = "round";
    capstyle_ = "butt";
    filled_ = fillstyle_ != "none";
    altPath_.reset();
    const std::string& fs = fillstyle_;
    const bool half = fs == "left" || fs == "right" || fs == "top" || fs == "bottom";  // _half_fill
    // the alt transform: the marker's own (alt, the same object, follows it) or a frozen copy
    bool altFollows = false;
    transforms::Affine2D altT;
    // the angle by fill style, in the order right, top, left, bottom
    const auto angle = [&](double right, double top, double left, double bottom) {
        return fs == "right" ? right : fs == "top" ? top : fs == "left" ? left : bottom;
    };
    // the paths by fill style: (top, bottom), (bottom, top), (left, right), (right, left)
    const auto halves = [&](const Path& top, const Path& bottom, const Path& left, const Path& right) {
        if (fs == "top") path_ = top, altPath_ = bottom;
        else if (fs == "bottom") path_ = bottom, altPath_ = top;
        else if (fs == "left") path_ = left, altPath_ = right;
        else path_ = right, altPath_ = left;
        altFollows = true;
    };
    const double inf = std::numeric_limits<double>::infinity();
    const auto triangle = [&](double rot, int skip) {
        t.scale(0.5).rotateDeg(rot);
        snapThreshold_ = 5.0;
        if (!half) {
            path_ = trianglePath();
        } else {
            const Path* mpaths[4] = {&triangleUPath(), &triangleLPath(), &triangleDPath(), &triangleRPath()};
            const int k = fs == "top" ? 0 : fs == "bottom" ? 2 : fs == "left" ? 1 : 3;
            path_ = *mpaths[(k + skip) % 4];
            altPath_ = *mpaths[(k + 2 + skip) % 4];
            altFollows = true;
        }
        joinstyle_ = "miter";
    };
    const auto tri = [&](std::optional<double> rot) {
        t.scale(0.5);
        snapThreshold_ = 5.0;
        filled_ = false;
        path_ = triPath();
        if (rot) t.rotateDeg(*rot);
    };
    const auto caret = [&](std::optional<double> rot, bool base) {
        t.scale(0.5);
        snapThreshold_ = 3.0;
        filled_ = false;
        path_ = base ? caretPathBase() : caretPath();
        joinstyle_ = "miter";
        if (rot) t.rotateDeg(*rot);
    };
    // The integer markers TICKLEFT (0) ... CARETDOWNBASE (11) go by their names here: the strings
    // "1" ... "4" and "8" are other markers.
    if (isNone()) {  // _set_nothing
        filled_ = false;
    } else if (marker == "o" || marker == ".") {  // _set_circle, _set_point
        t.scale(0.5 * (marker == "." ? 0.5 : 1.0));
        snapThreshold_ = inf;
        if (!half) {
            path_ = unitCircle().madeWith(rc);
        } else {
            path_ = unitCircleRightHalf().madeWith(rc);
            altPath_ = path_;
            t.rotateDeg(angle(0, 90, 180, 270));
            altT = transforms::Affine2D(t.matrix());
            altT.rotateDeg(180.);
        }
    } else if (marker == ",") {  // _set_pixel
        path_ = Path::unitRectangle(rc);
        t.translate(-0.49999, -0.49999);
    } else if (marker == "^") triangle(0.0, 0);
    else if (marker == "v") triangle(180.0, 2);
    else if (marker == "<") triangle(90.0, 3);
    else if (marker == ">") triangle(270.0, 1);
    else if (marker == "s") {  // _set_square
        t.translate(-0.5, -0.5);
        snapThreshold_ = 2.0;
        if (!half) {
            path_ = Path::unitRectangle(rc);
        } else {
            path_ = Path::make({0.0, 0.0, 1.0, 0.0, 1.0, 0.5, 0.0, 0.5, 0.0, 0.0}, {}, rc);
            altPath_ = Path::make({0.0, 0.5, 1.0, 0.5, 1.0, 1.0, 0.0, 1.0, 0.0, 0.5}, {}, rc);
            t.rotateDeg(fs == "bottom" ? 0 : fs == "right" ? 90 : fs == "top" ? 180 : 270);
            altFollows = true;
        }
        joinstyle_ = "miter";
    } else if (marker == "D" || marker == "d") {  // _set_diamond, _set_thin_diamond
        t.translate(-0.5, -0.5).rotateDeg(45);
        snapThreshold_ = 5.0;
        if (!half) {
            path_ = Path::unitRectangle(rc);
        } else {
            path_ = Path::make({0, 0, 1, 0, 1, 1, 0, 0}, {}, rc);
            altPath_ = Path::make({0, 0, 0, 1, 1, 1, 0, 0}, {}, rc);
            t.rotateDeg(angle(0, 90, 180, 270));
            altFollows = true;
        }
        joinstyle_ = "miter";
        if (marker == "d") t.scale(0.6, 1.0);
    } else if (marker == "p") {  // _set_pentagon
        t.scale(0.5);
        snapThreshold_ = 5.0;
        const Path polypath = markerData("polygon5");
        if (!half) {
            path_ = polypath.madeWith(rc);
        } else {
            const double y = (1 + std::sqrt(5.0)) / 4.;
            halves(HalfBuilder{polypath, {}}.at({0, 1, 4, 0}).make(rc), HalfBuilder{polypath, {}}.at({1, 2, 3, 4, 1}).make(rc),
                   HalfBuilder{polypath, {}}.at({0, 1, 2}).point(0, -y).at({0}).make(rc),
                   HalfBuilder{polypath, {}}.at({0, 4, 3}).point(0, -y).at({0}).make(rc));
        }
        joinstyle_ = "miter";
    } else if (marker == "*") {  // _set_star
        t.scale(0.5);
        snapThreshold_ = 5.0;
        const Path polypath = markerData("star5");
        if (!half) {
            path_ = polypath.madeWith(rc);
        } else {
            halves(HalfBuilder{polypath, {}}.range(0, 4).range(7, 10).at({0}).make(rc),
                   HalfBuilder{polypath, {}}.range(3, 8).at({3}).make(rc), HalfBuilder{polypath, {}}.range(0, 6).at({0}).make(rc),
                   HalfBuilder{polypath, {}}.at({0}).range(5, 10).at({0}).make(rc));
        }
        joinstyle_ = "bevel";
    } else if (marker == "h") {  // _set_hexagon1
        t.scale(0.5);
        const Path polypath = markerData("polygon6");
        if (!half) {
            path_ = polypath.madeWith(rc);
        } else {
            const double x = markerConstant("hexagon1_half_x");
            halves(HalfBuilder{polypath, {}}.point(-x, 0).at({1, 0, 5}).point(x, 0).make(rc),
                   HalfBuilder{polypath, {}}.point(-x, 0).range(2, 5).point(x, 0).make(rc), HalfBuilder{polypath, {}}.range(0, 4).make(rc),
                   HalfBuilder{polypath, {}}.at({0, 5, 4, 3}).make(rc));
        }
        joinstyle_ = "miter";
    } else if (marker == "H") {  // _set_hexagon2
        t.scale(0.5).rotateDeg(30);
        const Path polypath = markerData("polygon6");
        if (!half) {
            path_ = polypath.madeWith(rc);
        } else {
            const double x = std::sqrt(3.0) / 4, y = 3 / 4.;
            halves(HalfBuilder{polypath, {}}.at({1, 0, 5, 4, 1}).make(rc), HalfBuilder{polypath, {}}.range(1, 5).make(rc),
                   HalfBuilder{polypath, {}}.point(x, y).range(0, 3).point(-x, -y).point(x, y).make(rc),
                   HalfBuilder{polypath, {}}.point(x, y).at({5, 4, 3}).point(-x, -y).make(rc));
        }
        joinstyle_ = "miter";
    } else if (marker == "8") {  // _set_octagon
        t.scale(0.5);
        snapThreshold_ = 5.0;
        if (!half) {
            t.rotateDeg(22.5);
            path_ = markerData("polygon8").madeWith(rc);
        } else {
            const double x = std::sqrt(2.) / 4.;
            path_ = Path::make({0, -1, 0, 1, -x, 1, -1, x, -1, -x, -x, -1, 0, -1}, {}, rc);
            altPath_ = path_;
            t.rotateDeg(fs == "left" ? 0 : fs == "bottom" ? 90 : fs == "right" ? 180 : 270);
            altT = transforms::Affine2D(t.matrix());
            altT.rotateDeg(180.0);
        }
        joinstyle_ = "miter";
    } else if (marker == "|" || marker == "_") {  // _set_vline, _set_hline
        t.scale(0.5);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = lineMarkerPath();
        if (marker == "_") t.rotateDeg(90);
    } else if (marker == "tickleft" || marker == "tickright") {
        t.scale(marker == "tickleft" ? -1.0 : 1.0, 1.0);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = tickhorizPath();
    } else if (marker == "tickup" || marker == "tickdown") {
        t.scale(1.0, marker == "tickup" ? 1.0 : -1.0);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = tickvertPath();
    } else if (marker == "1") tri(std::nullopt);
    else if (marker == "2") tri(180.0);
    else if (marker == "3") tri(270.0);
    else if (marker == "4") tri(90.0);
    else if (marker == "caretdown") caret(std::nullopt, false);
    else if (marker == "caretup") caret(180.0, false);
    else if (marker == "caretleft") caret(270.0, false);
    else if (marker == "caretright") caret(90.0, false);
    else if (marker == "caretdownbase") caret(std::nullopt, true);
    else if (marker == "caretupbase") caret(180.0, true);
    else if (marker == "caretleftbase") caret(270.0, true);
    else if (marker == "caretrightbase") caret(90.0, true);
    else if (marker == "+") {  // _set_plus
        t.scale(0.5);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = plusPath();
    } else if (marker == "x") {  // _set_x
        t.scale(0.5);
        snapThreshold_ = 3.0;
        filled_ = false;
        path_ = xPath();
    } else if (marker == "P" || marker == "X") {  // _set_plus_filled, _set_x_filled
        snapThreshold_ = 5.0;
        joinstyle_ = "miter";
        if (!half) {
            path_ = marker == "P" ? plusFilledPath() : xFilledPath();
        } else {  // the top half, turned
            path_ = marker == "P" ? plusFilledTopPath() : xFilledTopPath();
            altPath_ = path_;
            t.rotateDeg(fs == "top" ? 0 : fs == "left" ? 90 : fs == "bottom" ? 180 : 270);
            altT = transforms::Affine2D(t.matrix());
            altT.rotateDeg(180);
        }
    } else {
        *error = "marker '" + marker + "' is not supported yet";
        return false;
    }
    transform_ = t.matrix();
    altTransform_ = altFollows ? t.matrix() : altT.matrix();
    return true;
}

Line2D::Line2D(const rc::Params& rc, std::vector<double> xdata, std::vector<double> ydata)
    : Artist(rc),
      color(rc.text("lines.color")),
      markeredgecolor(rc.text("lines.markeredgecolor")),
      markerfacecolor(rc.text("lines.markerfacecolor")),
      markersize(rc.number("lines.markersize")),
      antialiased(rc.boolean("lines.antialiased")),
      dashCapstyle(rc.text("lines.dash_capstyle")),
      dashJoinstyle(rc.text("lines.dash_joinstyle")),
      solidCapstyle(rc.text("lines.solid_capstyle")),
      solidJoinstyle(rc.text("lines.solid_joinstyle")) {
    zorder = 2;
    std::string error;
    setLinewidth(rc.number("lines.linewidth"));
    setLinestyle(rc.text("lines.linestyle"), &error);
    setMarker(rc.text("lines.marker"), &error);
    markeredgewidth = rc.number("lines.markeredgewidth");
    setData(std::move(xdata), std::move(ydata));
}

bool Line2D::setMarker(const std::string& marker, std::string* error) {
    return markerStyle.set(marker, std::nullopt, rc_, error);
}

bool Line2D::setLinestyle(const std::string& ls, std::string* error) {
    std::string style = (ls == " " || ls.empty() || ls == "none") ? "None" : shortLinestyle(ls);
    if (style != "-" && style != "--" && style != "-." && style != ":" && style != "None") {
        *error = "line style '" + ls + "' is not supported yet";
        return false;
    }
    linestyle_ = style;
    const std::string named = style == "-" ? "solid" : style == "--" ? "dashed" : style == "-." ? "dashdot" : style == ":" ? "dotted" : "None";
    unscaledDashes_ = dashPattern(named, rc_);
    dashes_ = scaleDashes(unscaledDashes_, linewidth_, rc_);
    return true;
}

void Line2D::setDashPattern(double offset, std::optional<std::vector<double>> onoff) {
    linestyle_ = "--";
    // (the offset comes within one cycle of the dashes, as _get_dash_pattern brings it)
    unscaledDashes_ = {offset, std::move(onoff)};
    dashes_ = scaleDashes(unscaledDashes_, linewidth_, rc_);
}

void Line2D::setLinewidth(double w) {
    linewidth_ = w;
    dashes_ = scaleDashes(unscaledDashes_, linewidth_, rc_);
}

void Line2D::setData(std::vector<double> x, std::vector<double> y) {
    x_ = std::move(x);
    y_ = std::move(y);
    invalid_ = true;
}
void Line2D::setXdata(std::vector<double> x) {
    x_ = std::move(x);
    invalid_ = true;
}
void Line2D::setYdata(std::vector<double> y) {
    y_ = std::move(y);
    invalid_ = true;
}

namespace {

// _path.is_sorted_and_has_non_nan: ascending where not NaN, and not all NaN
bool isSortedAndHasNonNan(const std::vector<double>& x) {
    double last = -std::numeric_limits<double>::infinity();
    bool found = false;
    for (double v : x) {
        if (v == v) {
            found = true;
            if (v < last) return false;
            last = v;
        }
    }
    return found;
}

// np.interp(indices[nan], indices[~nan], x[~nan]): the NaNs of x filled along the index
std::vector<double> filledByIndex(const std::vector<double>& x) {
    std::vector<std::size_t> known;
    for (std::size_t i = 0; i < x.size(); ++i)
        if (!std::isnan(x[i])) known.push_back(i);
    std::vector<double> out = x;
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!std::isnan(x[i])) continue;
        const auto after = std::upper_bound(known.begin(), known.end(), i);
        if (after == known.begin()) {
            out[i] = x[known.front()];  // left of the known points: the first one
        } else if (after == known.end()) {
            out[i] = x[known.back()];   // right of them: the last one
        } else {
            const std::size_t a = *(after - 1), b = *after;
            const double xa = double(a), xb = double(b), ya = x[a], yb = x[b];
            const double slope = (yb - ya) / (xb - xa);
            double v = slope * (double(i) - xa) + ya;
            if (std::isnan(v)) {  // numpy tries the other end, then the equal ends
                v = slope * (double(i) - xb) + yb;
                if (std::isnan(v) && ya == yb) v = ya;
            }
            out[i] = v;
        }
    }
    return out;
}

}  // namespace

void Line2D::setDrawstyle(const std::string& style) {
    if (drawstyle_ != style) invalid_ = true;
    drawstyle_ = style;
}

void Line2D::recache() {
    // np.column_stack(np.broadcast_arrays(x, y))
    xy_.clear();
    const std::size_t n = x_.size() == 1 ? y_.size() : x_.size();
    for (std::size_t i = 0; i < n; ++i) {
        xy_.push_back(x_.size() == 1 ? x_[0] : x_[i]);
        xy_.push_back(y_.size() == 1 ? y_[0] : y_[i]);
    }
    // A long line with sorted x on a linear x axis, clipped, in data coordinates and with markers on
    // every point, is drawn in the slice of points the view shows (_subslice; the condition is taken
    // here, when the path is made).
    subslice_ = axes && !axes->polar && x_.size() > 1000 && isSortedAndHasNonNan(x_) && axes->xaxis->scale() == "linear" && !markevery &&
                clipOn && transforms::equal(*transform, *axes->transData);
    if (subslice_) {
        std::vector<double> x(n);
        for (std::size_t i = 0; i < n; ++i) x[i] = xy_[2 * i];
        xFilled_ = filledByIndex(x);
    }
    const int steps = path_.interpolationSteps;  // (the last path's, or 1)
    path_ = Path::make(vertices(0, n, true), {}, rc_);
    path_.interpolationSteps = steps;
    linePath_ = path_;
    invalid_ = false;
}

void Line2D::setInterpolationSteps(int steps) {
    if (invalid_) recache();  // get_path()
    path_.interpolationSteps = linePath_.interpolationSteps = steps;
}

std::vector<double> Line2D::vertices(std::size_t start, std::size_t stop, bool withDrawstyle) const {
    std::vector<double> out;
    const std::size_t n = stop - start;
    const auto x = [&](std::size_t i) { return xy_[2 * (start + i)]; };
    const auto y = [&](std::size_t i) { return xy_[2 * (start + i) + 1]; };
    if (!withDrawstyle || drawstyle_ == "default") {
        out.assign(xy_.begin() + std::ptrdiff_t(2 * start), xy_.begin() + std::ptrdiff_t(2 * stop));
    } else if (drawstyle_ == "steps-mid") {
        // pts_to_midstep: x0, then the midpoints twice each, then the last x; each y twice
        for (std::size_t i = 0; i < n; ++i) {
            const double left = i == 0 ? x(0) : (x(i - 1) + x(i)) / 2;
            const double right = i + 1 == n ? x(n - 1) : (x(i) + x(i + 1)) / 2;
            out.push_back(left), out.push_back(y(i));
            out.push_back(right), out.push_back(y(i));
        }
    } else {
        // pts_to_prestep ('steps', 'steps-pre'): between two points first the y step, then the x step;
        // pts_to_poststep: the other way round
        const bool pre = drawstyle_ != "steps-post";
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back(x(i)), out.push_back(y(i));
            if (i + 1 < n) {
                out.push_back(pre ? x(i) : x(i + 1));
                out.push_back(pre ? y(i + 1) : y(i));
            }
        }
    }
    return out;
}

void Line2D::updateFrom(const Line2D& o) {
    // Artist.update_from
    transform = o.transform;
    visible = o.visible;
    alpha = o.alpha;
    clipBox = o.clipBox;
    clipOn = o.clipOn;
    clipPath = o.clipPath;
    label = o.label;
    sketch = o.sketch;
    linestyle_ = o.linestyle_;
    linewidth_ = o.linewidth_;
    color = o.color;
    markersize = o.markersize;
    markerfacecolor = o.markerfacecolor;
    markerfacecoloralt = o.markerfacecoloralt;
    markeredgecolor = o.markeredgecolor;
    markeredgewidth = o.markeredgewidth;
    unscaledDashes_ = o.unscaledDashes_;
    dashes_ = o.dashes_;
    dashCapstyle = o.dashCapstyle;
    dashJoinstyle = o.dashJoinstyle;
    solidCapstyle = o.solidCapstyle;
    solidJoinstyle = o.solidJoinstyle;
    markerStyle = o.markerStyle;
    drawstyle_ = o.drawstyle_;  // (update_from sets it without a recache)
}

const Path& Line2D::path() {
    if (invalid_) recache();
    return path_;
}

namespace {
bool isAuto(const colors::Spec& c, bool anyCase) {
    const auto* s = std::get_if<std::string>(&c);
    if (!s) return false;
    if (!anyCase) return *s == "auto";
    std::string lower = *s;
    for (char& ch : lower) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return lower == "auto";
}
}  // namespace

colors::Spec Line2D::markerEdgeColor() const {
    if (!isAuto(markeredgecolor, false)) return markeredgecolor;
    // 'auto' is the line's colour; in the classic style it is black for a filled marker, except the
    // point and the pixel
    if (rc_.boolean("_internal.classic_mode") && markerStyle.marker() != "." && markerStyle.marker() != "," && markerStyle.filled() &&
        markerStyle.fillstyle() != "none")
        return std::string("k");
    return color;
}

colors::Spec Line2D::markerFaceColor(bool alt) const {
    if (markerStyle.fillstyle() == "none") return std::string("none");
    const colors::Spec& fc = alt ? markerfacecoloralt : markerfacecolor;
    if (isAuto(fc, true)) return color;
    return fc;
}

// _mark_every_path(markevery, tpath, affine, ax): the points of the (non-affine transformed) path that get
// markers; an error matplotlib raises goes into markevery.error
Path Line2D::markEveryPath(Markevery& every, const Path& tpath, const transforms::Matrix& affine) const {
    const auto& all = *tpath.vertices;
    const auto n = static_cast<long long>(tpath.size());
    std::vector<double> v;
    const auto take = [&](long long k) { v.push_back(all[std::size_t(2 * k)]), v.push_back(all[std::size_t(2 * k + 1)]); };
    switch (every.kind) {
    case Markevery::Kind::Slice: {  // verts[slice(start, None, step)]
        if (every.step == 0) {
            every.error = "slice step cannot be zero";
            return tpath;
        }
        long long i = every.start;
        if (every.step > 0) {
            if (i < 0) i = std::max(i + n, 0LL);
            for (; i < n; i += every.step) take(i);
        } else {
            if (i < 0) i += n;
            if (i >= n) i = n - 1;
            for (; i >= 0; i += every.step) take(i);
        }
        break;
    }
    case Markevery::Kind::Indices:
        for (long long i : every.indices) {
            const long long k = i < 0 ? i + n : i;
            if (k < 0 || k >= n) return every.error = "markevery=" + every.repr + " is iterable but not a valid numpy fancy index", tpath;
            take(k);
        }
        break;
    case Markevery::Kind::Mask:
        if ((long long)every.mask.size() != n)
            return every.error = "markevery=" + every.repr + " is iterable but not a valid numpy fancy index", tpath;
        for (long long k = 0; k < n; ++k)
            if (every.mask[std::size_t(k)]) take(k);
        break;
    case Markevery::Kind::Along: {
        // the distance along the path in display coordinates, cumulated, of its finite points
        std::vector<std::array<double, 2>> fverts, disp;
        for (long long k = 0; k < n; ++k) {
            const double x = all[std::size_t(2 * k)], y = all[std::size_t(2 * k + 1)];
            if (std::isfinite(x) && std::isfinite(y)) fverts.push_back({x, y}), disp.push_back(transforms::apply(affine, x, y));
        }
        if (fverts.empty()) break;  // (delta[-1] of nothing: matplotlib's IndexError is not reached, as tpath has points)
        std::vector<double> delta(disp.size());
        double sum = 0;
        for (std::size_t k = 0; k < disp.size(); ++k) {
            const double d = k == 0 ? 0.0 : std::hypot(disp[k][0] - disp[k - 1][0], disp[k][1] - disp[k - 1][1]);
            delta[k] = sum += d;  // np.hypot(*delta.T).cumsum()
        }
        // the Axes' diagonal is a distance of one
        const auto m = axes->transAxes->matrix();
        const auto p0 = transforms::apply(m, 0, 0), p1 = transforms::apply(m, 1, 1);
        const double scale = std::hypot(p1[0] - p0[0], p1[1] - p0[1]);
        const double markerStart = every.startAlong * scale, markerStep = every.stepAlong * scale;
        if (markerStep <= 0) {
            every.error = "'markevery' step must be positive, but got " + support::pythonRepr(every.stepAlong);
            return tpath;
        }
        // the candidate markers: those just before and after each vertex
        std::vector<double> md;
        for (double d : delta) md.push_back(d - support::divmod(d - markerStart, markerStep).second);
        const std::size_t count = md.size();
        for (std::size_t k = 0; k < count; ++k) md.push_back(md[k] + markerStep);
        std::sort(md.begin(), md.end(), [](double a, double b) { return a < b || (std::isnan(b) && !std::isnan(a)); });
        md.erase(std::unique(md.begin(), md.end(), [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); }), md.end());
        std::vector<double> kept;
        for (double d : md)
            if (d >= markerStart && d < delta.back()) kept.push_back(d);
        // each candidate's closest point (the first of several at one distance)
        const auto left = [&](double key) { return std::size_t(std::lower_bound(delta.begin(), delta.end(), key) - delta.begin()); };
        std::vector<std::size_t> inds;
        for (double d : kept) {
            std::size_t right = left(d);
            const std::size_t lo = right > 0 ? right - 1 : 0;
            right = std::min(right, delta.size() - 1);
            const std::size_t k = std::fabs(delta[right] - d) < std::fabs(d - delta[lo]) ? right : lo;
            inds.push_back(left(delta[k]));
        }
        std::sort(inds.begin(), inds.end());
        inds.erase(std::unique(inds.begin(), inds.end()), inds.end());
        for (std::size_t k : inds) v.push_back(fverts[k][0]), v.push_back(fverts[k][1]);
        break;
    }
    }
    return Path::make(std::move(v), {}, rc_);
}

void Line2D::draw(Renderer& renderer) {
    if (!visible) return;
    if (invalid_) recache();
    renderer.openGroup("line2d", gid);
    // A subslice: the points from one before the first in the view (get_xbound) to one after the last
    const std::size_t points = xy_.size() / 2;
    std::size_t start = 0, stop = points;
    if (subslice_ && axes) {
        const auto view = axes->xaxis->viewInterval();
        const double x0 = std::min(view[0], view[1]), x1 = std::max(view[0], view[1]);
        const auto i0 = std::size_t(std::lower_bound(xFilled_.begin(), xFilled_.end(), x0) - xFilled_.begin());
        const auto i1 = std::size_t(std::upper_bound(xFilled_.begin(), xFilled_.end(), x1) - xFilled_.begin());
        start = i0 > 0 ? i0 - 1 : 0;
        stop = std::min(i1 + 1, points);
    }
    const auto made = [&](bool withDrawstyle) {  // _transform_path(subslice): a new Path of those points
        Path p = Path::make(vertices(start, stop, withDrawstyle), {}, rc_);
        p.interpolationSteps = path_.interpolationSteps;
        return p;
    };
    // get_transformed_path_and_affine: the non-affine part applied to the path, the affine part apart
    const transforms::Matrix affine = transform->matrix();
    const Path tpath = transformPathNonAffine(subslice_ && axes ? made(true) : linePath_, *transform);
    // markers are drawn at the points themselves, whatever the draw style
    const Path markerPoints = drawstyle_ == "default" ? tpath : transformPathNonAffine(made(false), *transform);
    if (linestyle_ != "None" && tpath.size()) {
        GC gc = renderer.newGc();
        setGcClip(gc);
        gc.url = url;
        gc.antialiased = antialiased;
        gc.linewidth = linewidth_;
        gc.joinstyle = isDashed() ? dashJoinstyle : solidJoinstyle;
        gc.capstyle = isDashed() ? dashCapstyle : solidCapstyle;
        gc.snap = snap;
        if (sketch) gc.sketch = sketch;
        const auto rgba = *colors::toRgba(color, alpha, rc_, nullptr);
        gc.setForeground(std::vector<double>(rgba.begin(), rgba.end()));
        if (const char* problem = linewidth_ > 0 ? gc.setDashes(dashes_.first, dashes_.second) : gc.setDashes(0, std::nullopt))
            renderer.unsupported(problem);
        renderer.drawPath(gc, tpath, affine, std::nullopt);
    }
    if (!markerStyle.isNone() && markersize > 0) {
        GC gc = renderer.newGc();
        setGcClip(gc);
        gc.url = url;
        gc.linewidth = markeredgewidth;
        gc.antialiased = antialiased;
        auto ec = *colors::toRgba(markerEdgeColor(), alpha, rc_, nullptr);
        const auto fc = *colors::toRgba(markerFaceColor(false), alpha, rc_, nullptr);
        const auto fcalt = *colors::toRgba(markerFaceColor(true), alpha, rc_, nullptr);
        // markeredgecolor 'auto' with a face: the edge takes the face's alpha (get_markerfacecolor()
        // is 'none' for unfilled marker styles)
        if (isAuto(markeredgecolor, false) && !colors::isNone(markerFaceColor(false))) ec[3] = fc[3];
        gc.setForeground(std::vector<double>(ec.begin(), ec.end()));
        if (sketch) gc.sketch = std::array<double, 3>{(*sketch)[0] / 2, (*sketch)[1] / 2, 2 * (*sketch)[2]};
        if (markerPoints.size()) {
            const auto threshold = markerStyle.snapThreshold();
            if (threshold) gc.snap = renderer.pointsToPixels(markersize) >= *threshold;
            else gc.snap.reset();
            gc.joinstyle = markerStyle.joinstyle();
            gc.capstyle = markerStyle.capstyle();
            transforms::Affine2D markerTrans(markerStyle.transform());
            const double w = renderer.pointsToPixels(markersize);
            if (markerStyle.marker() == ",") gc.linewidth = 0;
            else markerTrans.scale(w);
            Path subsampled = markerPoints;
            if (markevery) {
                if (!markevery->error.empty()) {
                    support::noteRaised(markevery->error);
                    return;
                }
                if (markerPoints.codes) renderer.unsupported("markevery on paths with codes is not supported yet");
                subsampled = markEveryPath(*markevery, markerPoints, affine);
                if (!markevery->error.empty()) {
                    support::noteRaised(markevery->error);
                    return;
                }
            }
            renderer.drawMarkers(gc, markerStyle.path(), markerTrans.matrix(), subsampled, affine,
                                 std::vector<double>(fc.begin(), fc.end()));
            if (markerStyle.altPath()) {
                transforms::Affine2D altTrans(markerStyle.altTransform());
                altTrans.scale(w);
                renderer.drawMarkers(gc, *markerStyle.altPath(), altTrans.matrix(), subsampled, affine,
                                     std::vector<double>(fcalt.begin(), fcalt.end()));
            }
        }
        // The markers' recache under the 'default' draw style leaves its path as get_path() from now
        // on (only _drawstyle and _transformed_path are put back): what a legend placed 'best' avoids.
        if (drawstyle_ != "default") {
            const int steps = path_.interpolationSteps;
            path_ = Path::make(vertices(0, points, false), {}, rc_);
            path_.interpolationSteps = steps;
        }
    }
    renderer.closeGroup("line2d");
}

}  // namespace calculate_plot::engine
