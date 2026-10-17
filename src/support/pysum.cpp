// Python's sum() of ints and floats: specified behaviour (Python 3.12+ sums floats with Neumaier's
// compensated algorithm, Numerische Mathematik 1974), pinned down by probing the oracle's Python;
// written for calculate-plot (no code taken from CPython).
#include "support/pynum.hpp"

#include <cmath>

namespace calculate_plot::support {

double pythonSum(const std::vector<double>& values, const std::vector<bool>& integers) {
    std::size_t i = 0;
    long long exact = 0;  // the leading ints, summed as integers
    for (; i < values.size() && integers[i]; ++i) exact += static_cast<long long>(values[i]);
    if (i == values.size()) return double(exact);
    double total = double(exact) + values[i++];  // int + float: a plain float addition
    double compensation = 0.0;
    for (; i < values.size(); ++i) {
        const double x = values[i];
        const double t = total + x;
        // What the addition lost, from whichever operand is larger in magnitude.
        compensation += std::fabs(total) >= std::fabs(x) ? (total - t) + x : (x - t) + total;
        total = t;
    }
    if (compensation != 0.0 && std::isfinite(compensation)) total += compensation;
    return total;
}

}  // namespace calculate_plot::support
