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

// ticker.AutoMinorLocator: the minor ticks of a linear axis, which divide the step between its major
// ticks.
struct AutoMinorLocator {
    std::optional<int> ndivs;  // rcParams["xtick.minor.ndivs"]: the divisions; unset for 'auto' (4 or 5)

    // __call__ for the major tick locations `major` and the view (vmin, vmax). (At most a million
    // ticks are made: more could not be drawn, and a locator of major ticks never asks for them.)
    std::vector<double> operator()(std::vector<double> major, double vmin, double vmax) const;
};

// Axis.get_minorticklocs on a linear scale: the minor ticks that are not on a major one (within a
// hundred-thousandth of the view).
std::vector<double> withoutMajorTicks(const std::vector<double>& minor, const std::vector<double>& major, double viewMin,
                                      double viewMax);

// ticker.ScalarFormatter with the default rcParams (offset on, threshold 4, power limits -5..6,
// no mathtext, no locale, Unicode minus).
class ScalarFormatter {
public:
    // What ScalarFormatter() takes from the rcParams when it is made.
    struct Settings {
        bool useOffset = true;                 // axes.formatter.useoffset
        int offsetThreshold = 4;               // axes.formatter.offset_threshold
        std::pair<int, int> powerLimits = {-5, 6};  // axes.formatter.limits
        bool useMathText = false;              // axes.formatter.use_mathtext
        bool unicodeMinus = true;              // axes.unicode_minus (read when a label is written)
    };
    ScalarFormatter() = default;
    explicit ScalarFormatter(const Settings& settings) : settings_(settings) {}

    void setViewInterval(double vmin, double vmax);
    void setLocs(const std::vector<double>& locs);
    std::string operator()(double x) const;
    std::string offsetText() const;

private:
    std::string fixMinus(const std::string& s) const;
    std::string formatData(double value) const;

    Settings settings_;
    std::vector<double> locs_;
    double vmin_ = 0, vmax_ = 0;
    double offset_ = 0;
    int orderOfMagnitude_ = 0;
    int sigfigs_ = 1;
};

}  // namespace calculate_plot::axes
