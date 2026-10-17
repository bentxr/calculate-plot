// Derived from OpenBLAS 0.3.34: kernel/x86_64/ddot.c, kernel/x86_64/ddot_microk_haswell-2.c and
// kernel/x86_64/dgemv_n_4.c.
// Copyright (c) 2014, The OpenBLAS Project. All rights reserved.
// Licence: LICENSES/OpenBLAS.txt (BSD-3-Clause). Changes: licensing/changes/openblas.md

#include "support/blas_dot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace calculate_plot::support {

double blasDot(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t n = std::min(x.size(), y.size());
    const std::size_t n1 = n & ~std::size_t(15);
    double dot = 0.0;
    if (n1) {
        double acc[4][4] = {};  // ymm4 to ymm7
        for (std::size_t i = 0; i < n1; i += 16)
            for (std::size_t k = 0; k < 4; ++k)
                for (std::size_t lane = 0; lane < 4; ++lane)
                    acc[k][lane] = std::fma(x[i + 4 * k + lane], y[i + 4 * k + lane], acc[k][lane]);
        // each accumulator's high lanes onto its low ones (vextractf128, vaddpd), then the
        // accumulators in pairs, then the two lanes (vhaddpd)
        double u[4][2];
        for (std::size_t k = 0; k < 4; ++k) u[k][0] = acc[k][0] + acc[k][2], u[k][1] = acc[k][1] + acc[k][3];
        double s[2];
        for (std::size_t j = 0; j < 2; ++j) s[j] = (u[0][j] + u[1][j]) + (u[2][j] + u[3][j]);
        dot = s[0] + s[1];
    }
    for (std::size_t i = n1; i < n; ++i) dot += y[i] * x[i];  // (not fused)
    return dot;
}

std::array<double, 2> blasGemvN2(const std::vector<std::array<double, 2>>& columns, const std::vector<double>& x) {
    const std::size_t n = std::min(columns.size(), x.size());
    double temp[2] = {0.0, 0.0};
    std::size_t i = 0;
    for (; i < (n & ~std::size_t(3)); i += 4)
        for (int r = 0; r < 2; ++r) {
            temp[r] += columns[i][std::size_t(r)] * x[i] + columns[i + 1][std::size_t(r)] * x[i + 1];
            temp[r] += columns[i + 2][std::size_t(r)] * x[i + 2] + columns[i + 3][std::size_t(r)] * x[i + 3];
        }
    for (; i < n; ++i)
        for (int r = 0; r < 2; ++r) temp[r] += columns[i][std::size_t(r)] * x[i];
    // y was set to 0 (beta 0); y += alpha * temp
    const double alpha = 1.0;
    return {0.0 + alpha * temp[0], 0.0 + alpha * temp[1]};
}

}  // namespace calculate_plot::support
