// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/path.hpp"

#include "rc/rc_params.hpp"

#include <algorithm>

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

Path Path::unitRectangle(const rc::Params& rc) {
    return make({0, 0, 1, 0, 1, 1, 0, 1, 0, 0}, {}, rc, true);
}

}  // namespace calculate_plot::engine
