#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::support {

// np.sum of a contiguous float64 array: 0.0 (the identity) plus NumPy's pairwise summation of all of
// it (eight accumulators below 128 values, halves above).
double npSum(const std::vector<double>& values);

// np.linspace(start, stop, num) of float64 (endpoint included).
std::vector<double> npLinspace(double start, double stop, long long num);

// np.mean, np.std and np.percentile(a, q) ('linear') of data that are float64, or int64 when
// `integer` (NumPy then casts them to float64 for its sums, in buffers of 8192 values).
double npMean(const std::vector<double>& a, bool integer = false);
double npStd(const std::vector<double>& a, bool integer = false);
double npPercentile(std::vector<double> a, double q);  // (NaN when the data have one)
// np.interp(x, xp, fp, left, right) (arr_interp of compiled_base.c; xp is taken as sorted, as NumPy takes it;
// left and right default to fp's ends)
std::vector<double> npInterp(const std::vector<double>& x, const std::vector<double>& xp, const std::vector<double>& fp,
                             std::optional<double> left = std::nullopt, std::optional<double> right = std::nullopt);
// np.min and np.max as NumPy's scalar loop reduces (of equal values the later one, which tells -0.0
// from 0.0; NaN wins). (Long arrays go through NumPy's SIMD loop, whose order may give the other zero.)
double npMin(const std::vector<double>& a);
double npMax(const std::vector<double>& a);
// np.median: the middle value, or the mean of the two middle ones, as np.mean makes it (from 0.0)
double npMedian(std::vector<double> a, bool integer = false);
// np.cov of one row of data (bias=False): the deviations from the mean, their dot product with
// themselves (np.dot), times 1 / (n - 1)
double npCovRow(const std::vector<double>& a);
// np.linalg.det of a 1x1 matrix: from slogdet, sign * exp(log|u|)
double npDet1(double u);

// The bins of np.histogram: a number of equal bins, their edges, or a rule by name ('auto', 'fd',
// 'doane', 'scott', 'stone', 'rice', 'sturges', 'sqrt'; when `name` is not empty).
struct Bins {
    std::optional<long long> count;
    std::vector<double> edges;
    std::string name;
};

// np.histogram_bin_edges(a, bins, range, weights) and np.histogram(a, bins, range, density,
// weights) of float64 data (int64 data when `integer`: the rules by name then take a width of at
// least 1), with NumPy's arithmetic; false and NumPy's message when it raises. A rule by name that
// asks for more than 100 000 bins is refused.
bool histogramBinEdges(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
                       const std::vector<double>* weights, std::vector<double>* edges, std::string* error,
                       bool integer = false);
bool histogram(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
               bool density, const std::vector<double>* weights, std::vector<double>* counts,
               std::vector<double>* edges, std::string* error, bool integer = false);

}  // namespace calculate_plot::support
