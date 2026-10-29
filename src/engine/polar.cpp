// Derived from Matplotlib 3.11.2: lib/matplotlib/projections/polar.py (PolarTransform, PolarAffine,
// InvertedPolarTransform, _WedgeBbox), lib/matplotlib/path.py (Path.iter_segments) and lib/matplotlib/cbook.py
// (simple_linear_interpolation).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/polar.hpp"
#include "agg/path_text.h"

#include "engine/artist.hpp"
#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/histogram.hpp"
#include "support/log10.hpp"
#include "support/pyformat.hpp"
#include "support/pynum.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace calculate_plot::engine {

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

double rad2deg(double v) { return v * (180.0 / M_PI); }  // npy_rad2deg

// np.sign
double sign(double v) { return std::isnan(v) ? v : v > 0 ? 1.0 : v < 0 ? -1.0 : 0.0; }

// Path.NUM_VERTICES_FOR_CODE
std::size_t verticesFor(unsigned char code) { return code == CURVE3 ? 2 : code == CURVE4 ? 3 : 1; }

}  // namespace

double PolarTransform::rorigin() const { return scale_->transform(0, origin_->points()[1])[1]; }

double PolarTransform::rsign() const {
    const auto p = origin_->points();
    return sign(p[3] - p[1]);
}

std::array<double, 2> PolarTransform::transformNonAffine(double theta, double r) const {
    if (useRmin_ && origin_) r = (r - rorigin()) * rsign();
    if (!(r >= 0)) r = kNaN;  // np.where(r >= 0, r, np.nan)
    return {r * std::cos(theta), r * std::sin(theta)};
}

transforms::TransformPtr PolarTransform::inverted() const {
    return std::make_shared<InvertedPolarTransform>(origin_, useRmin_, rc_);
}

Path PolarTransform::transformPath(const Path& path) const {
    const std::size_t n = path.size();
    std::vector<double> xys;
    std::vector<unsigned char> codes;
    if (!n || path.interpolationSteps == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            const auto p = transformNonAffine((*path.vertices)[2 * i], (*path.vertices)[2 * i + 1]);
            xys.insert(xys.end(), {p[0], p[1]});
        }
        return Path::make(std::move(xys), path.codes ? *path.codes : std::vector<unsigned char>{}, rc_);
    }
    // iter_segments(): the path as Path.cleaned leaves it (NaNs removed, simplified when it should be)
    mpl::PathIterator it(path.vertices, path.codes, path.shouldSimplify, path.simplifyThreshold);
    agg::trans_affine identity;
    std::vector<double> v;
    std::vector<unsigned char> cleaned;
    cleanup_path(it, identity, true, false, agg::rect_d(0.0, 0.0, 0.0, 0.0), SNAP_FALSE, 1.0, path.shouldSimplify, true,
                 SketchParams{0.0, 0.0, 0.0}, v, cleaned);
    const auto put = [&](double t, double r) {
        const auto p = transformNonAffine(t, r);
        xys.insert(xys.end(), {p[0], p[1]});
    };
    // the arc's vertices after the first (reversed, then after the first), times r
    const auto arc = [&](double theta1, double theta2, bool reversed, double r) {
        std::vector<double> av;
        std::vector<unsigned char> ac;
        Path::arcData(theta1, theta2, &av, &ac);
        const std::size_t m = av.size() / 2;
        for (std::size_t k = 1; k < m; ++k) {
            const std::size_t j = reversed ? m - 1 - k : k;
            xys.insert(xys.end(), {av[2 * j] * r, av[2 * j + 1] * r});
        }
        codes.insert(codes.end(), ac.begin() + 1, ac.end());
    };
    double lastT = kNaN, lastR = kNaN;  // (None at first: a cleaned path starts with a MOVETO)
    for (std::size_t i = 0; i < cleaned.size();) {
        const unsigned char c = cleaned[i];
        if (c == STOP) break;
        const std::size_t count = verticesFor(c);
        if (c == LINETO) {
            const double t = v[2 * i], r = v[2 * i + 1];
            if (t == lastT) {  // the same angle: a straight line
                put(t, r);
                codes.push_back(LINETO);
            } else if (r == lastR) {  // the same radius: arcs
                double lastTd = rad2deg(lastT);
                const double td = rad2deg(t);
                double rr = r;
                if (useRmin_ && origin_) rr = (r - rorigin()) * rsign();
                if (lastTd <= td) {
                    while (td - lastTd > 360) {
                        arc(lastTd, lastTd + 360, false, rr);
                        lastTd += 360;
                    }
                    arc(lastTd, td, false, rr);
                } else {
                    while (lastTd - td > 360) {
                        arc(lastTd - 360, lastTd, true, rr);
                        lastTd -= 360;
                    }
                    arc(td, lastTd, true, rr);
                }
            } else {  // cbook.simple_linear_interpolation of the two ends, after the first
                const int steps = path.interpolationSteps;
                std::vector<double> x, xp{0.0, double(steps)};
                for (int k = 0; k <= steps; ++k) x.push_back(double(k));
                const auto ts = support::npInterp(x, xp, {lastT, t});
                const auto rs = support::npInterp(x, xp, {lastR, r});
                for (int k = 1; k <= steps; ++k) {
                    put(ts[std::size_t(k)], rs[std::size_t(k)]);
                    codes.push_back(LINETO);
                }
            }
        } else {  // not a straight line: its points one by one
            for (std::size_t k = 0; k < count; ++k) {
                put(v[2 * (i + k)], v[2 * (i + k) + 1]);
                codes.push_back(c);
            }
        }
        lastT = v[2 * (i + count - 1)], lastR = v[2 * (i + count - 1) + 1];
        i += count;
    }
    return Path::make(std::move(xys), std::move(codes), rc_);
}

