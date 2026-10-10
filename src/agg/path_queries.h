// Derived from Matplotlib 3.11.2: src/_path.h (path_intersects_rectangle,
// segment_intersects_rectangle, count_bboxes_overlapping_bbox).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#ifndef MPL_PATH_QUERIES_H
#define MPL_PATH_QUERIES_H

#include "agg/path.h"
#include "agg/path_converters.h"

#include "agg_conv_curve.h"

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

}  // namespace mpl

#endif
