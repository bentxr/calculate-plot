// Derived from OpenBLAS 0.3.34: kernel/x86_64/ddot.c and kernel/x86_64/ddot_microk_haswell-2.c.
// Copyright (c) 2014, The OpenBLAS Project. All rights reserved.
// Licence: LICENSES/OpenBLAS.txt (BSD-3-Clause). Changes: licensing/changes/openblas.md
#pragma once

#include <vector>

namespace calculate_plot::support {

// x.dot(y) of float64 vectors as the reference machine's OpenBLAS computes it (ddot, its Haswell
// kernel: four fused accumulators of four lanes over blocks of 16 values, then the rest one by one,
// not fused). NumPy's np.dot of float64 vectors calls it; the 'stone' bin rule uses it.
double blasDot(const std::vector<double>& x, const std::vector<double>& y);

}  // namespace calculate_plot::support
