// Derived from NumPy 2.5.3: numpy/_core/src/npymath/npy_math_internal.h.src (npy_divmod) and
// numpy/_core/src/multiarray/calculation.c (power_of_ten, PyArray_Round).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt (BSD-3-Clause). Changes: licensing/changes/numpy.md

#include "support/pynum.hpp"

#include "support/pyformat.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

namespace calculate_plot::support {

std::pair<double, double> divmod(double a, double b) {
    double div, mod, floordiv;

    mod = std::fmod(a, b);
    if (!b) {
        /* b == 0 (not NaN): return result of fmod. For IEEE is nan */
        return {a / b, mod};
    }

    /* a - mod should be very nearly an integer multiple of b */
    div = (a - mod) / b;

    /* adjust fmod result to conform to Python convention of remainder */
    if (mod) {
        if (std::isless(b, 0.0) != std::isless(mod, 0.0)) {
            mod += b;
            div -= 1.0;
        }
    }
    else {
        /* if mod is zero ensure correct sign */
        mod = std::copysign(0.0, b);
    }

    /* snap quotient to nearest integral value */
    if (div) {
        floordiv = std::floor(div);
        if (std::isgreater(div - floordiv, 0.5))
            floordiv += 1.0;
    }
    else {
        /* if div is zero ensure correct sign */
        floordiv = std::copysign(0.0, a / b);
    }

    return {floordiv, mod};
}

// CPython's float floor division (_float_div_mod) is the same algorithm.
double floorDivide(double a, double b) { return divmod(a, b).first; }

namespace {

double powerOfTen(int n) {
    static const double p10[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8};
    double ret;
    if (n < 9) {
        ret = p10[n];
    }
    else {
        ret = 1e9;
        while (n-- > 9) {
            ret *= 10.;
        }
    }
    return ret;
}

}  // namespace

double npRound(double x, int decimals) {
    if (decimals == 0) return std::nearbyint(x);
    if (decimals > 0) {
        const double f = powerOfTen(decimals);
        return std::nearbyint(x * f) / f;
    }
    const double f = powerOfTen(-decimals);
    return std::nearbyint(x / f) * f;
}

}  // namespace calculate_plot::support
