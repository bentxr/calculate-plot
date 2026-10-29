// Derived from Matplotlib 3.11.2: lib/matplotlib/projections/polar.py (PolarTransform, PolarAffine,
// InvertedPolarTransform, _WedgeBbox), lib/matplotlib/path.py (Path.iter_segments) and lib/matplotlib/cbook.py
// (simple_linear_interpolation).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/polar.hpp"
#include "agg/path_text.h"

#include "engine/artist.hpp"
#include "support/histogram.hpp"
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

}  // namespace calculate_plot::engine
