// Derived from Matplotlib 3.11.2: lib/matplotlib/bezier.py (BezierSegment, split_de_casteljau,
// find_bezier_t_intersecting_with_closedpath, split_bezier_intersecting_with_closedpath, split_path_inout,
// inside_circle, get_cos_sin, check_if_parallel, get_intersection, get_normal_points, get_parallels,
// find_control_points, make_wedged_bezier2).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"

#include <array>
#include <functional>
#include <utility>
#include <vector>

namespace calculate_plot::engine::bezier {

using Point = std::array<double, 2>;
using Points = std::vector<Point>;
using Inside = std::function<bool(const Point&)>;

// BezierSegment(points)(t) of one t: the binomially weighted powers of t and 1 - t, times the points
Point at(const Points& points, double t);
// split_de_casteljau(beta, t): the control points of the two halves
std::pair<Points, Points> splitDeCasteljau(const Points& beta, double t);
// inside_circle(cx, cy, r): (x - cx) ** 2 + (y - cy) ** 2 < r ** 2
Inside insideCircle(double cx, double cy, double r);
// find_bezier_t_intersecting_with_closedpath: bisection of [t0, t1] down to points closer than
// `tolerance`; false where matplotlib raises NonIntersectingPathException (both ends on one side)
bool findTIntersecting(const std::function<Point(double)>& pointAt, const Inside& inside, double* t0, double* t1,
                       double tolerance = 0.01);
// split_bezier_intersecting_with_closedpath: the curve split where it leaves (or enters) the path
bool splitIntersecting(const Points& points, const Inside& inside, Points* left, Points* right, double tolerance = 0.01);
// split_path_inout(path, inside): the path's part inside and outside; false where matplotlib raises
// "The path does not intersect with the patch"
bool splitPathInout(const Path& path, const Inside& inside, const rc::Params& rc, Path* in, Path* out,
                    double tolerance = 0.01, bool reorderInout = false);
// get_cos_sin(x0, y0, x1, y1): the direction from the first point to the second (0, 0 when they are one)
std::array<double, 2> cosSin(double x0, double y0, double x1, double y1);
// check_if_parallel: 1 (parallel), -1 (antiparallel) or 0
int checkIfParallel(double dx1, double dy1, double dx2, double dy2, double tolerance = 1.e-5);
// get_intersection of two lines through points at angles; false where matplotlib raises (parallel lines)
bool intersection(double cx1, double cy1, double cosT1, double sinT1, double cx2, double cy2, double cosT2,
                  double sinT2, double* x, double* y);
// get_normal_points: the two points at `length` on the perpendicular through (cx, cy)
std::array<double, 4> normalPoints(double cx, double cy, double cosT, double sinT, double length);
// get_parallels(bezier2, width): the quadratic curves at `width` on either side
std::pair<Points, Points> parallels(const Points& bezier2, double width);
// make_wedged_bezier2: curves on either side, `width` times w1 at the start, wm in the middle, w2 at the end
std::pair<Points, Points> wedged(const Points& bezier2, double width, double w1 = 1., double wm = 0.5, double w2 = 0.);

}  // namespace calculate_plot::engine::bezier
