// Derived from Matplotlib 3.11.2: src/_path.h (path_intersects_rectangle,
// segment_intersects_rectangle, count_bboxes_overlapping_bbox, point_in_path_impl, points_in_path and
// point_in_path, for one point).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#ifndef MPL_PATH_QUERIES_H
#define MPL_PATH_QUERIES_H

#include "agg/path.h"
#include "agg/path_converters.h"

#include "agg_conv_contour.h"
#include "agg_conv_curve.h"
#include "agg_conv_transform.h"
#include "agg_trans_affine.h"

#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace mpl {

// see doc/segment_intersects_rectangle.svg in matplotlib for a more detailed explanation
inline bool segment_intersects_rectangle(double x1, double y1, double x2, double y2, double cx, double cy, double w,
                                         double h)
{
    return fabs(x1 + x2 - 2.0 * cx) < fabs(x1 - x2) + w &&
           fabs(y1 + y2 - 2.0 * cy) < fabs(y1 - y2) + h &&
           2.0 * fabs((x1 - cx) * (y1 - y2) - (y1 - cy) * (x1 - x2)) < w * fabs(y1 - y2) + h * fabs(x1 - x2);
}

// path_intersects_rectangle with filled=False (the only use so far: Legend's 'best' location).
inline bool path_intersects_rectangle(PathIterator &path, double rect_x1, double rect_y1, double rect_x2,
                                      double rect_y2)
{
    if (path.total_vertices() == 0) {
        return false;
    }

    auto no_nans = PathNanRemover{path, true, path.has_codes()};
    auto curve = agg::conv_curve{no_nans};

    double cx = (rect_x1 + rect_x2) * 0.5, cy = (rect_y1 + rect_y2) * 0.5;
    double w = fabs(rect_x1 - rect_x2), h = fabs(rect_y1 - rect_y2);

    double x1, y1, x2, y2;

    curve.vertex(&x1, &y1);
    if (2.0 * fabs(x1 - cx) <= w && 2.0 * fabs(y1 - cy) <= h) {
        return true;
    }

    while (curve.vertex(&x2, &y2) != agg::path_cmd_stop) {
        if (segment_intersects_rectangle(x1, y1, x2, y2, cx, cy, w, h)) {
            return true;
        }
        x1 = x2;
        y1 = y2;
    }

    return false;
}

// count_bboxes_overlapping_bbox: boxes given as x0, y0, x1, y1
inline int count_bboxes_overlapping_bbox(std::array<double, 4> a, const std::vector<std::array<double, 4>> &bboxes)
{
    int count = 0;

    if (a[2] < a[0]) {
        std::swap(a[0], a[2]);
    }
    if (a[3] < a[1]) {
        std::swap(a[1], a[3]);
    }

    for (std::array<double, 4> b : bboxes) {
        if (b[2] < b[0]) {
            std::swap(b[0], b[2]);
        }
        if (b[3] < b[1]) {
            std::swap(b[1], b[3]);
        }
        if (!((b[2] <= a[0]) || (b[3] <= a[1]) || (b[0] >= a[2]) || (b[1] >= a[3]))) {
            ++count;
        }
    }

    return count;
}

// point_in_path_impl for one point (x, y): the crossing test over each subpath of `path`
template <class Path>
inline bool point_in_path_impl(double tx, double ty, Path &path)
{
    uint8_t yflag0 = 0, yflag1;
    double vtx0 = 0, vty0 = 0, vtx1 = 0, vty1 = 0;
    double sx = 0, sy = 0;
    double x = 0, y = 0;
    uint8_t subpath_flag = 0, inside_flag = 0;
    const bool finite = std::isfinite(tx) && std::isfinite(ty);

    path.rewind(0);
    unsigned code = 0;
    do {
        if (code != agg::path_cmd_move_to) {
            code = path.vertex(&x, &y);
            if (code == agg::path_cmd_stop || (code & agg::path_cmd_end_poly) == agg::path_cmd_end_poly) {
                continue;
            }
        }
        sx = vtx0 = vtx1 = x;
        sy = vty0 = vty1 = y;
        if (std::isfinite(ty)) {
            yflag0 = (vty0 >= ty);
            subpath_flag = 0;
        }
        do {
            code = path.vertex(&x, &y);
            if (code == agg::path_cmd_stop || (code & agg::path_cmd_end_poly) == agg::path_cmd_end_poly) {
                x = sx;
                y = sy;
            } else if (code == agg::path_cmd_move_to) {
                break;
            }
            if (finite) {
                yflag1 = (vty1 >= ty);
                if (yflag0 != yflag1) {
                    if (((vty1 - ty) * (vtx0 - vtx1) >= (vtx1 - tx) * (vty0 - vty1)) == yflag1) {
                        subpath_flag ^= 1;
                    }
                }
                yflag0 = yflag1;
            }
            vtx0 = vtx1;
            vty0 = vty1;
            vtx1 = x;
            vty1 = y;
        } while (code != agg::path_cmd_stop && (code & agg::path_cmd_end_poly) != agg::path_cmd_end_poly);

        bool all_done = true;
        if (finite) {
            yflag1 = (vty1 >= ty);
            if (yflag0 != yflag1) {
                if (((vty1 - ty) * (vtx0 - vtx1) >= (vtx1 - tx) * (vty0 - vty1)) == yflag1) {
                    subpath_flag = subpath_flag ^ true;
                }
            }
            inside_flag |= subpath_flag;
            if (inside_flag == 0) {
                all_done = false;
            }
        }
        if (all_done) {
            break;
        }
    } while (code != agg::path_cmd_stop);
    return inside_flag != 0;
}

// point_in_path(x, y, r, path, trans): whether the point is inside the path through `trans`, grown by
// a contour of width r when r is not 0 (Path.contains_point)
template <class PathIterator>
inline bool point_in_path(double x, double y, const double r, PathIterator &path, agg::trans_affine &trans)
{
    if (path.total_vertices() < 3) {
        return false;
    }
    auto trans_path = agg::conv_transform{path, trans};
    auto no_nans_path = PathNanRemover{trans_path, true, path.has_codes()};
    auto curved_path = agg::conv_curve{no_nans_path};
    if (r != 0.0) {
        auto contoured_path = agg::conv_contour{curved_path};
        contoured_path.width(r);
        return point_in_path_impl(x, y, contoured_path);
    }
    return point_in_path_impl(x, y, curved_path);
}

}  // namespace mpl

#endif
