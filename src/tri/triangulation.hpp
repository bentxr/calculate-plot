// Derived from Matplotlib 3.11.2: lib/matplotlib/tri/_triangulation.py (Triangulation) and src/tri/_tri.h,
// src/tri/_tri.cpp (Triangulation, TriEdge, XY, XYZ).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::tri {

// A triangle's edge: the triangle and the edge's index in it (from point edge to point (edge + 1) % 3)
struct TriEdge {
    int tri = -1, edge = -1;
    bool operator<(const TriEdge& o) const { return tri != o.tri ? tri < o.tri : edge < o.edge; }
    bool operator==(const TriEdge& o) const { return tri == o.tri && edge == o.edge; }
    bool operator!=(const TriEdge& o) const { return !(*this == o); }
};

struct XY {
    double x = 0, y = 0;
    bool operator==(const XY& o) const { return x == o.x && y == o.y; }
    bool operator!=(const XY& o) const { return x != o.x || y != o.y; }
    XY operator-(const XY& o) const { return {x - o.x, y - o.y}; }
    XY operator+(const XY& o) const { return {x + o.x, y + o.y}; }
    XY operator*(double m) const { return {x * m, y * m}; }
    double crossZ(const XY& o) const { return x * o.y - y * o.x; }
};

// matplotlib.tri.Triangulation with its C++ part (_tri.Triangulation) made when first needed: the points, the
// triangles (Delaunay's when none are given), the mask, and the edges, neighbours and boundaries worked out on demand.
class Triangulation {
public:
    using Triangles = std::vector<std::array<int, 3>>;
    using Boundary = std::vector<TriEdge>;
    // Triangulation(x, y, triangles=None, mask=None); false and matplotlib's message where it raises
    static bool make(std::vector<double> x, std::vector<double> y, std::optional<Triangles> triangles,
                     std::optional<std::vector<bool>> mask, Triangulation* out, std::string* error);
    bool setMask(std::optional<std::vector<bool>> mask, std::string* error);  // set_mask

    const std::vector<double>& x() const { return x_; }
    const std::vector<double>& y() const { return y_; }
    // .triangles (anticlockwise in place once the C++ part has been made for triangles that were given)
    const Triangles& triangles() const { return triangles_; }
    const std::optional<std::vector<bool>>& mask() const { return mask_; }
    bool isDelaunay() const { return isDelaunay_; }
    Triangles maskedTriangles() const;  // get_masked_triangles
    // .edges: each edge of the unmasked triangles once, (start, end) with start > end, sorted
    const std::vector<std::array<int, 2>>& edges();
    const Triangles& neighbors();  // .neighbors (-1: none)
    // calculate_plane_coefficients(z): of each triangle a, b, c of z = a x + b y + c (zeros where masked); false and
    // matplotlib's message when z does not match the points
    bool planeCoefficients(const std::vector<double>& z, std::vector<std::array<double, 3>>* out, std::string* error);

    // the C++ triangulation's own parts, as TriContourGenerator uses them
    int npoints() const { return int(x_.size()); }
    int ntri() const { return int(triangles_.size()); }
    bool isMasked(int tri) const { return mask_ && (*mask_)[std::size_t(tri)]; }
    int trianglePoint(int tri, int edge) const { return triangles_[std::size_t(tri)][std::size_t(edge)]; }
    int trianglePoint(const TriEdge& e) const { return trianglePoint(e.tri, e.edge); }
    XY pointCoords(int point) const { return {x_[std::size_t(point)], y_[std::size_t(point)]}; }
    int neighbor(int tri, int edge);
    TriEdge neighborEdge(int tri, int edge);
    const std::vector<Boundary>& boundaries();
    void boundaryEdge(const TriEdge& triEdge, int* boundary, int* edge);

private:
    void cpp();  // get_cpp_triangulation
    int edgeInTriangle(int tri, int point) const;
    std::vector<double> x_, y_;
    Triangles triangles_;
    std::optional<std::vector<bool>> mask_;
    bool isDelaunay_ = false;
    // the Python layer's cached _edges and _neighbors, and the C++ layer's
    std::optional<std::vector<std::array<int, 2>>> edges_;
    std::optional<Triangles> neighbors_;
    bool cppMade_ = false;
    std::optional<std::vector<std::array<int, 2>>> cppEdges_;
    std::optional<Triangles> cppNeighbors_;
    std::vector<Boundary> boundaries_;
    std::map<TriEdge, std::pair<int, int>> triEdgeToBoundary_;
};

}  // namespace calculate_plot::tri
