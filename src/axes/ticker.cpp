// Derived from Matplotlib 3.11.2: lib/matplotlib/ticker.py (scale_range, _Edge_integer,
// MaxNLocator, AutoLocator, ScalarFormatter) and lib/matplotlib/transforms.py (_nonsingular).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "axes/ticker.hpp"

#include "support/log10.hpp"
#include "support/pyformat.hpp"
#include "support/pynum.hpp"
#include "support/raised.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::axes {

using support::floorDivide;

namespace {

std::pair<double, double> scaleRange(double vmin, double vmax, double n = 1, double threshold = 100) {
    const double dv = std::abs(vmax - vmin);  // > 0 as nonsingular is called before.
    const double meanv = (vmax + vmin) / 2;
    double offset;
    if (std::abs(meanv) / dv < threshold) {
        offset = 0;
    } else {
        offset = std::copysign(std::pow(10.0, floorDivide(support::log10(std::abs(meanv)), 1.0)), meanv);
    }
    const double scale = std::pow(10.0, floorDivide(support::log10(dv / n), 1.0));
    return {scale, offset};
}

// Take floating-point precision limitations into account when calculating tick locations as
// integer multiples of a step.
struct EdgeInteger {
    double step;
    double offset;

    EdgeInteger(double s, double o) : step(s), offset(std::abs(o)) {}

    bool closeto(double ms, double edge) const {
        // Allow more slop when the offset is large compared to the step.
        double tol;
        if (offset > 0) {
            const double digits = support::log10(offset / step);
            tol = std::max(1e-10, std::pow(10.0, digits - 12));
            tol = std::min(0.4999, tol);
        } else {
            tol = 1e-10;
        }
        return std::abs(ms - edge) < tol;
    }

    // The largest n: n*step <= x.
    double le(double x) const {
        const auto [d, m] = support::divmod(x, step);
        if (closeto(m / step, 1)) return d + 1;
        return d;
    }

    // The smallest n: n*step >= x.
    double ge(double x) const {
        const auto [d, m] = support::divmod(x, step);
        if (closeto(m / step, 0)) return d;
        return d + 1;
    }
};

std::vector<double> validateSteps(std::vector<double> steps) {
    if (steps.empty()) steps = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10};
    if (steps.front() != 1) steps.insert(steps.begin(), 1);
    if (steps.back() != 10) steps.push_back(10);
    return steps;
}

std::vector<double> staircase(const std::vector<double>& steps) {
    std::vector<double> out;
    for (std::size_t i = 0; i + 1 < steps.size(); ++i) out.push_back(0.1 * steps[i]);
    out.insert(out.end(), steps.begin(), steps.end());
    out.push_back(10 * steps[1]);
    return out;
}

}  // namespace

std::pair<double, double> nonsingular(double vmin, double vmax, double expander, double tiny, bool increasing) {
    if (!std::isfinite(vmin) || !std::isfinite(vmax)) return {-expander, expander};
    bool swapped = false;
    if (vmax < vmin) {
        std::swap(vmin, vmax);
        swapped = true;
    }
    const double maxabsvalue = std::max(std::abs(vmin), std::abs(vmax));
    if (maxabsvalue < (1e6 / tiny) * std::numeric_limits<double>::min()) {
        vmin = -expander;
        vmax = expander;
    } else if (vmax - vmin <= maxabsvalue * tiny) {
        if (vmax == 0 && vmin == 0) {
            vmin = -expander;
            vmax = expander;
        } else {
            vmin -= expander * std::abs(vmin);
            vmax += expander * std::abs(vmax);
        }
    }
    if (swapped && !increasing) std::swap(vmin, vmax);
    return {vmin, vmax};
}

MaxNLocator MaxNLocator::autoLocator() {
    MaxNLocator l;
    l.nbins = 0;  // 'auto'
    l.setSteps({1, 2, 2.5, 5, 10});
    return l;
}

void MaxNLocator::setSteps(const std::vector<double>& s) { steps = validateSteps(s); }

std::pair<double, double> MaxNLocator::viewLimits(double dmin, double dmax) const {
    if (symmetric) {
        dmax = std::max(std::abs(dmin), std::abs(dmax));
        dmin = -dmax;
    }
    std::tie(dmin, dmax) = nonsingular(dmin, dmax, 1e-12, 1e-13);
    if (roundNumbers) {
        const auto ticks = rawTicks(dmin, dmax);  // _raw_ticks(dmin, dmax)[[0, -1]]
        return {ticks.front(), ticks.back()};
    }
    return {dmin, dmax};
}

