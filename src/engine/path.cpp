// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path, Path.arc) and lib/matplotlib/backends/backend_agg.py
// (RendererAgg.draw_path: chunks).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/path.hpp"
#include "engine/polar.hpp"
#include "agg/path_text.h"

#include "rc/rc_params.hpp"
#include "support/histogram.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::engine {

Path Path::arc(double theta1, double theta2, const rc::Params& rc) {
    std::vector<double> v;
    std::vector<unsigned char> codes;
    arcData(theta1, theta2, &v, &codes);
    return make(std::move(v), std::move(codes), rc);
}

void Path::arcData(double theta1, double theta2, std::vector<double>* vertices, std::vector<unsigned char>* pathCodes) {
    const double halfpi = M_PI * 0.5;
    double eta1 = theta1;
    const double nTurns = (theta2 - theta1) / 360;
    const double nearest = std::nearbyint(nTurns);  // np.rint
    const bool fullCircle = nearest != 0 && std::fabs(nTurns - nearest) <= 1e-12;
    double eta2 = fullCircle ? theta1 + 360 : theta2 - 360 * std::floor(nTurns);
    // np.deg2rad: x * (pi / 180)
    eta1 = eta1 * (M_PI / 180.0);
    eta2 = eta2 * (M_PI / 180.0);
    const long long n = static_cast<long long>(std::pow(2.0, std::ceil((eta2 - eta1) / halfpi)));
    const double deta = (eta2 - eta1) / double(n);
    const double t = std::tan(0.5 * deta);
    const double alpha = std::sin(deta) * (std::sqrt(4.0 + 3.0 * t * t) - 1) / 3.0;
    const std::vector<double> steps = support::npLinspace(eta1, eta2, n + 1);
    std::vector<double> cosEta, sinEta;
    for (double s : steps) cosEta.push_back(std::cos(s)), sinEta.push_back(std::sin(s));
    std::vector<double> v{cosEta[0], sinEta[0]};
    std::vector<unsigned char> codes{MOVETO};
    for (long long i = 0; i < n; ++i) {
        const double xA = cosEta[std::size_t(i)], yA = sinEta[std::size_t(i)];
        const double xB = cosEta[std::size_t(i + 1)], yB = sinEta[std::size_t(i + 1)];
        const double xAdot = -yA, yAdot = xA, xBdot = -yB, yBdot = xB;
        v.insert(v.end(), {xA + alpha * xAdot, yA + alpha * yAdot, xB - alpha * xBdot, yB - alpha * yBdot, xB, yB});
        codes.insert(codes.end(), {CURVE4, CURVE4, CURVE4});
    }
    *vertices = std::move(v);
    *pathCodes = std::move(codes);
}

Path Path::make(std::vector<double> vertices, std::vector<unsigned char> codes, const rc::Params& rc, bool closed) {
    Path p;
    const std::size_t n = vertices.size() / 2;
    if (codes.empty() && closed && n) {
        codes.assign(n, LINETO);
        codes.front() = MOVETO;
        codes.back() = CLOSEPOLY;
    }
    // _update_values: the threshold and whether to simplify come from the current rcParams.
    p.simplifyThreshold = rc.number("path.simplify_threshold");
    p.shouldSimplify = p.simplifyThreshold > 0 && rc.boolean("path.simplify") && n >= 128 &&
                       (codes.empty() || std::all_of(codes.begin(), codes.end(), [](unsigned char c) { return c <= LINETO; }));
    p.vertices = std::make_shared<std::vector<double>>(std::move(vertices));
    if (!codes.empty() || (closed && n)) p.codes = std::make_shared<std::vector<unsigned char>>(std::move(codes));
    return p;
}

std::vector<Path> aggChunks(const Path& path, bool filled, bool hatched, const rc::Params& rc) {
    const double nmax = rc.number("agg.path.chunksize");
    const std::size_t npts = path.size();
    if (!(double(npts) > nmax && nmax > 100 && path.shouldSimplify && !filled && !hatched)) return {};
    const double nch = std::ceil(double(npts) / nmax);
    const auto chsize = static_cast<std::size_t>(std::ceil(double(npts) / nch));
    std::vector<Path> chunks;
    for (std::size_t i0 = 0; i0 < npts; i0 += chsize) {
        const std::size_t i1 = i0 + chsize < npts ? i0 + chsize - 1 : npts;
        std::vector<double> v(path.vertices->begin() + 2 * i0, path.vertices->begin() + 2 * i1);
        std::vector<unsigned char> c;
        if (path.codes) {
            c.assign(path.codes->begin() + i0, path.codes->begin() + i1);
            c[0] = MOVETO;  // move to the end of the last chunk
        }
        Path p = Path::make(std::move(v), std::move(c), rc);
        p.simplifyThreshold = path.simplifyThreshold;
        chunks.push_back(std::move(p));
    }
    return chunks;
}

