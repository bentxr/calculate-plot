// Derived from OpenBLAS 0.3.34: kernel/x86_64/ddot.c and kernel/x86_64/ddot_microk_haswell-2.c.
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

}  // namespace calculate_plot::support
