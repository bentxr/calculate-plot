// Derived from NumPy 2.5.3: numpy/_core/src/umath/loops_exponent_log.dispatch.c.src (simd_log_FLOAT) and
// numpy/_core/src/umath/npy_simd_data.h (its coefficients).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md
#pragma once

namespace calculate_plot::support {

// np.log of float32 values as NumPy computes it on x86-64 with AVX2 and FMA (the reference machine): its SIMD
// kernel (a rational approximation of log(1 + x) with fused multiply-adds), one lane at a time.
float npyLogf(float x);

}  // namespace calculate_plot::support