std::vector<double> MaxNLocator::tickValues(double vmin, double vmax) const {
    if (symmetric) {
        vmax = std::max(std::abs(vmin), std::abs(vmax));
        vmin = -vmax;
    }
    std::tie(vmin, vmax) = nonsingular(vmin, vmax, 1e-13, 1e-14);
    std::vector<double> ticks = rawTicks(vmin, vmax);
    if (prune == "lower" && !ticks.empty()) ticks.erase(ticks.begin());
    else if (prune == "upper" && !ticks.empty()) ticks.pop_back();
    else if (prune == "both" && ticks.size() >= 2) ticks = std::vector<double>(ticks.begin() + 1, ticks.end() - 1);
    else if (prune == "both") ticks.clear();
    return ticks;
}

int tickSpaceCount(double space) {
    if (std::isnan(space)) return support::noteRaised("cannot convert float NaN to integer"), 9;
    if (std::isinf(space)) return support::noteRaised("cannot convert float infinity to integer"), 9;
    if (space >= double(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
    if (space <= double(std::numeric_limits<int>::min())) return std::numeric_limits<int>::min();
    return int(space);
}

std::vector<double> MaxNLocator::rawTicks(double vmin, double vmax) const {
    // 'auto': np.clip(axis.get_tick_space(), max(1, min_n_ticks - 1), 9), or 9 without an axis
    const int n = nbins == 0 ? (tickSpace ? std::clamp(tickSpaceCount(*tickSpace), std::max(1, minNTicks - 1), 9) : 9) : nbins;
    const auto [scale, offset] = scaleRange(vmin, vmax, n);
    const double vmin_ = vmin - offset;
    const double vmax_ = vmax - offset;
    std::vector<double> stepList;
    for (double s : staircase(validateSteps(steps))) stepList.push_back(s * scale);
    if (integer) {
        std::vector<double> good;
        for (double s : stepList)
            if (s < 1 || std::abs(s - std::nearbyint(s)) < 0.001) good.push_back(s);
        stepList = good;
    }
    const double rawStep = (vmax_ - vmin_) / n;
    std::size_t istep = stepList.size() - 1;
    for (std::size_t i = 0; i < stepList.size(); ++i) {
        bool large = stepList[i] >= rawStep;
        if (roundNumbers) {
            const double flooredVmin = floorDivide(vmin_, stepList[i]) * stepList[i];
            const double flooredVmax = flooredVmin + stepList[i] * n;
            large = large && flooredVmax >= vmax_;
        }
        if (large) {
            istep = i;
            break;
        }
    }
    std::vector<double> ticks;
    for (std::size_t k = istep + 1; k-- > 0;) {
        double step = stepList[k];
        if (integer && std::floor(vmax_) - std::ceil(vmin_) >= minNTicks - 1) step = std::max(1.0, step);
        const double bestVmin = floorDivide(vmin_, step) * step;
        const EdgeInteger edge(step, offset);
        const double low = edge.le(vmin_ - bestVmin);
        const double high = edge.ge(vmax_ - bestVmin);
        // np.arange(low, high + 1) * step + best_vmin
        ticks.clear();
        const double stop = high + 1;
        const double count = std::ceil(stop - low);
        for (double i = 0; i < count; ++i) ticks.push_back((low + i) * step + bestVmin);
        int nticks = 0;
        for (double t : ticks) nticks += (t <= vmax_ && t >= vmin_);
        if (nticks >= minNTicks) break;
    }
    for (double& t : ticks) t = t + offset;
    return ticks;
}

std::vector<double> AutoMinorLocator::operator()(std::vector<double> major, double vmin, double vmax) const {
    // np.unique(self.axis.get_majorticklocs())
    std::sort(major.begin(), major.end());
    major.erase(std::unique(major.begin(), major.end()), major.end());
    // Need at least two major ticks to find minor tick locations.
    if (major.size() < 2) return {};
    const double majorstep = major[1] - major[0];
    int divisions = 0;
    if (ndivs) {
        divisions = *ndivs;
    } else {
        // 'auto': five where the step is 1, 2.5, 5 or 10 times a power of ten, else four
        const double mantissa = std::pow(10.0, support::divmod(support::log10(majorstep), 1.0).second);
        divisions = 4;
        for (double round : {1.0, 2.5, 5.0, 10.0})
            if (std::abs(mantissa - round) <= 1e-8 + 1e-5 * round) divisions = 5;  // np.isclose
    }
    const double minorstep = majorstep / divisions;  // (no divisions: an infinite step, and a tick at nan)
    if (vmin > vmax) std::swap(vmin, vmax);
    const double t0 = major[0];
    const double tmin = std::nearbyint((vmin - t0) / minorstep);
    const double tmax = std::nearbyint((vmax - t0) / minorstep) + 1;
    // np.arange(tmin, tmax) * minorstep + t0
    std::vector<double> locs;
    for (double i = tmin; i < tmax && locs.size() < 1000000; i += 1) locs.push_back(i * minorstep + t0);
    return locs;
}

std::vector<double> withoutMajorTicks(const std::vector<double>& minor, const std::vector<double>& major, double viewMin,
                                      double viewMax) {
    const double tol = std::abs(viewMax - viewMin) * 1e-5;
    std::vector<double> out;
    for (double m : minor) {
        bool close = false;
        for (double M : major) close = close || std::abs(m - M) <= tol;  // np.isclose(atol=tol, rtol=0)
        if (!close) out.push_back(m);
    }
    return out;
}

// Formatter.fix_minus: U+2212 MINUS SIGN for the hyphens, if rcParams["axes.unicode_minus"]
std::string ScalarFormatter::fixMinus(const std::string& s) const {
    if (!settings_.unicodeMinus) return s;
    std::string out;
    for (char c : s) {
        if (c == '-') out += "\xe2\x88\x92";
        else out.push_back(c);
    }
    return out;
}

void ScalarFormatter::setViewInterval(double vmin, double vmax) {
    vmin_ = vmin;
    vmax_ = vmax;
}

namespace {

// The visible ticks: within the sorted view interval.
std::vector<double> visible(const std::vector<double>& locs, double a, double b) {
    const double vmin = std::min(a, b), vmax = std::max(a, b);
    std::vector<double> out;
    for (double l : locs)
        if (vmin <= l && l <= vmax) out.push_back(l);
    return out;
}

}  // namespace

void ScalarFormatter::setLocs(const std::vector<double>& locs) {
    locs_ = locs;
    if (locs_.empty()) return;

    // _compute_offset, if useOffset
    offset_ = 0;
    const auto vis = visible(locs_, vmin_, vmax_);
    if (settings_.useOffset && !vis.empty()) {
        const double lmin = *std::min_element(vis.begin(), vis.end());
        const double lmax = *std::max_element(vis.begin(), vis.end());
        // Only use offset if there are at least two ticks and every tick has the same sign.
        if (!(lmin == lmax || (lmin <= 0 && 0 <= lmax))) {
            double absMin = std::abs(lmin), absMax = std::abs(lmax);
            if (absMin > absMax) std::swap(absMin, absMax);
            const double sign = std::copysign(1.0, lmin);
            const double oomMax = std::ceil(support::log10(absMax));
            // The smallest power of ten at which abs_min and abs_max are equal.
            double oom = oomMax;
            for (int guard = 0; guard < 4000 && floorDivide(absMin, std::pow(10.0, oom)) ==
                                                     floorDivide(absMax, std::pow(10.0, oom));
                 ++guard)
                oom -= 1;
            oom = 1 + oom;
            if ((absMax - absMin) / std::pow(10.0, oom) <= 1e-2) {
                // Straddling a multiple of a large power of ten (relative to the span): the smallest
                // power of ten at which abs_min and abs_max are no more than 1 apart.
                oom = oomMax;
                for (int guard = 0; guard < 4000 && !(floorDivide(absMax, std::pow(10.0, oom)) -
                                                          floorDivide(absMin, std::pow(10.0, oom)) >
                                                      1);
                     ++guard)
                    oom -= 1;
                oom = 1 + oom;
            }
            // Only use offset if it saves at least _offset_threshold digits. (10**n is an integer in
            // Python for n >= 0, exact here up to 10**22, and a float below 1 otherwise.)
            const double q = floorDivide(absMax, std::pow(10.0, oom));
            offset_ = q >= std::pow(10.0, settings_.offsetThreshold - 1) ? sign * q * std::pow(10.0, oom) : 0;
        }
    }

    // _set_order_of_magnitude (scientific)
    orderOfMagnitude_ = 0;
    std::vector<double> absVis = visible(locs_, vmin_, vmax_);
    for (double& v : absVis) v = std::abs(v);
    const auto [lowPower, highPower] = settings_.powerLimits;
    if (lowPower == highPower && lowPower != 0) {
        // fixed scaling
        orderOfMagnitude_ = lowPower;
    } else if (!absVis.empty()) {
        int oom;
        if (offset_ != 0) {
            const double vmin = std::min(vmin_, vmax_), vmax = std::max(vmin_, vmax_);
            oom = int(std::floor(support::log10(vmax - vmin)));
        } else {
            const double val = *std::max_element(absVis.begin(), absVis.end());
            oom = val == 0 ? 0 : int(std::floor(support::log10(val)));
        }
        if (oom <= lowPower || oom >= highPower) orderOfMagnitude_ = oom;
    }

    // _set_format
    std::vector<double> l = locs_;
    if (locs_.size() < 2) {
        l.push_back(vmin_);  // the axis' view interval, unsorted
        l.push_back(vmax_);
    }
    const double scale = std::pow(10.0, orderOfMagnitude_);
    for (double& v : l) v = (v - offset_) / scale;
    double locRange = *std::max_element(l.begin(), l.end()) - *std::min_element(l.begin(), l.end());
    if (locRange == 0) {
        locRange = 0;
        for (double v : l) locRange = std::max(locRange, std::abs(v));
    }
    if (locRange == 0) locRange = 1;
    if (locs_.size() < 2) l.resize(l.size() - 2);
    const int locRangeOom = int(std::floor(support::log10(locRange)));
    int sigfigs = std::max(0, 3 - locRangeOom);
    const double thresh = 1e-3 * std::pow(10.0, locRangeOom);
    while (sigfigs >= 0) {
        double worst = 0;
        for (double v : l) worst = std::max(worst, std::abs(v - support::npRound(v, sigfigs)));
        if (!l.empty() && worst < thresh) sigfigs -= 1;
        else break;
    }
    sigfigs_ = sigfigs + 1;
}

std::string ScalarFormatter::operator()(double x) const {
    if (locs_.empty()) return "";
    double xp = (x - offset_) / std::pow(10.0, orderOfMagnitude_);
    if (std::abs(xp) < 1e-8) xp = 0;  // an int 0 in Python: never "-0.0"
    const std::string number = support::percentF(xp, sigfigs_);
    // _format is '$\mathdefault{%1.nf}$' with mathtext
    return fixMinus(settings_.useMathText ? "$\\mathdefault{" + number + "}$" : number);
}

// ScalarFormatter.format_data, for the offset: "1e6", "1.5e-3", "12345"; with mathtext
// "1.5 \times 10^{-3}", and "10^{6}" for 1e6.
std::string ScalarFormatter::formatData(double value) const {
    const int e = int(std::floor(support::log10(std::abs(value))));
    const double s = support::pythonRound(value / std::pow(10.0, e), 10);
    const bool integral = support::divmod(s, 1.0).second == 0;
    // (matplotlib fixes the minus of each part here; get_offset, its one user, does it for the whole)
    const std::string significand = integral ? support::percentF(std::trunc(s), 0) : support::percentG(s, 10);
    if (e == 0) return significand;
    const std::string exponent = std::to_string(e);
    if (!settings_.useMathText) return significand + "e" + exponent;
    const std::string power = "10^{" + exponent + "}";
    return s == 1 ? power : significand + " \\times " + power;  // reformat 1x10^y as 10^y
}

std::string ScalarFormatter::offsetText() const {
    if (locs_.empty()) return "";
    if (orderOfMagnitude_ == 0 && offset_ == 0) return "";
    std::string offsetStr, sciNotStr;
    if (offset_ != 0) {
        offsetStr = formatData(offset_);
        if (offset_ > 0) offsetStr = "+" + offsetStr;
    }
    if (!settings_.useMathText) {
        if (orderOfMagnitude_ != 0) sciNotStr = "1e" + std::to_string(orderOfMagnitude_);
        return fixMinus(sciNotStr + offsetStr);
    }
    // 10 ** orderOfMagnitude: an integer in Python, and a float for a negative power
    if (orderOfMagnitude_ != 0) sciNotStr = "\\times\\mathdefault{" + formatData(std::pow(10.0, orderOfMagnitude_)) + "}";
    return fixMinus("$" + sciNotStr + "\\mathdefault{" + offsetStr + "}$");
}

support::Formatted formatStrFormatter(const std::string& fmt, double x) { return support::percentFormat(fmt, x, false, true); }

support::Formatted strMethodFormatter(const std::string& fmt, double x, int pos, bool unicodeMinus) {
    return support::keywordFormat(fmt, {{"x", x, false}, {"pos", double(pos), true}}, unicodeMinus);
}

}  // namespace calculate_plot::axes
