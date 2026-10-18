// Derived from ContourPy 1.4.0: lib/contourpy/__init__.py (contour_generator, _remove_z_mask) and
// lib/contourpy/chunk.py (calc_chunk_sizes), for the mpl2014 generator as matplotlib calls it.
// Copyright (c) 2021-2026, ContourPy Developers. All rights reserved.
// Licence: LICENSES/contourpy.txt. Changes: licensing/changes/contourpy.md
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace contourpy::mpl2014 { class Mpl2014ContourGenerator; }

namespace calculate_plot::contour {

// The paths of a level: each one's vertices (x0, y0, x1, y1, ...) and kind codes (1 MOVETO, 2 LINETO, 79 CLOSEPOLY)
struct Paths {
    std::vector<std::vector<double>> vertices;
    std::vector<std::vector<std::uint8_t>> codes;
};

// contourpy.contour_generator(x, y, z, name='mpl2014', corner_mask=, line_type=SeparateCode,
// fill_type=OuterCode, chunk_size=): the generator matplotlib's ContourSet makes. x, y and z are ny x nx grids
// row by row; non-finite z values are masked.
class Generator {
public:
    static std::unique_ptr<Generator> make(const std::vector<double>& x, const std::vector<double>& y,
                                           const std::vector<double>& z, int ny, int nx, bool cornerMask,
                                           long long chunkSize, std::string* error);
    ~Generator();
    Paths lines(double level);  // lines(level)
    bool filled(double lower, double upper, Paths* out, std::string* error);  // filled(lower, upper)

private:
    Generator() = default;
    std::vector<double> x_, y_, z_;
    std::unique_ptr<bool[]> mask_;
    std::unique_ptr<contourpy::mpl2014::Mpl2014ContourGenerator> gen_;
};

}  // namespace calculate_plot::contour
