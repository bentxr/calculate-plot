// Derived from NumPy 2.5.3: numpy/lib/_histograms_impl.py (histogram, histogram_bin_edges, the bin
// rules of _hist_bin_selectors and their helpers), numpy/_core/function_base.py (linspace),
// numpy/_core/_methods.py (_mean, _var, _std), numpy/lib/_function_base_impl.py (percentile with the
// 'linear' method: _quantile, _get_indexes, _lerp) and numpy/_core/src/umath/loops_utils.h.src
// (pairwise_sum).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt (BSD-3-Clause). Changes: licensing/changes/numpy.md

#include "support/histogram.hpp"

#include "support/blas_dot.hpp"
#include "support/pyformat.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <tuple>

namespace calculate_plot::support {

namespace {

// DOUBLE_pairwise_sum
double pairwiseSum(const double* a, std::size_t n) {
    if (n < 8) {
        double res = 0.;
        for (std::size_t i = 0; i < n; ++i) res += a[i];
        return res;
    }
    if (n <= 128) {
        double r[8];
        for (std::size_t j = 0; j < 8; ++j) r[j] = a[j];
        std::size_t i = 8;
        for (; i < n - (n % 8); i += 8)
            for (std::size_t j = 0; j < 8; ++j) r[j] += a[i + j];
        double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; ++i) res += a[i];
        return res;
    }
    std::size_t n2 = n / 2;
    n2 -= n2 % 8;
    return pairwiseSum(a, n2) + pairwiseSum(a + n2, n - n2);
}

// np.add.reduce of int64 data cast to float64: the iterator casts them in buffers of 8192 values, and
// each buffer's pairwise sum is added to the running total
double castSum(const std::vector<double>& a) {
    constexpr std::size_t buffer = 8192;
    double total = 0.0;
    for (std::size_t start = 0; start < a.size(); start += buffer)
        total += pairwiseSum(a.data() + start, std::min(buffer, a.size() - start));
    return total;
}

// np.sort and np.searchsorted order: NaN after every number
bool nanLast(double a, double b) { return !std::isnan(a) && (std::isnan(b) || a < b); }

std::string rangeText(double first, double last) { return "[" + pythonRepr(first) + ", " + pythonRepr(last) + "]"; }

// _get_outer_edges
bool outerEdges(const std::vector<double>& a, std::optional<std::pair<double, double>> range, double* first,
                double* last, std::string* error) {
    if (range) {
        std::tie(*first, *last) = *range;
        if (*first > *last) return *error = "max must be larger than min in range parameter.", false;
        if (!(std::isfinite(*first) && std::isfinite(*last)))
            return *error = "supplied range of " + rangeText(*first, *last) + " is not finite", false;
    } else if (a.empty()) {
        *first = 0, *last = 1;
    } else {
        // a.min() and a.max(): NaN if there is one
        *first = a[0], *last = a[0];
        for (double v : a) {
            if (std::isnan(v) || std::isnan(*first)) *first = *last = std::nan("");
            else *first = std::min(*first, v), *last = std::max(*last, v);
        }
        if (!(std::isfinite(*first) && std::isfinite(*last)))
            return *error = "autodetected range of " + rangeText(*first, *last) + " is not finite", false;
    }
    if (*first == *last) {
        *first = *first - 0.5;
        *last = *last + 0.5;
    }
    return true;
}

struct Uniform {
    double first, last;
    long long count;
};

// _ptp: x.max() - x.min() (of int64 data, exact as unsigned)
double ptp(const std::vector<double>& x) {
    const auto [lo, hi] = std::minmax_element(x.begin(), x.end());
    return *hi - *lo;
}

// np.histogram(x, bins=n, range=range)[0] of finite data
std::vector<double> counts(const std::vector<double>& x, long long n, std::pair<double, double> range);

// The bin rules (_hist_bin_selectors): the width of a bin, for data trimmed to the range, not empty
double binWidth(const std::string& name, const std::vector<double>& x, std::pair<double, double> range, bool integer) {
    const double n = double(x.size());
    const auto sqrtRule = [&] { return ptp(x) / std::sqrt(n); };
    const auto sturges = [&] { return ptp(x) / (std::log2(n) + 1.0); };
    const auto fd = [&] {
        const double iqr = npPercentile(x, 75) - npPercentile(x, 25);
        return 2.0 * iqr * std::pow(n, -1.0 / 3.0);
    };
    if (name == "sqrt") return sqrtRule();
    if (name == "sturges") return sturges();
    if (name == "rice") return ptp(x) / (2.0 * std::pow(n, 1.0 / 3));
    if (name == "scott") return std::pow(24.0 * std::pow(M_PI, 0.5) / n, 1.0 / 3.0) * npStd(x, integer);
    if (name == "fd") return fd();
    if (name == "auto") {
        const double fdBw = fd(), sturgesBw = sturges(), sqrtBw = sqrtRule();
        const double corrected = sqrtBw / 2 > fdBw ? sqrtBw / 2 : fdBw;  // max(fd_bw, sqrt_bw / 2)
        return sturgesBw < corrected ? sturgesBw : corrected;              // min(..., sturges_bw)
    }
    if (name == "doane") {
        if (x.size() > 2) {
            const double sg1 = std::sqrt(6.0 * (n - 2) / ((n + 1.0) * (n + 3)));
            const double sigma = npStd(x, integer);
            if (sigma > 0.0) {
                const double mean = npMean(x, integer);
                std::vector<double> temp;
                for (double v : x) temp.push_back(std::pow((v - mean) / sigma, 3.0));
                const double g1 = npMean(temp);
                return ptp(x) / (1.0 + std::log2(n) + std::log2(1.0 + std::fabs(g1) / sg1));
            }
        }
        return 0.0;
    }
    // stone: the count of bins (1 to max(100, int(sqrt(n)))) whose estimated ISE is least, the first
    // of equal ones
    const double ptpX = ptp(x);
    if (x.size() <= 1 || ptpX == 0) return 0;
    const long long upper = std::max(100LL, static_cast<long long>(std::sqrt(n)));
    long long best = 0;
    double least = 0;
    for (long long nbins = 1; nbins <= upper; ++nbins) {
        const double hh = ptpX / double(nbins);
        std::vector<double> p = counts(x, nbins, range);
        for (double& v : p) v = v / n;
        const double jhat = (2 - (n + 1) * blasDot(p, p)) / hh;
        if (nbins == 1 || jhat < least) best = nbins, least = jhat;
    }
    return ptpX / double(best);
}

// _ravel_and_check_weights and _get_bin_edges
bool binEdges(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
              const std::vector<double>* weights, std::vector<double>* edges, std::optional<Uniform>* uniform,
              std::string* error, bool integer) {
    if (weights && weights->size() != a.size()) return *error = "weights should have the same shape as a.", false;
    if (!bins.name.empty()) {
        static const char* const names[] = {"stone", "auto", "doane", "fd", "rice", "scott", "sqrt", "sturges"};
        if (std::find(std::begin(names), std::end(names), bins.name) == std::end(names))
            return *error = pythonStrRepr(bins.name) + " is not a valid estimator for `bins`", false;
        if (weights) return *error = "Automated estimation of the number of bins is not supported for weighted data", false;
        double first = 0, last = 0;
        if (!outerEdges(a, range, &first, &last, error)) return false;
        std::vector<double> kept = a;
        if (range) {  // truncated to the range (NaN goes too)
            kept.clear();
            for (double v : a)
                if (v >= first && v <= last) kept.push_back(v);
        }
        long long n = 1;
        if (!kept.empty()) {
            double width = binWidth(bins.name, kept, {first, last}, integer);
            if (width != 0) {  // (NaN is true there, and int(nan) raises)
                if (integer && width < 1) width = 1;
                const double count = std::ceil((last - first) / width);
                if (std::isnan(count)) return *error = "cannot convert float NaN to integer", false;
                if (!(count <= 100000)) return *error = "more than 100000 bins are not supported", false;
                n = static_cast<long long>(count);
            }
        }
        *edges = npLinspace(first, last, n + 1);
        for (std::size_t i = 1; i < edges->size(); ++i)
            if ((*edges)[i - 1] >= (*edges)[i])
                return *error = "Too many bins for data range. Cannot create " + std::to_string(n) + " finite-sized bins.", false;
        *uniform = Uniform{first, last, n};
        return true;
    }
    if (!bins.count) {
        for (std::size_t i = 1; i < bins.edges.size(); ++i)
            if (bins.edges[i - 1] > bins.edges[i]) return *error = "`bins` must increase monotonically, when an array", false;
        *edges = bins.edges;
        return true;
    }
    const long long n = *bins.count;
    if (n < 1) return *error = "`bins` must be positive, when an integer", false;
    double first = 0, last = 0;
    if (!outerEdges(a, range, &first, &last, error)) return false;
    *edges = npLinspace(first, last, n + 1);
    for (std::size_t i = 1; i < edges->size(); ++i)
        if ((*edges)[i - 1] >= (*edges)[i])
            return *error = "Too many bins for data range. Cannot create " + std::to_string(n) + " finite-sized bins.", false;
    *uniform = Uniform{first, last, n};
    return true;
}

constexpr std::size_t BLOCK = 65536;

}  // namespace