std::array<double, 2> InvertedPolarTransform::transformNonAffine(double x, double y) const {
    double r = std::hypot(x, y);
    const double theta = support::divmod(std::atan2(y, x), 2 * M_PI).second;  // % (2 * np.pi)
    if (useRmin_ && origin_) {
        const auto p = origin_->points();
        r += p[1];
        r *= sign(p[3] - p[1]);
    }
    return {theta, r};
}

transforms::TransformPtr InvertedPolarTransform::inverted() const {
    return std::make_shared<PolarTransform>(origin_, std::make_shared<transforms::IdentityTransform>(), useRmin_, rc_);
}

transforms::Matrix PolarAffine::matrix() const {
    // self._limits.transformed(self._scale_transform): both corners through the scale
    const auto p = limits_->points();
    const double y0 = scale_->transform(p[0], p[1])[1], y1 = scale_->transform(p[2], p[3])[1];
    const double yscale = support::npMax({y0, y1}) - support::npMin({y0, y1});
    transforms::Affine2D affine;
    affine.scale(0.5 / yscale).translate(0.5, 0.5);
    return affine.matrix();
}

std::array<double, 4> WedgeBbox::points() const {
    auto p = viewLim_->points();
    // the angular limits in degrees, increasing
    p[0] *= 180 / M_PI;
    p[2] *= 180 / M_PI;
    if (p[0] > p[2]) std::swap(p[0], p[2]);
    // the radial limits from the origin, the outer one at 0.5
    const double y0 = originLim_->points()[1];
    p[1] -= y0;
    p[3] -= y0;
    const double rscale = 0.5 / p[3];
    p[1] *= rscale;
    p[3] *= rscale;
    double width = p[3] - p[1];
    if (0.5 < width) width = 0.5;  // min(..., 0.5)
    std::vector<double> v;
    std::vector<unsigned char> codes;
    Wedge::pathData(center_[0], center_[1], p[3], p[0], p[2], width, &v, &codes);
    // update_from_path(ignore=True): the extremes of the finite vertices
    std::vector<double> xs, ys;
    for (std::size_t k = 0; k + 1 < v.size(); k += 2)
        if (std::isfinite(v[k]) && std::isfinite(v[k + 1])) xs.push_back(v[k]), ys.push_back(v[k + 1]);
    std::array<double, 4> out{support::npMin(xs), support::npMin(ys), support::npMax(xs), support::npMax(ys)};
    // an equal aspect ratio
    const double w = out[2] - out[0], h = out[3] - out[1];
    double deltah = w - h, deltaw = h - w;
    if (0 > deltah) deltah = 0;  // max(..., 0)
    if (0 > deltaw) deltaw = 0;
    deltah /= 2, deltaw /= 2;
    return {out[0] - deltaw, out[1] - deltah, out[2] + deltaw, out[3] + deltah};
}

