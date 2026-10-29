// Derived from Matplotlib 3.11.2: lib/matplotlib/quiver.py (Quiver, QuiverKey, _parse_args).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/collections.hpp"
#include "engine/text.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;

// Quiver's keywords
struct QuiverOptions {
    std::optional<double> scale, width;
    double headwidth = 3, headlength = 5, headaxislength = 4.5, minshaft = 1, minlength = 1;
    std::string units = "width";
    std::optional<std::string> scaleUnits;
    std::string angles = "uv";                   // 'uv' or 'xy' (when anglesArray is empty)
    std::optional<std::vector<double>> anglesArray;  // angles in degrees, one per arrow
    std::string pivot = "tail";
};

// Quiver: a PolyCollection of arrows at the offsets (in data coordinates), whose shapes and transform (arrow width
// units to pixels) are made when it is drawn, from the Axes as they are then.
class Quiver : public PolyCollection {
public:
    // Quiver(ax, X, Y, U, V, C): the positions and the components, one per arrow (U, V and C masked where not finite).
    // False and matplotlib's message when it raises.
    static std::unique_ptr<Quiver> make(const rc::Params& rc, Axes& ax, std::vector<double> x, std::vector<double> y,
                                        std::vector<double> u, std::vector<double> v, std::optional<std::vector<double>> c,
                                        const QuiverOptions& options, std::string* error);

    // _make_verts(XY, U, V, angles): the arrows' vertices in width units, NaN for masked ones
    std::vector<std::vector<std::array<double, 2>>> makeVerts(const std::vector<std::array<double, 2>>& xy,
                                                              const std::vector<double>& u, const std::vector<double>& v,
                                                              const std::string& angles, const std::vector<bool>& mask);
    // _init: the transform, the span and the default width, and the scale at the first drawing
    void init();
    double dpiAtLastInit = -1;  // _dpi_at_last_init (none yet)
    QuiverOptions o;
    std::vector<std::array<double, 2>> xy;  // XY
    std::vector<double> u, v;
    std::vector<bool> umask;  // Umask (empty: nomask)
    double transScale = 1;    // _trans_scale
    Axes* ax() const { return &ax_; }

protected:
    void beforeDraw() override;

private:
    Quiver(const rc::Params& rc, Axes& ax);
    double dotsPerUnit(const std::string& units) const;
    Axes& ax_;
    double span_ = 0;
};

// QuiverKey: a labelled arrow drawn with the shape of the quiver's arrows, at a place in some coordinates
class QuiverKey : public Artist {
public:
    QuiverKey(const rc::Params& rc, Quiver& q, double x, double y, double u, std::string labelText);
    void draw(Renderer& renderer) override;

    double angle = 0;
    std::string coordinates = "axes";
    std::optional<colors::Spec> color;
    double labelsep = 0.1;  // inches
    std::string labelpos = "N";
    std::unique_ptr<Text> text;
    std::unique_ptr<PolyCollection> vector;

private:
    Quiver& q_;
    double x_, y_, u_;
};

// Barbs' keywords
struct BarbsOptions {
    double endy = 0;  // the pivot as a shift along the staff (pivot_points of 'tip' / 'middle', or a number)
    double length = 7;
    double spacing = 0.125, height = 0.4, width = 0.25, emptybarb = 0.15;  // sizes
    double half = 5, full = 10, flag = 50;                                 // barb_increments
    bool fillEmpty = false, rounding = true;
    std::vector<bool> flip{false};  // flip_barb: one for all, or one per barb
};

// Barbs: a PolyCollection of wind barbs, made at once (set_UVC), in points scaled by sqrt(length ** 2 / 4) at the
// offsets in data coordinates
class Barbs : public PolyCollection {
public:
    // Barbs(ax, X, Y, U, V, C): the points where X, Y, U, V or C is not finite left out. False and matplotlib's
    // message when it raises.
    static std::unique_ptr<Barbs> make(const rc::Params& rc, Axes& ax, std::vector<double> x, std::vector<double> y,
                                       std::vector<double> u, std::vector<double> v, std::optional<std::vector<double>> c,
                                       const BarbsOptions& options, std::string* error);

protected:
    void beforeDraw() override;  // _CollectionWithSizes.draw: the sizes at the figure's dpi

private:
    explicit Barbs(const rc::Params& rc);
    double barbSize_ = 0;
};

}  // namespace calculate_plot::engine
