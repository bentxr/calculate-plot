// Derived from Matplotlib 3.11.2: lib/matplotlib/lines.py (Line2D) and lib/matplotlib/markers.py
// (MarkerStyle).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

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
    void setData(std::vector<double> x, std::vector<double> y);
    void setXdata(std::vector<double> x);
    void setYdata(std::vector<double> y);
    void draw(Renderer& renderer) override;

    std::string color;
    std::string markeredgecolor, markerfacecolor, markerfacecoloralt = "none";  // or "auto"
    double markersize, markeredgewidth;
    bool antialiased;
    std::string dashCapstyle, dashJoinstyle, solidCapstyle, solidJoinstyle;
    std::string drawstyle = "default";
    MarkerStyle markerStyle;

private:
    bool isDashed() const { return linestyle_ == "--" || linestyle_ == "-." || linestyle_ == ":"; }
    std::string markerEdgeColor() const;
    std::string markerFaceColor(bool alt) const;
    void recache();

    std::vector<double> x_, y_;
    Path path_;
    bool invalid_ = true;
    double linewidth_ = 0;
    std::string linestyle_ = "-";
    std::pair<double, std::optional<std::vector<double>>> unscaledDashes_{0.0, std::nullopt}, dashes_{0.0, std::nullopt};
};

}  // namespace calculate_plot::engine
