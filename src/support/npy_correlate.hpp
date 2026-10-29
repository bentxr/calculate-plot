// Derived from NumPy 2.5.3: numpy/_core/src/multiarray/multiarraymodule.c (_pyarray_correlate) and
// numpy/_core/src/multiarray/arraytypes.c.src (small_correlate, DOUBLE_dot, LONG_dot).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md
#pragma once

#include <vector>

namespace calculate_plot::support {

// np.correlate(x, y, mode='full') of two arrays of one length: the lags where they overlap in part are DOUBLE_dot's
// 0.0 + ddot (OpenBLAS's), the full overlap small_correlate's running sum when at most 11 values; int64 data
// (`integer`) LONG_dot's exact sums
std::vector<double> npCorrelateFull(const std::vector<double>& x, const std::vector<double>& y, bool integer);

}  // namespace calculate_plot::support
