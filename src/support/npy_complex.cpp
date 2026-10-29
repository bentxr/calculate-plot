// Derived from NumPy 2.5.3: numpy/_core/src/umath/loops_arithm_fp.dispatch.c.src (simd_cmul_f64) and
// numpy/_core/src/umath/loops_unary_complex.dispatch.c.src (simd_cabsolute_f64).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md

#include "support/npy_complex.hpp"

#include <cmath>
#include <limits>

namespace calculate_plot::support {

NpyComplex npyMultiply(NpyComplex a, NpyComplex b) {
    // ab_iiir = a_im * b_rev; npyv_muladdsub_f64(a_re, b, ab_iiir) (_mm256_fmaddsub_pd)
    return {std::fma(a.re, b.re, -(a.im * b.im)), std::fma(a.re, b.im, a.im * b.re)};
}

double npyAbsolute(NpyComplex z) {
    const double inf = std::numeric_limits<double>::infinity(), nan = std::numeric_limits<double>::quiet_NaN();
    double re = std::fabs(z.re), im = std::fabs(z.im);
    // an infinite part makes both infinite; then a NaN part both NaN
    const bool reInf = re == inf, imInf = im == inf;
    im = reInf ? inf : im;
    re = imInf ? inf : re;
    const bool reNumber = !std::isnan(re), imNumber = !std::isnan(im);
    im = reNumber ? im : nan;
    re = imNumber ? re : nan;
    const double larger = re > im ? re : im;   // npyv_max_f64(re, im)
    const double smaller = im < re ? im : re;  // npyv_min_f64(im, re)
    const double ratio = larger == 0 || smaller == inf ? 0.0 : smaller / larger;  // npyv_ifdivz_f64
    return std::sqrt(std::fma(ratio, ratio, 1.0)) * larger;
}

}  // namespace calculate_plot::support
