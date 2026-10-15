// Derived from Matplotlib 3.11.2: lib/matplotlib/ticker.py (LogLocator, LogFormatter,
// LogFormatterMathtext, LogFormatterSciNotation and the decade helpers).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::axes {

// LogLocator(base, subs): subs empty means 'auto' (the minor ticks of a log scale).
struct LogLocator {
    double base = 10.0;
    std::vector<double> subs{1.0};
    bool autoSubs = false;
    std::optional<int> numticks;     // nullopt: 'auto' (from the axis' tick space)
    bool classic = false;            // rcParams["_internal.classic_mode"]: the historic choice of decades
    std::optional<int> tickSpace;    // the axis' get_tick_space(), when the locator has an axis
    double minpos = std::numeric_limits<double>::infinity();  // the axis' get_minpos()

    std::vector<double> tickValues(double vmin, double vmax) const;
    std::pair<double, double> nonsingular(double vmin, double vmax) const;
    std::pair<double, double> viewLimits(double vmin, double vmax, bool roundNumbers) const;
};

// LogFormatterSciNotation(base, labelOnlyBase): mathtext labels such as $\mathdefault{10^{3}}$ or
// $\mathdefault{2\times10^{3}}$.
class LogFormatterSciNotation {
public:
    double base = 10.0;
    bool labelOnlyBase = false;
    std::pair<double, double> minorThresholds{1, 0.4};
    int minExponent = 0;  // rcParams["axes.formatter.min_exponent"]: below it a power is written out

    void setLocs(double viewMin, double viewMax);  // set_locs: which multiples get labels
    std::string operator()(double x) const;

private:
    std::optional<std::set<double>> sublabels_;
};

// Axis.get_minorticklocs for a log scale: the minor ticks not within 1e-5 of the view (in log space)
// of a major tick.
std::vector<double> withoutMajorTicks(const std::vector<double>& minor, const std::vector<double>& major,
                                      double viewMin, double viewMax, double base);

}  // namespace calculate_plot::axes
