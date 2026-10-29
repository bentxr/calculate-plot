// Derived from Matplotlib 3.11.2: src/tri/_tri.h, src/tri/_tri.cpp (TriContourGenerator, ContourLine).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "tri/triangulation.hpp"

#include <vector>

namespace calculate_plot::tri {

// _tri.TriContourGenerator(triangulation, z): the contour lines of z over a triangulation at a level, and the
// polygons between two levels; each piece its vertices and their path codes (MOVETO, LINETO, CLOSEPOLY)
class TriContourGenerator {
public:
    struct Pieces {
        std::vector<std::vector<double>> vertices;  // x0, y0, x1, y1, ... of each piece
        std::vector<std::vector<unsigned char>> codes;
    };
    TriContourGenerator(Triangulation& triangulation, std::vector<double> z) : tri_(triangulation), z_(std::move(z)) {}
    Pieces lines(double level);                       // create_contour: a piece per line
    Pieces filled(double lowerLevel, double upperLevel);  // create_filled_contour: one piece of every polygon

private:
    using Line = std::vector<XY>;
    void push(Line& line, const XY& point) {  // ContourLine::push_back: no point twice in a row
        if (line.empty() || point != line.back()) line.push_back(point);
    }
    void clearVisited(bool includeBoundaries);
    XY edgeInterp(int tri, int edge, double level) const;
    void findBoundaryLines(std::vector<Line>& contour, double level);
    void findBoundaryLinesFilled(std::vector<Line>& contour, double lower, double upper);
    void findInteriorLines(std::vector<Line>& contour, double level, bool onUpper);
    bool followBoundary(Line& line, TriEdge& triEdge, double lower, double upper, bool onUpper);
    void followInterior(Line& line, TriEdge& triEdge, bool endOnBoundary, double level, bool onUpper);
    int exitEdge(int tri, double level, bool onUpper) const;
    double z(int point) const { return z_[std::size_t(point)]; }

    Triangulation& tri_;
    std::vector<double> z_;
    std::vector<bool> interiorVisited_;
    std::vector<std::vector<bool>> boundariesVisited_;
    std::vector<bool> boundariesUsed_;
};

}  // namespace calculate_plot::tri
