// Derived from Matplotlib 3.11.2: src/_path.h (cleanup_path and convert_to_string).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <array>
#include <string>
#include <vector>

#include "agg_conv_curve.h"
#include "agg_conv_transform.h"
#include "backend_agg_basic_types.h"
#include "path_converters.h"
#include "support/pyformat.hpp"

const size_t NUM_VERTICES[] = { 1, 1, 1, 2, 3 };

template <class VertexSource>
void
__cleanup_path(VertexSource &source, std::vector<double> &vertices, std::vector<uint8_t> &codes)
{
    unsigned code;
    double x, y;
    do {
        code = source.vertex(&x, &y);
        vertices.push_back(x);
        vertices.push_back(y);
        codes.push_back(static_cast<uint8_t>(code));
    } while (code != agg::path_cmd_stop);
}

template <class PathIterator>
inline void cleanup_path(PathIterator &path,
                  agg::trans_affine &trans,
                  bool remove_nans,
                  bool do_clip,
                  const agg::rect_base<double> &rect,
                  e_snap_mode snap_mode,
                  double stroke_width,
                  bool do_simplify,
                  bool return_curves,
                  SketchParams sketch_params,
                  std::vector<double> &vertices,
                  std::vector<unsigned char> &codes)
{
    auto tpath = agg::conv_transform{path, trans};
    auto nan_removed = PathNanRemover{tpath, remove_nans, path.has_codes()};
    auto clipped = PathClipper{nan_removed, do_clip, rect};
    auto snapped = PathSnapper{
        clipped, snap_mode, path.total_vertices(), stroke_width};
    auto simplified = PathSimplifier{snapped, do_simplify, path.simplify_threshold()};

    vertices.reserve(path.total_vertices() * 2);
    codes.reserve(path.total_vertices());

    if (return_curves && sketch_params.scale == 0.0) {
        __cleanup_path(simplified, vertices, codes);
    } else {
        auto curve = agg::conv_curve{simplified};
        auto sketch = Sketch{
            curve, sketch_params.scale, sketch_params.length, sketch_params.randomness};
        __cleanup_path(sketch, vertices, codes);
    }
}

inline void quad2cubic(double x0, double y0,
                double x1, double y1,
                double x2, double y2,
                std::array<double, 3> &outx, std::array<double, 3> &outy)
{
    std::get<0>(outx) = x0 + 2./3. * (x1 - x0);
    std::get<0>(outy) = y0 + 2./3. * (y1 - y0);
    std::get<1>(outx) = std::get<0>(outx) + 1./3. * (x2 - x0);
    std::get<1>(outy) = std::get<0>(outy) + 1./3. * (y2 - y0);
    std::get<2>(outx) = x2;
    std::get<2>(outy) = y2;
}


inline void __add_number(double val, char format_code, int precision,
                         std::string& buffer)
{
    // As PyOS_double_to_string(val, 'f', precision, Py_DTSF_ADD_DOT_0): correctly rounded and
    // independent of the locale.
    (void)format_code;
    std::string str = calculate_plot::support::fixed(val, precision);
    if (str.find('.') == std::string::npos) {
        str += ".0";
    }
    // Delete trailing zeros and decimal point
    std::size_t c = str.size() - 1;  // Start at last character.
    // Rewind through all the zeros and, if present, the trailing decimal
    // point.  Py_DTSF_ADD_DOT_0 ensures we won't go past the start of str.
    while (str[c] == '0') {
        --c;
    }
    if (str[c] == '.') {
        --c;
    }
    buffer.append(str, 0, c + 1);
}


template <class PathIterator>
bool __convert_to_string(PathIterator &path,
                         int precision,
                         const std::array<std::string, 5> &codes,
                         bool postfix,
                         std::string& buffer)
{
    const char format_code = 'f';

    std::array<double, 3> x;
    std::array<double, 3> y;
    double last_x = 0.0;
    double last_y = 0.0;

    unsigned code;

    while ((code = path.vertex(&std::get<0>(x), &std::get<0>(y))) != agg::path_cmd_stop) {
        if (code == CLOSEPOLY) {
            buffer += std::get<4>(codes);
        } else if (code < 5) {
            size_t size = NUM_VERTICES[code];

            for (size_t i = 1; i < size; ++i) {
                unsigned subcode = path.vertex(&x.at(i), &y.at(i));
                if (subcode != code) {
                    return false;
                }
            }

            /* For formats that don't support quad curves, convert to
               cubic curves */
            if (code == CURVE3 && codes[code - 1][0] == '\0') {
                quad2cubic(last_x, last_y, x.at(0), y.at(0), x.at(1), y.at(1), x, y);
                code++;
                size = 3;
            }

            if (!postfix) {
                buffer += codes.at(code - 1);
                buffer += ' ';
            }

            for (size_t i = 0; i < size; ++i) {
                __add_number(x.at(i), format_code, precision, buffer);
                buffer += ' ';
                __add_number(y.at(i), format_code, precision, buffer);
                buffer += ' ';
            }

            if (postfix) {
                buffer += codes.at(code - 1);
            }

            last_x = x.at(size - 1);
            last_y = y.at(size - 1);
        } else {
            // Unknown code value
            return false;
        }

        buffer += '\n';
    }

    return true;
}

template <class PathIterator>
bool convert_to_string(PathIterator &path,
                       agg::trans_affine &trans,
                       agg::rect_d &clip_rect,
                       bool simplify,
                       SketchParams sketch_params,
                       int precision,
                       const std::array<std::string, 5> &codes,
                       bool postfix,
                       std::string& buffer)
{
    bool do_clip = (clip_rect.x1 < clip_rect.x2 && clip_rect.y1 < clip_rect.y2);

    auto tpath = agg::conv_transform{path, trans};
    auto nan_removed = PathNanRemover{tpath, true, path.has_codes()};
    auto clipped = PathClipper{nan_removed, do_clip, clip_rect};
    auto simplified = PathSimplifier{clipped, simplify, path.simplify_threshold()};

    size_t buffersize = (size_t) path.total_vertices() * (precision + 5) * 4;
    if (buffersize == 0) {
        return true;
    }

    if (sketch_params.scale != 0.0) {
        buffersize *= 10;
    }

    buffer.reserve(buffersize);

    if (sketch_params.scale == 0.0) {
        return __convert_to_string(simplified, precision, codes, postfix, buffer);
    } else {
        auto curve = agg::conv_curve{simplified};
        auto sketch = Sketch{
            curve, sketch_params.scale, sketch_params.length, sketch_params.randomness};
        return __convert_to_string(sketch, precision, codes, postfix, buffer);
    }
}

