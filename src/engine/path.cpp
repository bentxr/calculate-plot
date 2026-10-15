// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path) and lib/matplotlib/backends/backend_agg.py
// (RendererAgg.draw_path: chunks).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/path.hpp"

#include "rc/rc_params.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::engine {

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
