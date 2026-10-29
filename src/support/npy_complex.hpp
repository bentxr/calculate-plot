// Derived from NumPy 2.5.3: numpy/_core/src/umath/loops_arithm_fp.dispatch.c.src (simd_cmul_f64) and
// numpy/_core/src/umath/loops_unary_complex.dispatch.c.src (simd_cabsolute_f64).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md
#pragma once

namespace calculate_plot::support {

// A complex128 value
struct NpyComplex {
    double re, im;
};

// a * b as NumPy multiplies complex128 arrays on x86 with AVX2 and FMA (simd_cmul_f64): the imaginary parts'
// products rounded, then fused with the real part's
NpyComplex npyMultiply(NpyComplex a, NpyComplex b);

// np.abs of a complex128 value on x86 with AVX2 and FMA (simd_cabsolute_f64): the larger part times
// sqrt(1 + ratio ** 2), the square fused; infinities and NaN as it treats them
double npyAbsolute(NpyComplex z);

}  // namespace calculate_plot::support
