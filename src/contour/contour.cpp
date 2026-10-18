// Derived from ContourPy 1.4.0: lib/contourpy/__init__.py (contour_generator, _remove_z_mask) and
// lib/contourpy/chunk.py (calc_chunk_sizes), for the mpl2014 generator as matplotlib calls it.
// Copyright (c) 2021-2026, ContourPy Developers. All rights reserved.
// Licence: LICENSES/contourpy.txt. Changes: licensing/changes/contourpy.md

#include "contour/contour.hpp"

#include "contour/mpl2014.h"

#include <cmath>

namespace calculate_plot::contour {

Generator::~Generator() = default;

std::unique_ptr<Generator> Generator::make(const std::vector<double>& x, const std::vector<double>& y,
                                           const std::vector<double>& z, int ny, int nx, bool cornerMask,
                                           long long chunkSize, std::string* error) {
    if (ny < 2 || nx < 2) {
        *error = "Input z must be at least a (2, 2) shaped array, but has shape (" + std::to_string(ny) + ", " + std::to_string(nx) + ")";
        return nullptr;
    }
    const std::size_t n = std::size_t(ny) * std::size_t(nx);
    if (x.size() != n || y.size() != n || z.size() != n) {
        *error = "x, y and z arrays must have the same shape";
        return nullptr;
    }
    if (chunkSize < 0) {
        *error = "chunk_size cannot be negative";
        return nullptr;
    }
    std::unique_ptr<Generator> g(new Generator());
    g->x_ = x;
    g->y_ = y;
    g->z_ = z;
    // _remove_z_mask: masked where not finite; no mask when nothing is
    bool any = false;
    for (double v : z) any = any || !std::isfinite(v);
    contourpy::MaskArray mask;
    if (any) {
        g->mask_.reset(new bool[n]);
        for (std::size_t i = 0; i < n; ++i) g->mask_[i] = !std::isfinite(z[i]);
        mask = contourpy::MaskArray(g->mask_.get(), ny, nx);
    }
    g->gen_ = std::make_unique<contourpy::mpl2014::Mpl2014ContourGenerator>(
        contourpy::CoordinateArray(g->x_.data(), ny, nx), contourpy::CoordinateArray(g->y_.data(), ny, nx),
        contourpy::CoordinateArray(g->z_.data(), ny, nx), mask, cornerMask, chunkSize, chunkSize);
    if (const char* why = g->gen_->invalid()) {
        *error = why;
        return nullptr;
    }
    return g;
}

Paths Generator::lines(double level) {
    auto t = gen_->lines(level);
    return {std::move(t.points), std::move(t.codes)};
}

bool Generator::filled(double lower, double upper, Paths* out, std::string* error) {
    // ContourGenerator.check_levels
    if (std::isnan(lower) || std::isnan(upper)) return *error = "lower_level and upper_level cannot be NaN", false;
    if (lower >= upper) return *error = "upper_level must be larger than lower_level", false;
    auto t = gen_->filled(lower, upper);
    *out = {std::move(t.points), std::move(t.codes)};
    return true;
}

}  // namespace calculate_plot::contour
