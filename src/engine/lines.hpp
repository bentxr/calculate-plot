// Derived from Matplotlib 3.11.2: lib/matplotlib/lines.py (Line2D), lib/matplotlib/markers.py
// (MarkerStyle) and lib/matplotlib/cbook.py (the step functions of STEP_LOOKUP_MAP).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"
#include "support/json.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;

// A marker as a figure file or the rcParams give it, as MarkerStyle::set takes it: a string, or one of
// the numbers 0 to 11 (markers.py's TICKLEFT ... CARETDOWNBASE, which its dict also finds as a bool or an
// equal float). The names MarkerStyle gives those numbers are no marker to matplotlib. false, with
// matplotlib's message, for what it does not recognise.
bool markerFromJson(const support::Json& value, std::string* marker, std::string* error);

// Path.unit_circle() and Path.unit_regular_star(5) (inner circle 0.5), as hatches take them.
const Path& unitCirclePath();
const Path& unitStarPath();

// MarkerStyle: a marker's path, its transform (unit size), and how it is stroked and filled.
class MarkerStyle {
public:
    // false (with *error) for markers not supported yet.
    bool set(const std::string& marker, const std::optional<std::string>& fillstyle, const rc::Params& rc,
             std::string* error);
    bool isNone() const { return marker_ == "none" || marker_ == "None" || marker_ == " " || marker_.empty(); }
    const std::string& marker() const { return marker_; }
    const Path& path() const { return path_; }
    const transforms::Matrix& transform() const { return transform_; }
    std::optional<double> snapThreshold() const { return snapThreshold_; }
    const std::string& joinstyle() const { return joinstyle_; }
    const std::string& capstyle() const { return capstyle_; }
    const std::string& fillstyle() const { return fillstyle_; }
    bool filled() const { return filled_; }

private:
    std::string marker_ = "none";
    std::string fillstyle_ = "full";
    Path path_;
    transforms::Matrix transform_;
    std::optional<double> snapThreshold_;
    std::string joinstyle_ = "round", capstyle_ = "butt";
    bool filled_ = true;
};

class Line2D : public Artist {
public:
    // Line2D(xdata, ydata) with the rcParams' defaults (lines.*).
    Line2D(const rc::Params& rc, std::vector<double> xdata, std::vector<double> ydata);

    bool setMarker(const std::string& marker, std::string* error);
    bool setLinestyle(const std::string& style, std::string* error);
    void setLinewidth(double w);
    // set_linestyle((offset, on-off sequence)): the line then counts as a dashed one ('--'), also
    // without a sequence
    void setDashPattern(double offset, std::optional<std::vector<double>> onoff);
    void setData(std::vector<double> x, std::vector<double> y);
    void setXdata(std::vector<double> x);
    void setYdata(std::vector<double> y);
    const Path& path();  // the data path (recached if the data changed)
    const std::string& linestyle() const { return linestyle_; }
    void draw(Renderer& renderer) override;
    // Line2D.update_from: the style (not the data, the antialiasing or markevery) and the Artist part
    void updateFrom(const Line2D& other);

    colors::Spec color;
    // set_markevery: which points get markers (_mark_every_path)
    struct Markevery {
        enum class Kind { Slice, Along, Indices, Mask };
        Kind kind = Kind::Indices;
        long long start = 0, step = 1;     // Slice: the points [start::step]
        double startAlong = 0, stepAlong = 0;  // Along: spaced along the line, in Axes diagonals
        std::vector<long long> indices;    // Indices: the points at these (from the end when negative)
        std::vector<bool> mask;            // Mask: the points where it is true
        std::string repr;                  // the value as Python writes it, for messages
        std::string error;                 // what matplotlib raises when it draws the line, if anything
    };
    std::optional<Markevery> markevery;
    colors::Spec markeredgecolor, markerfacecolor, markerfacecoloralt = std::string("none");  // or "auto"
    double markersize, markeredgewidth;
    bool antialiased;
    std::string dashCapstyle, dashJoinstyle, solidCapstyle, solidJoinstyle;
    // set_drawstyle: 'default', 'steps' (as 'steps-pre'), 'steps-pre', 'steps-mid' or 'steps-post'
    void setDrawstyle(const std::string& style);
    const std::string& drawstyle() const { return drawstyle_; }
    MarkerStyle markerStyle;
    // The Axes the line is in (Artist.axes), which decides whether a long line is drawn in a subslice.
    const Axes* axes = nullptr;

private:
    Path markEveryPath(Markevery& every, const Path& tpath, const transforms::Matrix& affine) const;
    bool isDashed() const { return linestyle_ == "--" || linestyle_ == "-." || linestyle_ == ":"; }
    colors::Spec markerEdgeColor() const;
    colors::Spec markerFaceColor(bool alt) const;
    void recache();

    // The points, as the path of the given draw style makes them: all of them, or [start, stop).
    std::vector<double> vertices(std::size_t start, std::size_t stop, bool withDrawstyle) const;

    std::vector<double> x_, y_;
    std::vector<double> xy_;       // the points (x0, y0, x1, y1 ...) after broadcasting
    bool subslice_ = false;        // Line2D._subslice
    std::vector<double> xFilled_;  // _x_filled: x with its NaNs interpolated
    std::string drawstyle_ = "default";
    Path path_;      // get_path()
    Path linePath_;  // the path the line is drawn with (its TransformedPath's), the draw style's
    bool invalid_ = true;
    double linewidth_ = 0;
    std::string linestyle_ = "-";
    std::pair<double, std::optional<std::vector<double>>> unscaledDashes_{0.0, std::nullopt}, dashes_{0.0, std::nullopt};
};

}  // namespace calculate_plot::engine