namespace {

// Whether a transform has a part that transforms paths otherwise than vertex by vertex (PolarTransform)
bool transformsPaths(const transforms::Transform& t) {
    if (dynamic_cast<const PolarTransform*>(&t)) return true;
    if (const auto* w = dynamic_cast<const transforms::TransformWrapper*>(&t)) return transformsPaths(*w->child());
    if (const auto* c = dynamic_cast<const transforms::CompositeTransform*>(&t))
        return transformsPaths(*c->a()) || transformsPaths(*c->b());
    return false;
}

bool affineIsIdentity(const transforms::Transform& t) {
    if (const auto* w = dynamic_cast<const transforms::TransformWrapper*>(&t)) return affineIsIdentity(*w->child());
    return t.isIdentity();
}

Path pointwise(const Path& path, const transforms::Transform& transform) {
    Path out = path;
    std::vector<double> v = *path.vertices;
    for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
        const auto p = transform.transformNonAffine(v[i], v[i + 1]);
        v[i] = p[0], v[i + 1] = p[1];
    }
    out.vertices = std::make_shared<std::vector<double>>(std::move(v));
    return out;
}

}  // namespace

Path transformPathNonAffine(const Path& path, const transforms::Transform& transform) {
    if (transform.isAffine() || !path.vertices) return path;
    if (!transformsPaths(transform)) return pointwise(path, transform);
    if (const auto* polar = dynamic_cast<const PolarTransform*>(&transform)) return polar->transformPath(path);
    if (const auto* w = dynamic_cast<const transforms::TransformWrapper*>(&transform)) return transformPathNonAffine(path, *w->child());
    // CompositeGenericTransform.transform_path_non_affine: the first part's alone when the second is affine, else
    // the second's of the first's transform_path (its affine part applied after; the path's settings kept, as
    // Path(vertices, codes, _interpolation_steps) makes them again under the same rcParams)
    const auto& c = dynamic_cast<const transforms::CompositeTransform&>(transform);
    if (c.b()->isAffine()) return transformPathNonAffine(path, *c.a());
    Path first = transformPathNonAffine(path, *c.a());
    if (!affineIsIdentity(*c.a())) {  // (IdentityTransform.transform_path_affine returns the path)
        const transforms::Matrix m = c.a()->matrix();
        std::vector<double> v = *first.vertices;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            const auto p = transforms::apply(m, v[i], v[i + 1]);
            v[i] = p[0], v[i + 1] = p[1];
        }
        first.vertices = std::make_shared<std::vector<double>>(std::move(v));
    }
    return transformPathNonAffine(first, *c.b());
}

Path Path::madeWith(const rc::Params& rc) const {
    return make(*vertices, codes ? *codes : std::vector<unsigned char>{}, rc);
}

Path Path::unitRectangle(const rc::Params& rc) {
    return make({0, 0, 1, 0, 1, 1, 0, 1, 0, 0}, {}, rc, true);
}

