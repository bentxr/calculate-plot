// Derived from Matplotlib 3.11.2: lib/matplotlib/tri/_triangulation.py (Triangulation) and src/tri/_tri.cpp
// (Triangulation: calculate_boundaries, calculate_edges, calculate_neighbors, calculate_plane_coefficients,
// correct_triangles and the accessors).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "tri/triangulation.hpp"

#include "tri/delaunay.hpp"

#include <algorithm>
#include <set>

namespace calculate_plot::tri {

namespace {

// _tri.cpp's Edge: (start, end), ordered by start then end
struct Edge {
    int start = -1, end = -1;
    bool operator<(const Edge& o) const { return start != o.start ? start < o.start : end < o.end; }
};

std::string shape(std::size_t n) { return "(" + std::to_string(n) + ",)"; }

}  // namespace

bool Triangulation::make(std::vector<double> x, std::vector<double> y, std::optional<Triangles> triangles,
                         std::optional<std::vector<bool>> mask, Triangulation* out, std::string* error) {
    if (x.size() != y.size())
        return *error = "x and y must be equal-length 1D arrays, but found shapes " + shape(x.size()) + " and " + shape(y.size()), false;
    Triangulation t;
    if (!triangles) {
        Delaunay d;
        if (!delaunay(x, y, &d, error)) return false;
        t.triangles_ = std::move(d.triangles);
        t.isDelaunay_ = true;
    } else {
        if (triangles->empty()) return *error = "triangles must be a (N, 3) int array, but found shape (0,)", false;
        int lo = (*triangles)[0][0], hi = lo;
        for (const auto& tr : *triangles)
            for (int p : tr) lo = std::min(lo, p), hi = std::max(hi, p);
        if (hi >= int(x.size()))
            return *error = "triangles are indices into the points and must be in the range 0 <= i < " + std::to_string(x.size()) +
                            " but found value " + std::to_string(hi),
                   false;
        if (lo < 0)
            return *error = "triangles are indices into the points and must be in the range 0 <= i < " + std::to_string(x.size()) +
                            " but found value " + std::to_string(lo),
                   false;
        t.triangles_ = std::move(*triangles);
    }
    t.x_ = std::move(x);
    t.y_ = std::move(y);
    if (!t.setMask(std::move(mask), error)) return false;  // (which also drops Qhull's neighbours)
    *out = std::move(t);
    return true;
}

bool Triangulation::setMask(std::optional<std::vector<bool>> mask, std::string* error) {
    if (mask && mask->size() != triangles_.size()) return *error = "mask array must have same length as triangles array", false;
    mask_ = std::move(mask);
    // the C++ triangulation's derived fields and the Python ones go
    cppEdges_.reset();
    cppNeighbors_.reset();
    boundaries_.clear();
    triEdgeToBoundary_.clear();
    edges_.reset();
    neighbors_.reset();
    return true;
}

void Triangulation::cpp() {
    if (cppMade_) return;
    cppMade_ = true;
    cppEdges_ = edges_;
    cppNeighbors_ = neighbors_;
    if (isDelaunay_) return;
    // correct_triangles: clockwise triangles turned anticlockwise, in the triangles array itself
    for (std::size_t tri = 0; tri < triangles_.size(); ++tri) {
        const XY p0 = pointCoords(triangles_[tri][0]), p1 = pointCoords(triangles_[tri][1]), p2 = pointCoords(triangles_[tri][2]);
        if ((p1 - p0).crossZ(p2 - p0) < 0.0) {
            std::swap(triangles_[tri][1], triangles_[tri][2]);
            if (cppNeighbors_) std::swap((*cppNeighbors_)[tri][1], (*cppNeighbors_)[tri][2]);
        }
    }
}

Triangulation::Triangles Triangulation::maskedTriangles() const {
    if (!mask_) return triangles_;
    Triangles out;
    for (std::size_t i = 0; i < triangles_.size(); ++i)
        if (!(*mask_)[i]) out.push_back(triangles_[i]);
    return out;
}

const std::vector<std::array<int, 2>>& Triangulation::edges() {
    if (!edges_) {
        cpp();
        if (!cppEdges_) {  // calculate_edges
            std::set<Edge> edgeSet;
            for (int tri = 0; tri < ntri(); ++tri) {
                if (isMasked(tri)) continue;
                for (int edge = 0; edge < 3; ++edge) {
                    const int start = trianglePoint(tri, edge), end = trianglePoint(tri, (edge + 1) % 3);
                    edgeSet.insert(start > end ? Edge{start, end} : Edge{end, start});
                }
            }
            cppEdges_ = std::vector<std::array<int, 2>>{};
            for (const Edge& e : edgeSet) cppEdges_->push_back({e.start, e.end});
        }
        edges_ = cppEdges_;
    }
    return *edges_;
}

const Triangulation::Triangles& Triangulation::neighbors() {
    if (!neighbors_) {
        cpp();
        if (!cppNeighbors_) {  // calculate_neighbors
            Triangles n(triangles_.size(), {-1, -1, -1});
            std::map<Edge, TriEdge> edgeToTriEdge;
            for (int tri = 0; tri < ntri(); ++tri) {
                if (isMasked(tri)) continue;
                for (int edge = 0; edge < 3; ++edge) {
                    const int start = trianglePoint(tri, edge), end = trianglePoint(tri, (edge + 1) % 3);
                    const auto it = edgeToTriEdge.find(Edge{end, start});
                    if (it == edgeToTriEdge.end()) {
                        edgeToTriEdge[Edge{start, end}] = TriEdge{tri, edge};
                    } else {
                        n[std::size_t(tri)][std::size_t(edge)] = it->second.tri;
                        n[std::size_t(it->second.tri)][std::size_t(it->second.edge)] = tri;
                        edgeToTriEdge.erase(it);
                    }
                }
            }
            cppNeighbors_ = std::move(n);
        }
        neighbors_ = cppNeighbors_;
    }
    return *neighbors_;
}

int Triangulation::neighbor(int tri, int edge) {
    cpp();
    if (!cppNeighbors_) neighbors();
    return (*cppNeighbors_)[std::size_t(tri)][std::size_t(edge)];
}

int Triangulation::edgeInTriangle(int tri, int point) const {
    for (int edge = 0; edge < 3; ++edge)
        if (trianglePoint(tri, edge) == point) return edge;
    return -1;
}

TriEdge Triangulation::neighborEdge(int tri, int edge) {
    const int n = neighbor(tri, edge);
    if (n == -1) return TriEdge{-1, -1};
    return TriEdge{n, edgeInTriangle(n, trianglePoint(tri, (edge + 1) % 3))};
}

const std::vector<Triangulation::Boundary>& Triangulation::boundaries() {
    if (!boundaries_.empty()) return boundaries_;
    // calculate_boundaries: the edges without a neighbour, followed round
    cpp();
    std::set<TriEdge> boundaryEdges;
    for (int tri = 0; tri < ntri(); ++tri) {
        if (isMasked(tri)) continue;
        for (int edge = 0; edge < 3; ++edge)
            if (neighbor(tri, edge) == -1) boundaryEdges.insert(TriEdge{tri, edge});
    }
    while (!boundaryEdges.empty()) {
        auto it = boundaryEdges.cbegin();
        int tri = it->tri, edge = it->edge;
        boundaries_.emplace_back();
        Boundary& boundary = boundaries_.back();
        while (true) {
            boundary.push_back(TriEdge{tri, edge});
            boundaryEdges.erase(it);
            triEdgeToBoundary_[TriEdge{tri, edge}] = {int(boundaries_.size()) - 1, int(boundary.size()) - 1};
            edge = (edge + 1) % 3;
            const int point = trianglePoint(tri, edge);
            while (neighbor(tri, edge) != -1) {
                tri = neighbor(tri, edge);
                edge = edgeInTriangle(tri, point);
            }
            if (TriEdge{tri, edge} == boundary.front()) break;
            it = boundaryEdges.find(TriEdge{tri, edge});
        }
    }
    return boundaries_;
}

void Triangulation::boundaryEdge(const TriEdge& triEdge, int* boundary, int* edge) {
    boundaries();
    const auto it = triEdgeToBoundary_.find(triEdge);
    *boundary = it->second.first;
    *edge = it->second.second;
}

bool Triangulation::planeCoefficients(const std::vector<double>& z, std::vector<std::array<double, 3>>* out, std::string* error) {
    if (z.size() != x_.size())
        return *error = "z must be a 1D array with the same length as the triangulation x and y arrays", false;
    cpp();
    out->assign(triangles_.size(), {0.0, 0.0, 0.0});
    for (int tri = 0; tri < ntri(); ++tri) {
        if (isMasked(tri)) continue;
        const auto at = [&](int k) {
            const auto p = std::size_t(trianglePoint(tri, k));
            return std::array<double, 3>{x_[p], y_[p], z[p]};
        };
        const auto p0 = at(0), q1 = at(1), q2 = at(2);
        const std::array<double, 3> s01{q1[0] - p0[0], q1[1] - p0[1], q1[2] - p0[2]};
        const std::array<double, 3> s02{q2[0] - p0[0], q2[1] - p0[1], q2[2] - p0[2]};
        // XYZ::cross
        const std::array<double, 3> normal{s01[1] * s02[2] - s01[2] * s02[1], s01[2] * s02[0] - s01[0] * s02[2],
                                           s01[0] * s02[1] - s01[1] * s02[0]};
        auto& plane = (*out)[std::size_t(tri)];
        if (normal[2] == 0.0) {
            // colinear points: the Moore-Penrose pseudo-inverse
            const double sum2 = (s01[0] * s01[0] + s01[1] * s01[1] + s02[0] * s02[0] + s02[1] * s02[1]);
            const double a = (s01[0] * s01[2] + s02[0] * s02[2]) / sum2;
            const double b = (s01[1] * s01[2] + s02[1] * s02[2]) / sum2;
            plane = {a, b, p0[2] - a * p0[0] - b * p0[1]};
        } else {
            const double dot = normal[0] * p0[0] + normal[1] * p0[1] + normal[2] * p0[2];  // XYZ::dot
            plane = {-normal[0] / normal[2], -normal[1] / normal[2], dot / normal[2]};
        }
    }
    return true;
}

}  // namespace calculate_plot::tri
