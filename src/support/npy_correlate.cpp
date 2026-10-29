// Derived from NumPy 2.5.3: numpy/_core/src/multiarray/multiarraymodule.c (_pyarray_correlate) and
// numpy/_core/src/multiarray/arraytypes.c.src (small_correlate, DOUBLE_dot, LONG_dot).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md

#include "support/npy_correlate.hpp"

#include "support/blas_dot.hpp"

#include <cstddef>

namespace calculate_plot::support {

std::vector<double> npCorrelateFull(const std::vector<double>& x, const std::vector<double>& y, bool integer) {
    const std::size_t n = x.size();
    const auto dot = [&](std::size_t x0, std::size_t y0, std::size_t len) {
        if (integer) {
            long long acc = 0;
            for (std::size_t k = 0; k < len; ++k) acc += (long long)x[x0 + k] * (long long)y[y0 + k];
            return double(acc);
        }
        std::vector<double> a(x.begin() + std::ptrdiff_t(x0), x.begin() + std::ptrdiff_t(x0 + len));
        std::vector<double> b(y.begin() + std::ptrdiff_t(y0), y.begin() + std::ptrdiff_t(y0 + len));
        return 0.0 + blasDot(a, b);  // (sum = 0.; sum += cblas_ddot(...))
    };
    std::vector<double> out;
    // mode 2: n - 1 lags on the left (growing overlaps), the full one, n - 1 on the right (shrinking)
    for (std::size_t len = 1; len < n; ++len) out.push_back(dot(0, n - len, len));
    if (!integer && n <= 11) {  // small_correlate
        double acc = 0;
        for (std::size_t k = 0; k < n; ++k) acc += x[k] * y[k];
        out.push_back(acc);
    } else {
        out.push_back(dot(0, 0, n));
    }
    for (std::size_t start = 1; start < n; ++start) out.push_back(dot(start, 0, n - start));
    return out;
}

}  // namespace calculate_plot::support
