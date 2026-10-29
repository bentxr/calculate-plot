// Derived from Matplotlib 3.11.2: lib/matplotlib/projections/polar.py (PolarTransform, PolarAffine,
// InvertedPolarTransform, _WedgeBbox).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"
#include "transforms/transforms.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

class Axes;
class Axis;
class Tick;

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

// What a PolarAxes has beyond an Axes: the parts of its transforms that its setters change, and the transforms
// its axes, ticks and spines use (_set_lim_and_transforms).
struct PolarAxesState {
    double defaultThetaOffset = 0, defaultThetaDirection = 1;
    double defaultRlabelPosition = 22.5 * (M_PI / 180.0);  // np.deg2rad(rlabel_position)
    std::shared_ptr<transforms::LockableBbox> originViewLim;  // _originViewLim
    std::shared_ptr<transforms::Affine2D> direction, offset, rLabelPosition;  // _direction, _theta_offset, _r_label_position
    transforms::TransformPtr transShift, transWedge, transProjection, transProjectionAffine;
    std::shared_ptr<transforms::TransformedBbox> realViewLim;  // _realViewLim
    std::shared_ptr<WedgeBbox> axesLim;
    transforms::TransformPtr xaxisTransform, xaxisTextTransform, yaxisTransform;
    std::shared_ptr<transforms::TransformWrapper> yaxisTextTransform;

    double thetaOffset() const { return offset->matrix().tx; }        // get_theta_offset
    double thetaDirection() const { return direction->matrix().sx; }  // get_theta_direction
    double rorigin() const { return originViewLim->points()[1]; }     // get_rorigin
    double rsign() const;                                             // get_rsign
    double rlabelPosition() const { return rLabelPosition->matrix().tx * (180.0 / M_PI); }  // get_rlabel_position
};

// PolarAxes._set_lim_and_transforms: the Axes' transforms (transScale, transAxes, transData, the axes' transforms)
// and its polar state.
void setPolarLimAndTransforms(Axes& ax);
// PolarAxes.clear's part after Axes.clear: the theta limits, the grid, the offset and direction
void clearPolar(Axes& ax);
// PolarAxes.draw before Axes.draw: the wedge patch, the start / end / inner spines and the radial labels' transform
void beforePolarDraw(Axes& ax);
// ThetaLocator and ThetaFormatter of the theta axis: the locations and their labels
void thetaTicks(Axis& axis, std::vector<double>* locs, std::vector<std::string>* labels);
// RadialLocator: of a full circle, the ticks beyond the origin
std::vector<double> radialTicks(const Axes& ax, std::vector<double> locs);
// _zero_in_bounds: zero is within the radial scale's range
bool radialZeroInBounds(const Axis& axis);
// ThetaTick.update_position / RadialTick.update_position after XTick's / YTick's
void updatePolarTick(Axes& ax, Tick& tick, bool isX, double loc);
// _ThetaShift(axes, pad, mode): 'min', 'max' or 'rlabel'
transforms::TransformPtr polarThetaShift(const Axes& ax, double pad, const std::string& mode);
// _is_full_circle_deg / _is_full_circle_rad
bool isFullCircleDeg(double thetamin, double thetamax);
bool isFullCircleRad(double thetamin, double thetamax);

}  // namespace calculate_plot::engine
