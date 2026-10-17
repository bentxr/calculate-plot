// Derived from Matplotlib 3.11.2: lib/matplotlib/path.py (Path) and lib/matplotlib/backends/backend_agg.py
// (RendererAgg.draw_path: chunks).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "transforms/transforms.hpp"

#include <array>
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
    // Path.arc(theta1, theta2): the unit circle's arc from theta1 to theta2 degrees, as cubic Bezier
    // curves (n = 2 ** ceil(sweep / (pi/2)) of them)
    static Path arc(double theta1, double theta2, const rc::Params& rc);
    // Path.unit_rectangle(): (0, 0), (1, 0), (1, 1), (0, 1), closed.
    static Path unitRectangle(const rc::Params& rc);
    // The same path made under other rcParams (its simplification settings are theirs).
    Path madeWith(const rc::Params& rc) const;
};

// RendererAgg.draw_path: a path of more than rcParams["agg.path.chunksize"] (> 100) vertices that may be
// simplified, drawn without a face or a hatch, goes to Agg as chunks of at most that many vertices, each
// a new Path (its own should_simplify, the original threshold), starting with a MOVETO when there are
// codes; the vertex before each chunk is left out. Empty when the path is drawn whole.
// Matplotlib also writes those MOVETOs into the original path's codes; a later draw of the same path
// would see them. No path the engine draws has codes and that many vertices without a face.
std::vector<Path> aggChunks(const Path& path, bool filled, bool hatched, const rc::Params& rc);

// Transform.transform_path_non_affine: the vertices through the transform's non-affine part, the
// codes and simplification settings kept (_fast_from_codes_and_verts); the path itself when affine.
Path transformPathNonAffine(const Path& path, const transforms::Transform& transform);

// Path.iter_bezier(simplify=False): the path as Path.cleaned leaves it (non-finite vertices removed, the
// next one a MOVETO), each segment a Bezier curve; of each, its ends and the points where its derivative
// is zero (axis_aligned_extrema), as Path.get_extents and _update_patch_limits take them
std::vector<std::array<double, 2>> bezierEnds(const Path& path);

}  // namespace calculate_plot::engine
