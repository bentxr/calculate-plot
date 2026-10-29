// Derived from Matplotlib 3.11.2: lib/matplotlib/projections/polar.py (PolarTransform, PolarAffine,
// InvertedPolarTransform, _WedgeBbox).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"
#include "transforms/transforms.hpp"

#include <array>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

// PolarTransform(axis, use_rmin, scale_transform): (theta, r) to x = r cos(theta), y = r sin(theta), after r is
// moved by the radial origin and turned by its sign (what the axis gives: `origin` is the polar Axes'
// _originViewLim, whose y0 is get_rorigin() and the sign of y1 - y0 get_rsign()); negative radii are NaN. No
// origin: no axis. A path whose _interpolation_steps is more than 1 is drawn along the circles: a segment at
// one radius becomes arcs, one at one angle stays straight, any other is interpolated. `rc` makes the new
// paths (Path(xys, codes) takes the simplification settings of the rcParams).
class PolarTransform : public transforms::Transform {
public:
    PolarTransform(transforms::BboxPtr origin, transforms::TransformPtr scale, bool useRmin, const rc::Params& rc)
        : origin_(std::move(origin)), scale_(std::move(scale)), useRmin_(useRmin), rc_(rc) {}
    transforms::Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    std::array<double, 2> transformNonAffine(double theta, double r) const override;
    transforms::TransformPtr inverted() const override;
    // transform_path_non_affine
    Path transformPath(const Path& path) const;

private:
    double rorigin() const;  // _get_rorigin: the origin through the radial scale
    double rsign() const;
    transforms::BboxPtr origin_;
    transforms::TransformPtr scale_;
    bool useRmin_;
    const rc::Params& rc_;
};

// InvertedPolarTransform(axis, use_rmin): (x, y) to theta = arctan2(y, x) % 2 pi and r = hypot(x, y), plus the
// radial origin, times its sign.
class InvertedPolarTransform : public transforms::Transform {
public:
    InvertedPolarTransform(transforms::BboxPtr origin, bool useRmin, const rc::Params& rc)
        : origin_(std::move(origin)), useRmin_(useRmin), rc_(rc) {}
    transforms::Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    std::array<double, 2> transformNonAffine(double x, double y) const override;
    transforms::TransformPtr inverted() const override;

private:
    transforms::BboxPtr origin_;
    bool useRmin_;
    const rc::Params& rc_;
};

// PolarAffine(scale_transform, limits): scales by 0.5 over the scaled radial span of `limits` and moves the
// origin to (0.5, 0.5).
class PolarAffine : public transforms::Transform {
public:
    PolarAffine(transforms::TransformPtr scale, transforms::BboxPtr limits) : scale_(std::move(scale)), limits_(std::move(limits)) {}
    transforms::Matrix matrix() const override;

private:
    transforms::TransformPtr scale_;
    transforms::BboxPtr limits_;
};

// _WedgeBbox(center, viewLim, originLim): the box around the Wedge the (shifted) view limits make, with the
// radii measured from originLim's y0 and scaled so the outer one is 0.5, grown to a square.
class WedgeBbox : public transforms::BboxBase {
public:
    WedgeBbox(std::array<double, 2> center, transforms::BboxPtr viewLim, transforms::BboxPtr originLim)
        : center_(center), viewLim_(std::move(viewLim)), originLim_(std::move(originLim)) {}
    std::array<double, 4> points() const override;

private:
    std::array<double, 2> center_;
    transforms::BboxPtr viewLim_, originLim_;
};

}  // namespace calculate_plot::engine
