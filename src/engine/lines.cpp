// Derived from Matplotlib 3.11.2: lib/matplotlib/lines.py (Line2D), lib/matplotlib/markers.py
// (MarkerStyle) and lib/matplotlib/cbook.py (the step functions of STEP_LOOKUP_MAP).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "support/pynum.hpp"
#include "engine/lines.hpp"

#include "engine/axes.hpp"
#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <map>

namespace calculate_plot::engine {

namespace {

// The marker paths markers.py makes when it is imported (with the default rcParams). The unit
// circle, rectangle, polygons and star are made by Path the first time they are asked for, under the
// rcParams of then: the figure's (MarkerStyle::set gives them those).
const Path& pathOf(std::vector<double> vertices, std::vector<unsigned char> codes, bool closed, int which) {
    static std::map<int, Path> cache;
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
    if (fillstyle_ != "full" && fillstyle_ != "none") {
        *error = "half-filled markers (fillstyle '" + fillstyle_ + "') are not supported yet";
        return false;
    }
    const double inf = std::numeric_limits<double>::infinity();
    const auto triangle = [&](double rot) {
        t.scale(0.5).rotateDeg(rot);
        snapThreshold_ = 5.0;
        path_ = trianglePath();
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
        path_ = unitCircle().madeWith(rc);
    } else if (marker == ",") {  // _set_pixel
        path_ = Path::unitRectangle(rc);
        t.translate(-0.49999, -0.49999);
    } else if (marker == "^") triangle(0.0);
    else if (marker == "v") triangle(180.0);
    else if (marker == "<") triangle(90.0);
    else if (marker == ">") triangle(270.0);
    else if (marker == "s") {  // _set_square
        t.translate(-0.5, -0.5);
        snapThreshold_ = 2.0;
        path_ = Path::unitRectangle(rc);
        joinstyle_ = "miter";
    } else if (marker == "D" || marker == "d") {  // _set_diamond, _set_thin_diamond
        t.translate(-0.5, -0.5).rotateDeg(45);
        snapThreshold_ = 5.0;
        path_ = Path::unitRectangle(rc);
        joinstyle_ = "miter";
        if (marker == "d") t.scale(0.6, 1.0);
    } else if (marker == "p") {  // _set_pentagon
        t.scale(0.5);
        snapThreshold_ = 5.0;
        path_ = markerData("polygon5").madeWith(rc);
        joinstyle_ = "miter";
    } else if (marker == "*") {  // _set_star
        t.scale(0.5);
        snapThreshold_ = 5.0;
        path_ = markerData("star5").madeWith(rc);
        joinstyle_ = "bevel";
    } else if (marker == "h" || marker == "H") {  // _set_hexagon1, _set_hexagon2
        t.scale(0.5);
        if (marker == "H") t.rotateDeg(30);
        path_ = markerData("polygon6").madeWith(rc);
        joinstyle_ = "miter";
    } else if (marker == "8") {  // _set_octagon
        t.scale(0.5);
        snapThreshold_ = 5.0;
        t.rotateDeg(22.5);
        path_ = markerData("polygon8").madeWith(rc);
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
        path_ = marker == "P" ? plusFilledPath() : xFilledPath();
    } else {
        *error = "marker '" + marker + "' is not supported yet";
        return false;
    }
    transform_ = t.matrix();
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
    subslice_ = axes && x_.size() > 1000 && isSortedAndHasNonNan(x_) && axes->xaxis->scale() == "linear" && !markevery &&
                clipOn && transforms::equal(*transform, *axes->transData);
    if (subslice_) {
        std::vector<double> x(n);
        for (std::size_t i = 0; i < n; ++i) x[i] = xy_[2 * i];
        xFilled_ = filledByIndex(x);
    }
    path_ = Path::make(vertices(0, n, true), {}, rc_);
    linePath_ = path_;
    invalid_ = false;
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
            if (markevery) {  // _mark_every_path with a list of indices
                std::vector<double> v;
                const auto& all = *markerPoints.vertices;
                const long long n = (long long)markerPoints.size();
                for (long long i : *markevery) {
                    const long long k = i < 0 ? i + n : i;
                    if (k < 0 || k >= n) {
                        renderer.unsupported("markevery index out of range");
                        continue;
                    }
                    v.push_back(all[std::size_t(2 * k)]), v.push_back(all[std::size_t(2 * k + 1)]);
                }
                subsampled = Path::make(std::move(v), {}, rc_);
                if (markerPoints.codes) renderer.unsupported("markevery on paths with codes is not supported yet");
            }
            renderer.drawMarkers(gc, markerStyle.path(), markerTrans.matrix(), subsampled, affine,
                                 std::vector<double>(fc.begin(), fc.end()));
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
