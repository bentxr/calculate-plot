#pragma once

#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::support {

// Python's and NumPy's floor division of doubles (x // y) and divmod(x, y), with their exact
// rounding and signs of zero (one algorithm, NumPy's npy_divmod).
double floorDivide(double a, double b);
std::pair<double, double> divmod(double a, double b);

// numpy.round(x, decimals): x * 10**decimals, rounded half to even, divided back.
double npRound(double x, int decimals);
// Python's round(x, ndigits): the double nearest to x correctly rounded to ndigits decimals.
double pythonRound(double x, int ndigits);

// Python's sum() of numbers (CPython 3.12+): `integers` says which values are Python ints. Leading
// ints are summed exactly, the first float is added plainly, then the rest with Neumaier's
// compensated summation.
double pythonSum(const std::vector<double>& values, const std::vector<bool>& integers);

// printf-style formatting as Python's % operator does it, whatever the C locale says.
std::string percentF(double x, int precision);  // "%.{p}f"
std::string percentG(double x, int precision);  // "%.{p}g"

}  // namespace calculate_plot::support
