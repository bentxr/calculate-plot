#pragma once

#include <string>

namespace calculate_plot::support {

// repr(x) of a Python float: the shortest digits that round-trip, in positional form for
// exponents from -4 to 15 and scientific otherwise ("1e-05", "1e+16", "100000.0").
std::string pythonRepr(double x);

// repr(np.float64(x)) in NumPy 2: "np.float64(...)".
std::string numpyRepr(double x);

// f"{x:.{precision}f}": correctly rounded fixed notation, always with '.', whatever the locale.
std::string fixed(double x, int precision);

// Reads a decimal number with '.' as the point, whatever the locale (strtod rules); false unless
// the whole text is a number.
bool readDouble(const std::string& text, double& value);

// matplotlib's _short_float_fmt: f"{x:f}" without trailing zeros or decimal point.
std::string shortFloat(double x);

}  // namespace calculate_plot::support
