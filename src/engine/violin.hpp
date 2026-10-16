// Derived from Matplotlib 3.11.2: lib/matplotlib/mlab.py (GaussianKDE) and lib/matplotlib/cbook.py
// (violin_stats, delete_masked_points).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

// The bandwidth of a Gaussian kernel density estimate: Scott's or Silverman's rule, or a factor given.
struct Bandwidth {
    std::string rule = "scott";
    std::optional<double> factor;
};

// What cbook.violin_stats says of one data set: the density at the coordinates (from the smallest
// to the largest value), the mean, the median, the extremes and the quantiles asked for.
struct ViolinStats {
    std::vector<double> vals, coords, quantiles;
    double mean, median, min, max;
};

// violin_stats for one data set (`integer`: NumPy holds it as int64), its non-finite values dropped;
// false with matplotlib's message when it raises.
bool violinStats(std::vector<double> x, bool integer, const Bandwidth& bw, long long points, const std::vector<double>& quantiles,
                 ViolinStats* out, std::string* error);

}  // namespace calculate_plot::engine