double npSum(const std::vector<double>& values) { return 0.0 + pairwiseSum(values.data(), values.size()); }

double npMean(const std::vector<double>& a, bool integer) {
    const double sum = integer ? castSum(a) : npSum(a);
    return sum / double(a.size());
}

double npStd(const std::vector<double>& a, bool integer) {
    // _var: the mean (a sum kept as an array), the squared deviations, their sum over the count
    const double mean = npMean(a, integer);
    std::vector<double> d;
    d.reserve(a.size());
    for (double v : a) {
        const double x = v - mean;
        d.push_back(x * x);
    }
    return std::sqrt(npSum(d) / double(a.size()));
}

double npPercentile(std::vector<double> a, double q) {
    // (data with a NaN, which sorts last: the result is NaN)
    if (std::any_of(a.begin(), a.end(), [](double v) { return std::isnan(v); })) return std::nan("");
    std::sort(a.begin(), a.end(), nanLast);
    const std::size_t n = a.size();
    const double virtualIndex = double(n - 1) * (q / 100);  // (n - 1) * quantiles
    // _get_indexes: the neighbours, the last value (index -1) at or above the end, the first below 0
    double previous = std::floor(virtualIndex), next = previous + 1;
    if (virtualIndex >= double(n) - 1) previous = next = -1;
    if (virtualIndex < 0) previous = next = 0;
    const auto at = [&](double i) { return a[i < 0 ? n - 1 : std::size_t(i)]; };
    const double gamma = virtualIndex - previous;  // _get_gamma (of the clamped index)
    // _lerp
    const double lo = at(previous), hi = at(next), diff = hi - lo;
    return gamma >= 0.5 ? hi - diff * (1 - gamma) : lo + diff * gamma;
}

