// Derived from Matplotlib 3.11.2: lib/matplotlib/ticker.py, lib/matplotlib/transforms.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::axes {

// transforms._nonsingular: widens a range too small to divide, orders it.
std::pair<double, double> nonsingular(double vmin, double vmax, double expander = 0.001, double tiny = 1e-15,
                                      bool increasing = true);

// ticker.MaxNLocator (and AutoLocator, its default instance). nbins == 0 means "auto", which is 9
// bins without an axis.
struct MaxNLocator {
    int nbins = 10;
    std::vector<double> steps;  // validated (1 first, 10 last); empty means the defaults
    bool integer = false;
    bool symmetric = false;
    std::string prune;  // "", "lower", "upper" or "both"
    int minNTicks = 2;
    bool roundNumbers = false;
    std::optional<int> tickSpace;  // the axis' get_tick_space(), when the locator has an axis  // rcParams["axes.autolimit_mode"] == "round_numbers"

    static MaxNLocator autoLocator();
    void setSteps(const std::vector<double>& steps);
    std::vector<double> tickValues(double vmin, double vmax) const;
    // MaxNLocator.view_limits (axes.autolimit_mode 'data'; 'round_numbers' when roundNumbers)
    std::pair<double, double> viewLimits(double dmin, double dmax) const;
    std::vector<double> rawTicks(double vmin, double vmax) const;  // _raw_ticks
};

// ticker.ScalarFormatter with the default rcParams (offset on, threshold 4, power limits -5..6,
// no mathtext, no locale, Unicode minus).
class ScalarFormatter {
public:
    void setViewInterval(double vmin, double vmax);
    void setLocs(const std::vector<double>& locs);
    std::string operator()(double x) const;
    std::string offsetText() const;

private:
    std::vector<double> locs_;
    double vmin_ = 0, vmax_ = 0;
    double offset_ = 0;
    int orderOfMagnitude_ = 0;
    int sigfigs_ = 1;
};

}  // namespace calculate_plot::axes
