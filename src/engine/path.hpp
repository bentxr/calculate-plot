// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <memory>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

enum PathCode : unsigned char { STOP = 0, MOVETO = 1, LINETO = 2, CURVE3 = 3, CURVE4 = 4, CLOSEPOLY = 79 };

// matplotlib.path.Path: vertices, optional codes, and the simplification settings taken from the
// rcParams when the path is made.
struct Path {
    std::shared_ptr<const std::vector<double>> vertices;  // x0, y0, x1, y1, ...
    std::shared_ptr<const std::vector<unsigned char>> codes;
    bool shouldSimplify = false;
    double simplifyThreshold = 0.111111111111;
    int interpolationSteps = 1;

    std::size_t size() const { return vertices ? vertices->size() / 2 : 0; }

    // Path(vertices, codes, closed=...): `closed` sets MOVETO, LINETO..., CLOSEPOLY when codes are not given.
    static Path make(std::vector<double> vertices, std::vector<unsigned char> codes, const rc::Params& rc,
                     bool closed = false);
    // Path.unit_rectangle(): (0, 0), (1, 0), (1, 1), (0, 1), closed.
    static Path unitRectangle(const rc::Params& rc);
};

}  // namespace calculate_plot::engine