std::vector<double> npLinspace(double start, double stop, long long num) {
    std::vector<double> y;
    if (num <= 0) return y;
    const long long div = num - 1;
    const double delta = stop - start;
    for (long long i = 0; i < num; ++i) y.push_back(double(i));
    if (div > 0) {
        const double step = delta / double(div);
        if (step == 0) {
            for (double& v : y) v = (v / double(div)) * delta;
        } else {
            for (double& v : y) v = v * step;
        }
    } else {
        for (double& v : y) v = v * delta;  // (step = nan, y = y * delta)
    }
    for (double& v : y) v += start;
    if (num > 1) y.back() = stop;
    return y;
}

bool histogramBinEdges(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
                       const std::vector<double>* weights, std::vector<double>* edges, std::string* error, bool integer) {
    std::optional<Uniform> uniform;
    return binEdges(a, bins, range, weights, edges, &uniform, error, integer);
}

bool histogram(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
               bool density, const std::vector<double>* weights, std::vector<double>* counts,
               std::vector<double>* edges, std::string* error, bool integer) {
    std::optional<Uniform> uniform;
    if (!binEdges(a, bins, range, weights, edges, &uniform, error, integer)) return false;
    std::vector<double>& n = *counts;
    if (uniform) {
        const long long nBins = uniform->count;
        const double first = uniform->first, last = uniform->last;
        n.assign(std::size_t(nBins), 0.0);
        const double normDenom = last - first;
        const double normNumerator = double(nBins);
        for (std::size_t start = 0; start < a.size(); start += BLOCK) {
            const std::size_t end = std::min(a.size(), start + BLOCK);
            std::vector<double> block(std::size_t(nBins), 0.0);  // np.bincount of this block
            for (std::size_t k = start; k < end; ++k) {
                const double v = a[k];
                if (!(v >= first && v <= last)) continue;  // keep
                const double f = ((v - first) / normDenom) * normNumerator;
                long long index = f >= normNumerator ? nBins : static_cast<long long>(f);  // astype(np.intp)
                if (index == nBins) index -= 1;
                if (v < (*edges)[std::size_t(index)]) index -= 1;  // decrement
                else if (v >= (*edges)[std::size_t(index) + 1] && index != nBins - 1) index += 1;  // increment
                block[std::size_t(index)] += weights ? (*weights)[k] : 1.0;
            }
            for (std::size_t b = 0; b < n.size(); ++b) n[b] += block[b];
        }
    } else {
        std::vector<double> cum(edges->size(), 0.0);
        const auto searchInclusive = [&](const std::vector<double>& sorted, std::vector<std::size_t>* out) {
            // _search_sorted_inclusive: 'left' for all edges but the last, 'right' for the last
            out->clear();
            for (std::size_t e = 0; e < edges->size(); ++e) {
                const double v = (*edges)[e];
                const auto it = e + 1 < edges->size() ? std::lower_bound(sorted.begin(), sorted.end(), v, nanLast)
                                                      : std::upper_bound(sorted.begin(), sorted.end(), v, nanLast);
                out->push_back(std::size_t(it - sorted.begin()));
            }
        };
        std::vector<std::size_t> index;
        for (std::size_t start = 0; start < a.size(); start += BLOCK) {
            const std::size_t end = std::min(a.size(), start + BLOCK);
            if (!weights) {
                std::vector<double> sorted(a.begin() + std::ptrdiff_t(start), a.begin() + std::ptrdiff_t(end));
                std::sort(sorted.begin(), sorted.end(), nanLast);
                searchInclusive(sorted, &index);
                for (std::size_t e = 0; e < cum.size(); ++e) cum[e] += double(index[e]);
            } else {
                // np.argsort (NumPy's sort is not stable: weights of equal values may be summed in
                // another order there)
                std::vector<std::size_t> order(end - start);
                std::iota(order.begin(), order.end(), start);
                std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) { return nanLast(a[x], a[y]); });
                std::vector<double> sorted, cw{0.0};
                for (std::size_t k : order) {
                    sorted.push_back(a[k]);
                    cw.push_back(cw.back() + (*weights)[k]);
                }
                searchInclusive(sorted, &index);
                for (std::size_t e = 0; e < cum.size(); ++e) cum[e] += cw[index[e]];
            }
        }
        n.clear();
        for (std::size_t e = 1; e < cum.size(); ++e) n.push_back(cum[e] - cum[e - 1]);
    }
    if (density) {
        const double total = npSum(n);
        for (std::size_t b = 0; b < n.size(); ++b) n[b] = n[b] / ((*edges)[b + 1] - (*edges)[b]) / total;
    }
    return true;
}

namespace {

std::vector<double> counts(const std::vector<double>& x, long long n, std::pair<double, double> range) {
    Bins b;
    b.count = n;
    std::vector<double> c, e;
    std::string error;
    histogram(x, b, range, false, nullptr, &c, &e, &error);
    return c;
}

}  // namespace

}  // namespace calculate_plot::support