// --- PolarAxes -----------------------------------------------------------------------------------------------

namespace {

double deg2rad(double v) { return v * (M_PI / 180.0); }  // npy_deg2rad

// Python's min(a, b) and max(a, b): the first unless the second is smaller (larger)
double pyMin(double a, double b) { return b < a ? b : a; }
double pyMax(double a, double b) { return b > a ? b : a; }

// _ThetaShift(axes, pad, mode): a translation of `pad` points away from the start ('min') or the end ('max') of the
// theta limits, or along the radial labels' angle ('rlabel')
class ThetaShift : public transforms::Transform {
public:
    ThetaShift(const Axes& ax, double pad, std::string mode) : ax_(ax), pad_(pad), mode_(std::move(mode)) {}
    transforms::Matrix matrix() const override {
        const PolarAxesState& p = *ax_.polar;
        double angle = 0;
        if (mode_ == "rlabel") {
            angle = deg2rad(p.rlabelPosition() * p.thetaDirection()) + p.thetaOffset() - M_PI / 2;
        } else {
            const auto v = p.realViewLim->points();
            angle = mode_ == "min" ? std::min(v[0], v[2]) - M_PI / 2 : std::max(v[0], v[2]) + M_PI / 2;
        }
        const auto t = ax_.figure.dpiScaleTrans->transform(pad_ * std::cos(angle) / 72, pad_ * std::sin(angle) / 72);
        return transforms::Matrix{1.0, 0.0, t[0], 0.0, 1.0, t[1]};
    }

private:
    const Axes& ax_;
    double pad_;
    std::string mode_;
};

// RadialTick._determine_anchor in the 'default' rotation mode
std::pair<std::string, std::string> determineAnchor(double angle, bool start) {
    if (start) {
        if (angle < -68.5) return {"center", "top"};
        if (angle < -23.5) return {"left", "top"};
        if (angle < 22.5) return {"left", "center"};
        if (angle < 67.5) return {"left", "bottom"};
        if (angle < 112.5) return {"center", "bottom"};
        if (angle < 157.5) return {"right", "bottom"};
        if (angle < 202.5) return {"right", "center"};
        if (angle < 247.5) return {"right", "top"};
        return {"center", "top"};
    }
    if (angle < -68.5) return {"center", "bottom"};
    if (angle < -23.5) return {"right", "bottom"};
    if (angle < 22.5) return {"right", "center"};
    if (angle < 67.5) return {"right", "top"};
    if (angle < 112.5) return {"center", "top"};
    if (angle < 157.5) return {"left", "top"};
    if (angle < 202.5) return {"left", "center"};
    if (angle < 247.5) return {"left", "bottom"};
    return {"center", "bottom"};
}

double pyMod(double a, double b) { return support::divmod(a, b).second; }

}  // namespace

transforms::TransformPtr polarThetaShift(const Axes& ax, double pad, const std::string& mode) {
    return std::make_shared<ThetaShift>(ax, pad, mode);
}

double PolarAxesState::rsign() const {
    const auto p = originViewLim->points();
    return sign(p[3] - p[1]);
}

bool isFullCircleDeg(double thetamin, double thetamax) { return std::fabs(std::fabs(thetamax - thetamin) - 360.0) < 1e-12; }

bool isFullCircleRad(double thetamin, double thetamax) { return std::fabs(std::fabs(thetamax - thetamin) - 2 * M_PI) < 1.74e-14; }

