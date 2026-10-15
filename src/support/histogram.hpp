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

// The bins of np.histogram: a number of equal bins, or their edges. (The rules given by name,
// bins='auto' and the like, are not supported yet.)
struct Bins {
    std::optional<long long> count;
    std::vector<double> edges;
};

// np.histogram_bin_edges(a, bins, range, weights) and np.histogram(a, bins, range, density,
// weights) of float64 data, with NumPy's arithmetic; false and NumPy's message when it raises.
bool histogramBinEdges(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
                       const std::vector<double>* weights, std::vector<double>* edges, std::string* error);
bool histogram(const std::vector<double>& a, const Bins& bins, std::optional<std::pair<double, double>> range,
               bool density, const std::vector<double>* weights, std::vector<double>* counts,
               std::vector<double>* edges, std::string* error);

}  // namespace calculate_plot::support
