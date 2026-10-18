#pragma once

#include <string>
#include <vector>

namespace calculate_plot::support {

// repr(x) of a Python float: the shortest digits that round-trip, in positional form for
// exponents from -4 to 15 and scientific otherwise ("1e-05", "1e+16", "100000.0").
std::string pythonRepr(double x);

// repr(s) of a Python str: single quotes unless the text has some and no double; backslashes, the
// quote, \n, \r, \t and other control characters escaped; other UTF-8 bytes as they are.
std::string pythonStrRepr(const std::string& s);

// repr(np.float64(x)) in NumPy 2: "np.float64(...)".
std::string numpyRepr(double x);

// f"{x:.{precision}f}": correctly rounded fixed notation, always with '.', whatever the locale.
std::string fixed(double x, int precision);

// Reads a decimal number with '.' as the point, whatever the locale (strtod rules); false unless
// the whole text is a number.
bool readDouble(const std::string& text, double& value);

// matplotlib's _short_float_fmt: f"{x:f}" without trailing zeros or decimal point.
std::string shortFloat(double x);

// What Python makes of a format string and one number: a float (np.float64), or an int (np.int64)
// when `isInt`. `text` is the result, or for Raised the exception as "TypeError: ..."; Unsupported:
// a form not ported (mapping keys, '*', grouping, ...).
struct Formatted {
    enum Kind { Text, Raised, Unsupported } kind = Text;
    std::string text;
};
// fmt % (value,); `numpyScalar`: fmt % value of a NumPy scalar, which % takes as a mapping (a format without any
// conversion gives itself back)
Formatted percentFormat(const std::string& fmt, double value, bool isInt, bool numpyScalar = false);
// fmt.format(value)
Formatted braceFormat(const std::string& fmt, double value, bool isInt);
// A number given by name to a format: its value, and whether it is an int.
struct FormatArgument {
    std::string name;
    double value = 0;
    bool isInt = false;
};
// string.Formatter().format(fmt, **kwargs) (no positional arguments); `fixMinus`: each formatted field's '-' made a
// U+2212 minus (matplotlib's _UnicodeMinusFormat)
Formatted keywordFormat(const std::string& fmt, const std::vector<FormatArgument>& kwargs, bool fixMinus);

}  // namespace calculate_plot::support