void setPolarLimAndTransforms(Axes& ax) {
    using namespace transforms;
    PolarAxesState& p = *ax.polar;
    const auto identity = std::make_shared<IdentityTransform>();
    p.originViewLim = std::make_shared<LockableBbox>(ax.viewLim);
    p.direction = std::make_shared<Affine2D>();
    p.direction->scale(p.defaultThetaDirection, 1.0);
    p.offset = std::make_shared<Affine2D>();
    p.offset->translate(p.defaultThetaOffset, 0.0);
    p.transShift = compose(p.direction, p.offset);
    p.realViewLim = std::make_shared<TransformedBbox>(ax.viewLim, p.transShift);
    ax.transScale = std::make_shared<TransformWrapper>(identity);
    p.axesLim = std::make_shared<WedgeBbox>(std::array<double, 2>{0.5, 0.5}, p.realViewLim, p.originViewLim);
    p.transWedge = std::make_shared<BboxTransformFrom>(p.axesLim);
    ax.transAxes = std::make_shared<BboxTransformTo>(ax.bbox);
    p.transProjection = std::make_shared<PolarTransform>(p.originViewLim, ax.transScale, true, ax.rc());
    p.transProjectionAffine = std::make_shared<PolarAffine>(ax.transScale, p.originViewLim);
    // transScale + transShift + transProjection + (transProjectionAffine + transWedge + transAxes)
    ax.transData = compose(compose(compose(ax.transScale, p.transShift), p.transProjection),
                           compose(compose(p.transProjectionAffine, p.transWedge), ax.transAxes));
    // (a PolarAxes has no transLimits; this one is the rectilinear Axes' and draws nothing)
    ax.transLimits = std::make_shared<BboxTransformFrom>(std::make_shared<TransformedBbox>(ax.viewLim, ax.transScale));
    // the theta ticks: r from the radial limits' bottom (0) to their top (1); their labels flipped to the outside
    p.xaxisTransform = compose(blend(identity, std::make_shared<BboxTransformTo>(ax.viewLim)), ax.transData);
    auto flipr = std::make_shared<Affine2D>();
    flipr->translate(0.0, -0.5).scale(1.0, -1.0).translate(0.0, 0.5);
    p.xaxisTextTransform = compose(flipr, p.xaxisTransform);
    // the radial ticks: theta from the theta limits' start (0) to their end (1); their labels at the rlabel angle
    p.yaxisTransform = compose(blend(std::make_shared<BboxTransformTo>(ax.viewLim), identity), ax.transData);
    p.rLabelPosition = std::make_shared<Affine2D>();
    p.rLabelPosition->translate(p.defaultRlabelPosition, 0.0);
    p.yaxisTextTransform = std::make_shared<TransformWrapper>(compose(p.rLabelPosition, ax.transData));
    ax.xaxisTransform = p.xaxisTransform;
    ax.yaxisTransform = p.yaxisTransform;
}

void clearPolar(Axes& ax) {
    PolarAxesState& p = *ax.polar;
    ax.title->y = 1.05;
    ax.spine("start").visible = false;
    ax.spine("end").visible = false;
    // ThetaAxis._set_scale: ticks at degree multiples
    ax.xaxis->locator.setSteps({1, 1.5, 3, 4.5, 9, 10});
    ax.xaxis->setLim(0.0, 2 * M_PI, false);
    const bool grid = ax.rc().boolean("polaraxes.grid");
    ax.xaxis->grid(grid, "major", TickSwitches(), false);
    ax.yaxis->grid(grid, "major", TickSwitches(), false);
    ax.spine("inner").visible = false;
    p.originViewLim->locked[1].reset();  // set_rorigin(None)
    auto m = p.offset->matrix();
    m.tx = p.defaultThetaOffset;  // set_theta_offset
    p.offset->setMatrix(m);
    m = p.direction->matrix();
    m.sx = p.defaultThetaDirection;  // set_theta_direction
    p.direction->setMatrix(m);
    // PolarAxes.__init__: set_aspect('equal', adjustable='box', anchor='C')
    ax.aspect = 1.0;
    ax.anchor = {0.5, 0.5};
}

void beforePolarDraw(Axes& ax) {
    PolarAxesState& p = *ax.polar;
    ax.unstaleViewLim();
    const auto rv = p.realViewLim->points();
    double thetamin = rad2deg(rv[0]), thetamax = rad2deg(rv[2]);
    if (thetamin > thetamax) std::swap(thetamin, thetamax);
    const Axis& r = *ax.yaxis;
    const double origin = r.scaleForward(p.rorigin()), rs = p.rsign();
    const double rmin = (r.scaleForward(rv[1]) - origin) * rs, rmax = (r.scaleForward(rv[3]) - origin) * rs;
    if (auto* wedge = dynamic_cast<Wedge*>(ax.patch.get())) {
        const auto center = p.transWedge->transform(0.5, 0.5);
        const double edge = p.transWedge->transform(1, 0)[0];
        const double radius = edge - center[0];
        const double width = pyMin(radius * (rmax - rmin) / rmax, radius);
        wedge->set(center[0], center[1], radius, thetamin, thetamax, width);
        ax.spine("inner").visible = radius - width != 0.0;
    }
    const bool visible = !isFullCircleDeg(thetamin, thetamax);
    ax.spine("start").visible = visible;
    ax.spine("end").visible = visible;
    const transforms::TransformPtr labels = visible ? p.yaxisTransform : transforms::compose(p.rLabelPosition, ax.transData);
    if (!transforms::equal(*p.yaxisTextTransform, *labels)) {
        p.yaxisTextTransform->set(labels);
        ax.yaxis->resetTicks();
    }
}

