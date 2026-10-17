// Derived from Matplotlib 3.11.2: lib/matplotlib/ticker.py (LogLocator, LogFormatter,
// LogFormatterMathtext, LogFormatterSciNotation and the decade helpers).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "axes/log_ticker.hpp"

#include "axes/ticker.hpp"
#include "support/log10.hpp"
#include "support/pyformat.hpp"
#include "support/pynum.hpp"
#include "support/raised.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace calculate_plot::axes {

namespace {

// Python's round(x) to an int: halves to even.
double pyRoundInt(double x) { return std::nearbyint(x); }

// Python's int % int (floor modulo)
long long pyMod(long long a, long long b) {
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// LogLocator._log_b
double logB(double x, double base) {
    if (base == 10) return support::log10(x);
    if (base == 2) return std::log2(x);
    return std::log(x) / std::log(base);
}

// _decade_less_equal / _decade_greater_equal (base ** np.floor(np.log(x) / np.log(base)))
double decadeLessEqual(double x, double base);
double decadeGreaterEqual(double x, double base) {
    if (x == 0) return x;
    if (x < 0) return -decadeLessEqual(-x, base);
    return std::pow(base, std::ceil(std::log(x) / std::log(base)));
}
double decadeLessEqual(double x, double base) {
    if (x == 0) return x;
    if (x < 0) return -decadeGreaterEqual(-x, base);
    return std::pow(base, std::floor(std::log(x) / std::log(base)));
}
double decadeGreater(double x, double base);
double decadeLess(double x, double base) {
    if (x < 0) return -decadeGreater(-x, base);
    double less = decadeLessEqual(x, base);
    if (less == x) less /= base;
    return less;
}
double decadeGreater(double x, double base) {
    if (x < 0) return -decadeLess(-x, base);
    double greater = decadeGreaterEqual(x, base);
    if (greater == x) greater *= base;
    return greater;
}

// math.isclose(x, round(x)) with the default tolerances
bool isCloseToInt(double x) {
    const double r = pyRoundInt(x);
    if (x == r) return true;
    return std::fabs(x - r) <= std::max(1e-9 * std::max(std::fabs(x), std::fabs(r)), 0.0);
}

// CPython's math.log(x, base): log(x) / log(base)
double mathLog(double x, double base) { return std::log(x) / std::log(base); }

}  // namespace

std::vector<double> LogLocator::tickValues(double vmin, double vmax) const {
    int nRequest = numticks ? *numticks : (tickSpace ? std::clamp(*tickSpace, 2, 9) : 9);
    const double b = base;
    if (vmin <= 0.0) {
        vmin = minpos;
        if (vmin <= 0.0 || !std::isfinite(vmin)) {
            support::noteRaised("Data cannot be log-scaled because all values are <= 0.");
            return {};
        }
    }
    if (vmax < vmin) std::swap(vmin, vmax);
    const double efmin = logB(vmin, b), efmax = logB(vmax, b);
    // math.ceil(efmin), math.floor(efmax): of a NaN (a negative limit after the swap) or of an infinity
    // (a limit of 0) Python raises
    for (double e : {efmin, efmax}) {
        if (std::isnan(e)) return support::noteRaised("cannot convert float NaN to integer"), std::vector<double>{};
        if (std::isinf(e)) return support::noteRaised("cannot convert float infinity to integer"), std::vector<double>{};
    }
    const long long emin = (long long)std::ceil(efmin);
    const long long emax = (long long)std::floor(efmax);
    const long long nAvail = emax - emin + 1;
    std::vector<double> subsUsed = subs;
    if (autoSubs) {
        if (nAvail >= 10 || b < 3) return {};  // no minor ticks
        subsUsed.clear();
        for (double s = 2.0; s < b; s += 1.0) subsUsed.push_back(s);  // np.arange(2.0, b)
    }
    long long stride = nAvail / (nRequest + 1) + 1;
    const long long nr = (long long)std::ceil(double(nAvail) / double(stride));
    if (nr <= nRequest && !classic) nRequest = int(nr);
    std::vector<long long> decades;
    if (classic) {  // keep historic formulas
        stride = std::max((long long)std::ceil(double(nAvail - 1) / double(nRequest - 1)), 1LL);
        for (long long d = emin - stride; d < emax + stride + 1; d += stride) decades.push_back(d);
    } else if (nRequest == 0) {  // No tick in bounds; two ticks just outside.
        decades = {emin - 1, emax + 1};
        stride = decades[1] - decades[0];
    } else if (nRequest == 1) {  // A single tick close to center.
        const long long mid = (long long)pyRoundInt((efmin + efmax) / 2);
        stride = std::max(mid - (emin - 1), (emax + 1) - mid);
        decades = {mid - stride, mid, mid + stride};
    } else {
        stride = (nAvail - 1) / (nRequest - 1);
        if (double(stride) < double(nAvail) / double(nRequest)) stride = nAvail / nRequest;  // fallback
        const long long olo = std::max(nAvail - stride * nRequest, 0LL);
        const long long ohi = std::min(nAvail - stride * (nRequest - 1), stride);
        long long offset = pyMod(-emin, stride);
        if (!(olo <= offset && offset < ohi)) offset = olo;
        for (long long d = emin + offset - stride; d < emax + stride + 1; d += stride) decades.push_back(d);
    }
    const bool isMinor = subsUsed.size() > 1 || (subsUsed.size() == 1 && subsUsed[0] != 1.0);
    std::vector<double> ticklocs;
    if (isMinor) {
        if (stride == 1 || nAvail <= 1)
            for (long long decade = emin - 1; decade < emax + 1; ++decade) {
                const double scale = std::pow(b, double(decade));  // b**decade
                for (double s : subsUsed) ticklocs.push_back(s * scale);
            }
    } else {
        for (long long d : decades) ticklocs.push_back(std::pow(b, double(d)));  // b ** np.array(decades)
    }
    if (subsUsed.size() > 1 && stride == 1) {
        long long inView = 0;
        for (double t : ticklocs) inView += (vmin <= t && t <= vmax);
        if ((long long)decades.size() - 2 + inView <= 1) return MaxNLocator::autoLocator().tickValues(vmin, vmax);
    }
    return ticklocs;
}

std::pair<double, double> LogLocator::nonsingular(double vmin, double vmax) const {
    if (vmin > vmax) std::swap(vmin, vmax);
    if (!std::isfinite(vmin) || !std::isfinite(vmax)) {
        vmin = 1, vmax = 10;  // Initial range, no data plotted yet.
    } else if (vmax <= 0) {
        vmin = 1, vmax = 10;  // (matplotlib warns: no positive values)
    } else {
        double mp = minpos;
        if (!std::isfinite(mp)) mp = 1e-300;
        if (vmin <= 0) vmin = mp;
        if (vmin == vmax) {
            vmin = decadeLess(vmin, base);
            vmax = decadeGreater(vmax, base);
        }
    }
    return {vmin, vmax};
}

std::pair<double, double> LogLocator::viewLimits(double vmin, double vmax, bool roundNumbers) const {
    std::tie(vmin, vmax) = nonsingular(vmin, vmax);
    if (roundNumbers) {
        vmin = decadeLessEqual(vmin, base);
        vmax = decadeGreaterEqual(vmax, base);
    }
    return {vmin, vmax};
}

std::vector<double> SymmetricalLogLocator::tickValues(double vmin, double vmax) const {
    const double L = linthresh;
    if (vmax < vmin) std::swap(vmin, vmax);
    if (-L <= vmin && vmin < vmax && vmax <= L) {  // sorted({vmin, 0, vmax})
        std::vector<double> out{vmin};
        if (0 != vmin) out.push_back(0);
        if (vmax != vmin && vmax != 0) out.push_back(vmax);
        std::sort(out.begin(), out.end());
        return out;
    }
    const bool hasA = vmin < -L, hasC = vmax > L;
    const bool hasB = (hasA && vmax > -L) || (hasC && vmin < L);
    const double b = base;
    const auto logRange = [&](double lo, double hi) {
        return std::pair<double, double>{std::floor(std::log(lo) / std::log(b)), std::ceil(std::log(hi) / std::log(b))};
    };
    double aLo = 0, aHi = 0, cLo = 0, cHi = 0;
    if (hasA) std::tie(aLo, aHi) = logRange(std::fabs(std::min(-L, vmax)), std::fabs(vmin) + 1);
    if (hasC) std::tie(cLo, cHi) = logRange(std::max(L, vmin), vmax + 1);
    double total = (aHi - aLo) + (cHi - cLo);
    if (hasB) total += 1;
    const double stride = std::max(std::floor(total / (numticks - 1)), 1.0);  // max(total // (numticks - 1), 1)
    // np.arange(lo, hi, stride): lo + i * stride, ceil((hi - lo) / stride) of them
    const auto arange = [](double lo, double hi, double step) {
        std::vector<double> out;
        const double n = std::ceil((hi - lo) / step);
        for (double i = 0; i < n; ++i) out.push_back(i == 0 ? lo : lo + i * step);
        return out;
    };
    std::vector<double> decades;
    if (hasA) {
        auto e = arange(aLo, aHi, stride);
        for (auto it = e.rbegin(); it != e.rend(); ++it) decades.push_back(-1 * std::pow(b, *it));
    }
    if (hasB) decades.push_back(0.0);
    if (hasC)
        for (double e : arange(cLo, cHi, stride)) decades.push_back(std::pow(b, e));
    if (subs.size() > 1 || subs[0] != 1.0) {
        std::vector<double> ticklocs;
        for (double d : decades) {
            if (d == 0) ticklocs.push_back(d);
            else
                for (double s : subs) ticklocs.push_back(s * d);
        }
        return ticklocs;
    }
    return decades;
}

std::pair<double, double> SymmetricalLogLocator::viewLimits(double vmin, double vmax, bool roundNumbers) const {
    const double b = base;
    if (vmax < vmin) std::swap(vmin, vmax);
    if (roundNumbers) {
        vmin = decadeLessEqual(vmin, b);
        vmax = decadeGreaterEqual(vmax, b);
        if (vmin == vmax) {
            vmin = decadeLess(vmin, b);
            vmax = decadeGreater(vmax, b);
        }
    }
    return nonsingular(vmin, vmax);
}

void LogFormatterSciNotation::setLocs(double vmin, double vmax) {
    if (std::isinf(minorThresholds.first)) {
        sublabels_.reset();
        return;
    }
    if (vmin > vmax) std::swap(vmin, vmax);
    if (!linthresh && vmin <= 0) {
        sublabels_ = std::set<double>{1};
        return;
    }
    const double b = base;
    const double inf = std::numeric_limits<double>::infinity();
    double numticks = 0, numdec = 0;
    if (linthresh) {  // symlog: the decades of the logarithmic parts only
        const double L = *linthresh;
        if (vmin < -L) {
            const double rhs = std::min(vmax, -L);
            numticks += std::floor(mathLog(std::fabs(rhs), b)) - std::floor(std::nextafter(mathLog(std::fabs(vmin), b), -inf));
            numdec += mathLog(vmin / rhs, b);
        }
        if (vmax > L) {
            const double lhs = std::max(vmin, L);
            numticks += std::floor(mathLog(vmax, b)) - std::floor(std::nextafter(mathLog(lhs, b), -inf));
            numdec += mathLog(vmax / lhs, b);
        }
    } else {
        const double lmin = mathLog(vmin, b), lmax = mathLog(vmax, b);
        numticks = std::floor(lmax) - std::floor(std::nextafter(lmin, -inf));
        numdec = std::fabs(lmax - lmin);
    }
    if (numticks > minorThresholds.first) {
        sublabels_ = std::set<double>{1};
    } else if (numdec > minorThresholds.second) {
        // set(np.round(np.geomspace(1, b, int(b) // 2 + 1)))
        const int n = int(b) / 2 + 1;
        std::set<double> s;
        for (int i = 0; i < n; ++i) s.insert(std::nearbyint(std::pow(b, double(i) / double(n - 1))));
        sublabels_ = s;
    } else {
        std::set<double> s;
        for (double v = 1; v < b + 1; v += 1) s.insert(v);  // np.arange(1, b + 1)
        sublabels_ = s;
    }
}

std::string LogFormatterSciNotation::operator()(double x) const {
    // LogFormatterMathtext.__call__
    if (x == 0) return "$\\mathdefault{0}$";
    const std::string sign = x < 0 ? "-" : "";
    x = std::fabs(x);
    const double b = base;
    double fx = std::log(x) / std::log(b);
    const bool isDecade = isCloseToInt(fx);
    const double exponent = isDecade ? pyRoundInt(fx) : std::floor(fx);
    const double coeff = pyRoundInt(std::pow(b, fx - exponent));
    if (labelOnlyBase && !isDecade) return "";
    if (sublabels_ && !sublabels_->count(coeff)) return "";
    if (isDecade) fx = pyRoundInt(fx);
    const std::string baseText = std::fmod(b, 1.0) == 0.0 ? std::to_string((long long)b) : support::pythonRepr(b);
    if (std::fabs(fx) < minExponent) return "$\\mathdefault{" + sign + support::percentG(x, 6) + "}$";
    if (!isDecade) {
        // LogFormatterSciNotation._non_decade_format
        const double fb = b;
        const double e = std::floor(fx);
        double c = std::pow(fb, fx - e);
        if (isCloseToInt(c)) c = pyRoundInt(c);
        return "$\\mathdefault{" + sign + support::percentG(c, 6) + "\\times" + baseText + "^{" +
               std::to_string((long long)e) + "}}$";
    }
    return "$\\mathdefault{" + sign + baseText + "^{" + std::to_string((long long)fx) + "}}$";
}

std::vector<double> withoutMajorTicks(const std::vector<double>& minor, const std::vector<double>& major,
                                      double viewMin, double viewMax, double base) {
    const auto tr = [&](double v) { return v <= 0 ? -1000.0 : logB(v, base); };  // LogTransform, nonpositive='clip'
    double lo = tr(viewMin), hi = tr(viewMax);
    if (lo > hi) std::swap(lo, hi);
    const double tol = (hi - lo) * 1e-5;
    std::vector<double> out;
    for (double m : minor) {
        bool close = false;
        for (double M : major) close = close || std::fabs(tr(m) - tr(M)) <= tol;  // np.isclose(atol=tol, rtol=0)
        if (!close) out.push_back(m);
    }
    return out;
}

}  // namespace calculate_plot::axes
