// Derived from Matplotlib 3.11.2: lib/matplotlib/lines.py (Line2D) and lib/matplotlib/markers.py
// (MarkerStyle).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/lines.hpp"

#include "rc/rc_params.hpp"

namespace calculate_plot::engine {

namespace {

// The marker paths markers.py makes when it is imported (with the default rcParams).
const Path& tickvertPath() {
    static const Path p = Path::make({-0.0, 0.0, -0.0, 1.0}, {}, rc::Params::defaults());
    return p;
}
const Path& tickhorizPath() {
    static const Path p = Path::make({0.0, 0.0, 1.0, 0.0}, {}, rc::Params::defaults());
    return p;
}
const Path& lineMarkerPath() {
    static const Path p = Path::make({0.0, -1.0, 0.0, 1.0}, {}, rc::Params::defaults());
    return p;
}

transforms::Matrix scaled(double sx, double sy) {
    transforms::Affine2D a;
    a.scale(sx, sy);
    return a.matrix();
}

// lines.py's line style names and their long forms
std::string shortLinestyle(const std::string& ls) {
    if (ls == "solid") return "-";
    if (ls == "dashed") return "--";
    if (ls == "dashdot") return "-.";
    if (ls == "dotted") return ":";
    return ls;
}

}  // namespace

bool MarkerStyle::set(const std::string& marker, const std::optional<std::string>& fillstyle, const rc::Params& rc,
                      std::string* error) {
    fillstyle_ = fillstyle ? *fillstyle : rc.text("markers.fillstyle");
    marker_ = marker;
    // _recache
    path_ = Path();  // _empty_path
    transform_ = transforms::Matrix{};
    snapThreshold_.reset();
    joinstyle_ = "round";
    capstyle_ = "butt";
    filled_ = fillstyle_ != "none";
    if (isNone()) {  // _set_nothing
        filled_ = false;
        return true;
    }
    // The integer markers TICKLEFT (0) ... TICKDOWN (3) go by their names here: the strings
    // "1" ... "4" are other markers (tri_down ...).
    if (marker == "tickleft" || marker == "tickright") {
        transform_ = scaled(marker == "tickleft" ? -1.0 : 1.0, 1.0);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = tickhorizPath();
        return true;
    }
    if (marker == "tickup" || marker == "tickdown") {
        transform_ = scaled(1.0, marker == "tickup" ? 1.0 : -1.0);
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = tickvertPath();
        return true;
    }
    if (marker == "|" || marker == "_") {  // _set_vline, _set_hline
        transforms::Affine2D a;
        a.scale(0.5);
        if (marker == "_") a.rotateDeg(90);
        transform_ = a.matrix();
        snapThreshold_ = 1.0;
        filled_ = false;
        path_ = lineMarkerPath();
        return true;
    }
    *error = "marker '" + marker + "' is not supported yet";
    return false;
}

Line2D::Line2D(const rc::Params& rc, std::vector<double> xdata, std::vector<double> ydata)
    : Artist(rc),
      color(rc.text("lines.color")),
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
    markerfacecolor = rc.text("lines.markerfacecolor");
    markeredgecolor = rc.text("lines.markeredgecolor");
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

void Line2D::recache() {
    // np.column_stack(np.broadcast_arrays(x, y))
    std::vector<double> xy;
    const std::size_t n = x_.size() == 1 ? y_.size() : x_.size();
    for (std::size_t i = 0; i < n; ++i) {
        xy.push_back(x_.size() == 1 ? x_[0] : x_[i]);
        xy.push_back(y_.size() == 1 ? y_[0] : y_[i]);
    }
    path_ = Path::make(std::move(xy), {}, rc_);
    invalid_ = false;
}

std::string Line2D::markerEdgeColor() const {
    if (markeredgecolor == "auto") return color;  // (classic mode is not supported)
    return markeredgecolor;
}

std::string Line2D::markerFaceColor(bool alt) const {
    if (markerStyle.fillstyle() == "none") return "none";
    const std::string& fc = alt ? markerfacecoloralt : markerfacecolor;
    if (fc == "auto" || fc == "AUTO" || fc == "Auto") return color;
    return fc;
}

void Line2D::draw(Renderer& renderer) {
    if (!visible) return;
    if (invalid_) recache();
    renderer.openGroup("line2d", gid);
    const transforms::Matrix affine = transform->matrix();  // the transform is affine (linear scales)
    if (linestyle_ != "None" && path_.size()) {
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
        if (linewidth_ > 0) gc.setDashes(dashes_.first, dashes_.second);
        else gc.setDashes(0, std::nullopt);
        renderer.drawPath(gc, path_, affine, std::nullopt);
    }
    if (!markerStyle.isNone() && markersize > 0) {
        GC gc = renderer.newGc();
        setGcClip(gc);
        gc.url = url;
        gc.linewidth = markeredgewidth;
        gc.antialiased = antialiased;
        auto ec = *colors::toRgba(markerEdgeColor(), alpha, rc_, nullptr);
        const auto fc = *colors::toRgba(markerFaceColor(false), alpha, rc_, nullptr);
        if (markeredgecolor == "auto" && markerfacecolor != "none" && markerfacecolor != "None" &&
            markerfacecolor != "NONE")
            ec[3] = fc[3];
        gc.setForeground(std::vector<double>(ec.begin(), ec.end()));
        if (sketch) gc.sketch = std::array<double, 3>{(*sketch)[0] / 2, (*sketch)[1] / 2, 2 * (*sketch)[2]};
        if (path_.size()) {
            const auto threshold = markerStyle.snapThreshold();
            if (threshold) gc.snap = renderer.pointsToPixels(markersize) >= *threshold;
            else gc.snap.reset();
            gc.joinstyle = markerStyle.joinstyle();
            gc.capstyle = markerStyle.capstyle();
            transforms::Affine2D markerTrans(markerStyle.transform());
            const double w = renderer.pointsToPixels(markersize);
            if (markerStyle.marker() == ",") gc.linewidth = 0;
            else markerTrans.scale(w);
            renderer.drawMarkers(gc, markerStyle.path(), markerTrans.matrix(), path_, affine,
                                 std::vector<double>(fc.begin(), fc.end()));
        }
    }
    renderer.closeGroup("line2d");
}

}  // namespace calculate_plot::engine
