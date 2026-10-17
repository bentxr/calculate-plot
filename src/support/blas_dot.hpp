// Derived from OpenBLAS 0.3.34: kernel/x86_64/ddot.c, kernel/x86_64/ddot_microk_haswell-2.c and
// kernel/x86_64/dgemv_n_4.c.
// Copyright (c) 2014, The OpenBLAS Project. All rights reserved.
// Licence: LICENSES/OpenBLAS.txt (BSD-3-Clause). Changes: licensing/changes/openblas.md
#pragma once

#include <array>
#include <vector>

namespace calculate_plot::support {

// x.dot(y) of float64 vectors as the reference machine's OpenBLAS computes it (ddot, its Haswell
// kernel: four fused accumulators of four lanes over blocks of 16 values, then the rest one by one,
// not fused). NumPy's np.dot of float64 vectors calls it; the 'stone' bin rule uses it.
double blasDot(const std::vector<double>& x, const std::vector<double>& y);

// A @ x of a matrix of two rows (given as its columns) as the reference machine's OpenBLAS computes it
// (dgemv_n, the rows past the kernel's blocks of four: two sums of the columns, four at a time as two pairs,
// then the rest one by one, none fused; y = 0 + alpha * sum). NumPy's vector @ (n, 2) matrix calls it, as
// BezierSegment does at one parameter.
std::array<double, 2> blasGemvN2(const std::vector<std::array<double, 2>>& columns, const std::vector<double>& x);

}  // namespace calculate_plot::support
