// Derived from Matplotlib 3.11.2: src/tri/_tri.cpp (TriContourGenerator: clear_visited_flags,
// contour_line_to_segs_and_kinds, contour_to_segs_and_kinds, create_contour, create_filled_contour, edge_interp,
// find_boundary_lines, find_boundary_lines_filled, find_interior_lines, follow_boundary, follow_interior,
// get_exit_edge, interp).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "tri/tri_contour.hpp"

#include <algorithm>

namespace calculate_plot::tri {

namespace {
const unsigned char MOVETO = 1, LINETO = 2, CLOSEPOLY = 79;
}

void TriContourGenerator::clearVisited(bool includeBoundaries) {
    interiorVisited_.assign(std::size_t(2 * tri_.ntri()), false);
    if (includeBoundaries) {
        if (boundariesVisited_.empty()) {
            for (const auto& boundary : tri_.boundaries()) boundariesVisited_.emplace_back(boundary.size());
            boundariesUsed_.assign(tri_.boundaries().size(), false);
        }
        for (auto& v : boundariesVisited_) std::fill(v.begin(), v.end(), false);
        std::fill(boundariesUsed_.begin(), boundariesUsed_.end(), false);
    }
}

TriContourGenerator::Pieces TriContourGenerator::lines(double level) {
    clearVisited(false);
    std::vector<Line> contour;
    findBoundaryLines(contour, level);
    findInteriorLines(contour, level, false);
    // contour_line_to_segs_and_kinds: each line a piece; a closed loop (its last point its first) ends CLOSEPOLY
    Pieces out;
    for (const Line& line : contour) {
        std::vector<double> v;
        std::vector<unsigned char> c;
        for (const XY& p : line) v.push_back(p.x), v.push_back(p.y), c.push_back(LINETO);
        if (!c.empty()) c.front() = MOVETO;
        if (line.size() > 1 && line.front() == line.back()) c.back() = CLOSEPOLY;
        out.vertices.push_back(std::move(v));
        out.codes.push_back(std::move(c));
    }
    return out;
}

TriContourGenerator::Pieces TriContourGenerator::filled(double lowerLevel, double upperLevel) {
    clearVisited(true);
    std::vector<Line> contour;
    findBoundaryLinesFilled(contour, lowerLevel, upperLevel);
    findInteriorLines(contour, lowerLevel, false);
    findInteriorLines(contour, upperLevel, true);
    // contour_to_segs_and_kinds: all polygons in one piece, each from a MOVETO to a CLOSEPOLY
    Pieces out;
    std::vector<double> v;
    std::vector<unsigned char> c;
    for (const Line& line : contour) {
        for (std::size_t k = 0; k < line.size(); ++k) {
            v.push_back(line[k].x), v.push_back(line[k].y);
            c.push_back(k == 0 ? MOVETO : LINETO);
        }
        if (line.size() > 1) c.back() = CLOSEPOLY;
    }
    out.vertices.push_back(std::move(v));
    out.codes.push_back(std::move(c));
    return out;
}

XY TriContourGenerator::edgeInterp(int tri, int edge, double level) const {
    // interp(point1, point2, level)
    const int p1 = tri_.trianglePoint(tri, edge), p2 = tri_.trianglePoint(tri, (edge + 1) % 3);
    const double fraction = (z(p2) - level) / (z(p2) - z(p1));
    return tri_.pointCoords(p1) * fraction + tri_.pointCoords(p2) * (1.0 - fraction);
}

void TriContourGenerator::findBoundaryLines(std::vector<Line>& contour, double level) {
    for (const auto& boundary : tri_.boundaries()) {
        bool startAbove = false, endAbove = false;
        for (auto itb = boundary.cbegin(); itb != boundary.cend(); ++itb) {
            if (itb == boundary.cbegin()) startAbove = z(tri_.trianglePoint(*itb)) >= level;
            else startAbove = endAbove;
            endAbove = z(tri_.trianglePoint(itb->tri, (itb->edge + 1) % 3)) >= level;
            if (startAbove && !endAbove) {
                contour.emplace_back();
                TriEdge triEdge = *itb;
                followInterior(contour.back(), triEdge, true, level, false);
            }
        }
    }
}

void TriContourGenerator::findBoundaryLinesFilled(std::vector<Line>& contour, double lower, double upper) {
    const auto& boundaries = tri_.boundaries();
    for (std::size_t i = 0; i < boundaries.size(); ++i) {
        const auto& boundary = boundaries[i];
        for (std::size_t j = 0; j < boundary.size(); ++j) {
            if (boundariesVisited_[i][j]) continue;
            const double zStart = z(tri_.trianglePoint(boundary[j]));
            const double zEnd = z(tri_.trianglePoint(boundary[j].tri, (boundary[j].edge + 1) % 3));
            const bool incrUpper = zStart < upper && zEnd >= upper;
            const bool decrLower = zStart >= lower && zEnd < lower;
            if (decrLower || incrUpper) {
                contour.emplace_back();
                Line& line = contour.back();
                const TriEdge start = boundary[j];
                TriEdge triEdge = start;
                bool onUpper = incrUpper;
                do {
                    followInterior(line, triEdge, true, onUpper ? upper : lower, onUpper);
                    onUpper = followBoundary(line, triEdge, lower, upper, onUpper);
                } while (triEdge != start);
                push(line, line.front());
            }
        }
    }
    // the boundaries that no contour line crosses and lie between the levels
    for (std::size_t i = 0; i < boundaries.size(); ++i) {
        if (boundariesUsed_[i]) continue;
        const auto& boundary = boundaries[i];
        const double zz = z(tri_.trianglePoint(boundary[0]));
        if (zz >= lower && zz < upper) {
            contour.emplace_back();
            Line& line = contour.back();
            for (const TriEdge& edge : boundary) push(line, tri_.pointCoords(tri_.trianglePoint(edge)));
            push(line, line.front());
        }
    }
}

void TriContourGenerator::findInteriorLines(std::vector<Line>& contour, double level, bool onUpper) {
    const int ntri = tri_.ntri();
    for (int tri = 0; tri < ntri; ++tri) {
        const std::size_t visitedIndex = std::size_t(onUpper ? tri + ntri : tri);
        if (interiorVisited_[visitedIndex] || tri_.isMasked(tri)) continue;
        interiorVisited_[visitedIndex] = true;
        const int edge = exitEdge(tri, level, onUpper);
        if (edge == -1) continue;
        contour.emplace_back();
        Line& line = contour.back();
        TriEdge triEdge = tri_.neighborEdge(tri, edge);
        followInterior(line, triEdge, false, level, onUpper);
        push(line, line.front());
    }
}

bool TriContourGenerator::followBoundary(Line& line, TriEdge& triEdge, double lower, double upper, bool onUpper) {
    const auto& boundaries = tri_.boundaries();
    int boundary = 0, edge = 0;
    tri_.boundaryEdge(triEdge, &boundary, &edge);
    boundariesUsed_[std::size_t(boundary)] = true;
    bool stop = false, firstEdge = true;
    double zStart = 0, zEnd = 0;
    while (!stop) {
        boundariesVisited_[std::size_t(boundary)][std::size_t(edge)] = true;
        zStart = firstEdge ? z(tri_.trianglePoint(triEdge)) : zEnd;
        zEnd = z(tri_.trianglePoint(triEdge.tri, (triEdge.edge + 1) % 3));
        if (zEnd > zStart) {  // z increasing
            if (!(!onUpper && firstEdge) && zEnd >= lower && zStart < lower) {
                stop = true;
                onUpper = false;
            } else if (zEnd >= upper && zStart < upper) {
                stop = true;
                onUpper = true;
            }
        } else {  // z decreasing
            if (!(onUpper && firstEdge) && zStart >= upper && zEnd < upper) {
                stop = true;
                onUpper = true;
            } else if (zStart >= lower && zEnd < lower) {
                stop = true;
                onUpper = false;
            }
        }
        firstEdge = false;
        if (!stop) {
            edge = (edge + 1) % int(boundaries[std::size_t(boundary)].size());
            triEdge = boundaries[std::size_t(boundary)][std::size_t(edge)];
            push(line, tri_.pointCoords(tri_.trianglePoint(triEdge)));
        }
    }
    return onUpper;
}

void TriContourGenerator::followInterior(Line& line, TriEdge& triEdge, bool endOnBoundary, double level, bool onUpper) {
    int& tri = triEdge.tri;
    int& edge = triEdge.edge;
    push(line, edgeInterp(tri, edge, level));
    while (true) {
        std::size_t visitedIndex = std::size_t(tri);
        if (onUpper) visitedIndex += std::size_t(tri_.ntri());
        if (!endOnBoundary && interiorVisited_[visitedIndex]) break;  // back at the start
        edge = exitEdge(tri, level, onUpper);
        interiorVisited_[visitedIndex] = true;
        push(line, edgeInterp(tri, edge, level));
        const TriEdge next = tri_.neighborEdge(tri, edge);
        if (endOnBoundary && next.tri == -1) break;
        triEdge = next;
    }
}

int TriContourGenerator::exitEdge(int tri, double level, bool onUpper) const {
    unsigned config = unsigned(z(tri_.trianglePoint(tri, 0)) >= level) | unsigned(z(tri_.trianglePoint(tri, 1)) >= level) << 1 |
                      unsigned(z(tri_.trianglePoint(tri, 2)) >= level) << 2;
    if (onUpper) config = 7 - config;
    static const int exits[8] = {-1, 2, 0, 2, 1, 1, 0, -1};
    return exits[config];
}

}  // namespace calculate_plot::tri