namespace {

// The real roots in [0, 1] of the polynomial c0 + c1 t + c2 t^2 (bezier._real_roots_in_01 for a degree of
// at most 2, which is all a cubic curve's derivative has), sorted
std::vector<double> realRootsIn01(std::vector<double> coeffs) {
    std::size_t deg = coeffs.size() - 1;
    while (deg > 0 && std::fabs(coeffs[deg]) < 1e-12) --deg;  // trailing near-zeros
    std::vector<double> roots;
    if (deg == 0) return roots;
    if (deg == 1) {
        const double root = -coeffs[0] / coeffs[1];
        if (0 <= root && root <= 1) roots.push_back(root);
        return roots;
    }
    // _quadratic_roots_in_01
    const double c0 = coeffs[0], c1 = coeffs[1], c2 = coeffs[2];
    if (std::fabs(c2) < 1e-12) {
        if (std::fabs(c1) < 1e-12) return roots;
        const double root = -c0 / c1;
        if (0 <= root && root <= 1) roots.push_back(root);
        return roots;
    }
    const double disc = c1 * c1 - 4 * c2 * c0;
    if (disc < 0) return roots;
    const double sqrtDisc = std::sqrt(disc);
    const double q = c1 >= 0 ? -0.5 * (c1 + sqrtDisc) : -0.5 * (c1 - sqrtDisc);
    std::vector<double> all;
    if (std::fabs(q) > 1e-12) all.push_back(c0 / q);
    if (std::fabs(c2) > 1e-12) all.push_back(q / c2);
    for (double r : all)
        if (r >= 0 && r <= 1) roots.push_back(r);
    std::sort(roots.begin(), roots.end());
    return roots;
}

// BezierSegment(control points): its points at the start, at its axis-aligned extrema and at the end
// (curve([0, *dzeros, 1])), each a matrix product as NumPy's BLAS makes it (fused, from +0.0)
void bezierPoints(const std::vector<std::array<double, 2>>& p, std::vector<std::array<double, 2>>* out) {
    const std::size_t N = p.size(), n = N - 1;
    static const double binom[4][4] = {{1, 0, 0, 0}, {1, 1, 0, 0}, {1, 2, 1, 0}, {1, 3, 3, 1}};
    std::vector<double> ts{0.0};
    if (n > 1) {
        // polynomial_coefficients: _get_coeff_matrix(n) @ control points
        std::vector<std::array<double, 2>> C(N);
        for (std::size_t j = 0; j < N; ++j)
            for (int d = 0; d < 2; ++d) {
                double acc = 0.0;
                for (std::size_t i = 0; i < N; ++i) {
                    const double m = i <= j ? binom[n][j] * (((i + j) % 2) ? -1.0 : 1.0) * binom[j][i] : 0.0;
                    acc = std::fma(m, p[i][std::size_t(d)], acc);
                }
                C[j][std::size_t(d)] = acc;
            }
        // dCj = arange(1, n + 1)[:, None] * Cj[1:]; the roots of each dimension's derivative, x then y
        for (int d = 0; d < 2; ++d) {
            std::vector<double> coeffs;
            for (std::size_t k = 1; k < N; ++k) coeffs.push_back(double(k) * C[k][std::size_t(d)]);
            for (double r : realRootsIn01(coeffs)) ts.push_back(r);
        }
    }
    ts.push_back(1.0);
    for (double tt : ts) {
        std::array<double, 2> q{};
        for (int d = 0; d < 2; ++d) {
            double acc = 0.0;
            for (std::size_t k = 0; k < N; ++k) {
                // np.power.outer(1 - t, orders[::-1]) * np.power.outer(t, orders), times _px (binomial weights)
                const double w = std::pow(1 - tt, double(n - k)) * std::pow(tt, double(k));
                acc = std::fma(w, p[k][std::size_t(d)] * binom[n][k], acc);
            }
            q[std::size_t(d)] = acc;
        }
        out->push_back(q);
    }
}

}  // namespace

// Path.iter_bezier(simplify=False): the path as Path.cleaned leaves it (non-finite vertices removed, the
// next one a MOVETO), each segment a Bezier curve of its start and control points (a MOVETO a point,
// CLOSEPOLY a line back to the subpath's start), and of each the points _update_patch_limits takes
std::vector<std::array<double, 2>> bezierEnds(const Path& path) {
    mpl::PathIterator it(path.vertices, path.codes, false, path.simplifyThreshold);
    agg::trans_affine identity;
    std::vector<double> v;
    std::vector<unsigned char> codes;
    cleanup_path(it, identity, true, false, agg::rect_d(0.0, 0.0, 0.0, 0.0), SNAP_FALSE, 1.0, false, true,
                 SketchParams{0.0, 0.0, 0.0}, v, codes);
    std::vector<std::array<double, 2>> out;
    std::array<double, 2> first{}, prev{};
    for (std::size_t i = 0; i < codes.size(); ++i) {
        const unsigned code = codes[i];
        if (code == agg::path_cmd_stop) break;
        const auto at = [&](std::size_t k) { return std::array<double, 2>{v[2 * k], v[2 * k + 1]}; };
        std::vector<std::array<double, 2>> control;
        if (code == MOVETO) {
            first = at(i);
            control = {first};
            prev = first;
        } else if (code == CURVE3 || code == CURVE4) {
            const std::size_t extra = code == CURVE3 ? 1 : 2;
            control = {prev};
            for (std::size_t k = i; k <= i + extra && k < codes.size(); ++k) control.push_back(at(k));
            i += extra;
            prev = control.back();
        } else {  // LINETO, or CLOSEPOLY: the line back to the first vertex
            const std::array<double, 2> end = (code & 0x7f) == CLOSEPOLY ? first : at(i);
            control = {prev, end};
            prev = at(i);
        }
        bezierPoints(control, &out);
    }
    return out;
}

}  // namespace calculate_plot::engine