void thetaTicks(Axis& axis, std::vector<double>* locs, std::vector<std::string>* labels) {
    const auto view = axis.viewInterval();
    if (!labels) {
        // ThetaLocator: every 45 degrees of a full circle, else its base in degrees (_AxisWrapper)
        const double lim0 = rad2deg(view[0]), lim1 = rad2deg(view[1]);
        locs->clear();
        if (isFullCircleDeg(lim0, lim1)) {
            const double start = deg2rad(pyMin(lim0, lim1));
            for (int k = 0; k < 8; ++k) locs->push_back(start + double(k) * 2 * M_PI / 8);
        } else {
            axis.locator.tickSpace = axis.tickSpace();
            for (double v : axis.locator.tickValues(lim0, lim1)) locs->push_back(deg2rad(v));
        }
        return;
    }
    // ThetaFormatter: degrees, with as many decimals as the span needs, and the degree sign
    const double d = rad2deg(std::fabs(view[1] - view[0]));
    const int digits = static_cast<int>(pyMax(-std::trunc(support::log10(d) - 1.5), 0));
    labels->clear();
    for (double x : *locs) labels->push_back(support::percentF(rad2deg(x), digits) + "\u00b0");
}

std::vector<double> radialTicks(const Axes& ax, std::vector<double> locs) {
    const auto v = ax.viewLim->points();
    if (!isFullCircleRad(v[0], v[2])) return locs;
    const double rorigin = ax.polar->rorigin() * ax.polar->rsign();
    if (!(std::min(v[1], v[3]) <= rorigin)) return locs;  // get_rmin() <= rorigin
    std::vector<double> out;
    for (double t : locs)
        if (t > rorigin) out.push_back(t);
    return out;
}

bool radialZeroInBounds(const Axis& axis) { return !axis.logScale() && axis.scale() != "logit"; }

void updatePolarTick(Axes& ax, Tick& tick, bool isX, double loc) {
    const PolarAxesState& p = *ax.polar;
    const double direction = p.thetaDirection(), offset = p.thetaOffset();
    if (isX) {
        // (the label keeps the rotation it was given: the rotation mode is 'default'); the padding
        if (!tick.text1Translate) return;
        const double pad = tick.padPoints() + 7;
        const double angle = loc * direction + offset;
        const double padx = pad * std::cos(angle) / 72, pady = pad * std::sin(angle) / 72;
        tick.text1Translate->setTranslation(padx, pady);
        tick.text2Translate->setTranslation(-padx, -pady);
        return;
    }
    const auto v = ax.viewLim->points();
    const double thetamin = rad2deg(std::min(v[0], v[2])), thetamax = rad2deg(std::max(v[0], v[2]));
    const double offsetDeg = rad2deg(offset);
    const bool full = isFullCircleDeg(thetamin, thetamax);
    if (!full) {
        const double angle = pyMod(thetamin * direction + offsetDeg, 360) - 90;
        const auto [ha, va] = determineAnchor(angle, direction > 0);
        tick.label1.horizontalAlignment = ha;
        tick.label1.verticalAlignment = va;
    } else {
        tick.label2.visible = false;
        tick.tick2line.visible = false;
    }
    const double angle = pyMod(thetamax * direction + offsetDeg, 360) - 90;
    const auto [ha, va] = determineAnchor(angle, direction < 0);
    tick.label2.horizontalAlignment = ha;
    tick.label2.verticalAlignment = va;
}

}  // namespace calculate_plot::engine
