// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path, Path.arc) and lib/matplotlib/backends/backend_agg.py
// (RendererAgg.draw_path: chunks).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/path.hpp"

#include "rc/rc_params.hpp"
#include "support/histogram.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::engine {

Path Path::arc(double theta1, double theta2, const rc::Params& rc) {
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
    return make(std::move(v), std::move(codes), rc);
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

Path transformPathNonAffine(const Path& path, const transforms::Transform& transform) {
    if (transform.isAffine() || !path.vertices) return path;
    Path out = path;
    std::vector<double> v = *path.vertices;
    for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
        const auto p = transform.transformNonAffine(v[i], v[i + 1]);
        v[i] = p[0], v[i + 1] = p[1];
    }
    out.vertices = std::make_shared<std::vector<double>>(std::move(v));
    return out;
}

Path Path::madeWith(const rc::Params& rc) const {
    return make(*vertices, codes ? *codes : std::vector<unsigned char>{}, rc);
}

Path Path::unitRectangle(const rc::Params& rc) {
    return make({0, 0, 1, 0, 1, 1, 0, 1, 0, 0}, {}, rc, true);
}

}  // namespace calculate_plot::engine
