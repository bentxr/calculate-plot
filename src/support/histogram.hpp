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
double npPercentile(std::vector<double> a, double q);

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
