// Derived from Matplotlib 3.11.2: src/_qhull_wrapper.cpp (delaunay, delaunay_impl).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <array>
#include <string>
#include <vector>

namespace calculate_plot::tri {

// _qhull.delaunay(x, y): the Delaunay triangulation of the points by Qhull ("qhull d Qt Qbb Qc Qz" of the points less
// their mean), each triangle's three point indices (anticlockwise) and its three neighbours (-1: none). False and
// matplotlib's message when there are fewer than 3 points, fewer than 3 unique ones, or Qhull fails.
struct Delaunay {
    std::vector<std::array<int, 3>> triangles, neighbors;
};
bool delaunay(const std::vector<double>& x, const std::vector<double>& y, Delaunay* out, std::string* error);

}  // namespace calculate_plot::tri
