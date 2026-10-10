#include "support/pyformat.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>

namespace calculate_plot::support {

std::string pythonRepr(double x) {
    if (std::isnan(x)) return "nan";
    if (std::isinf(x)) return x > 0 ? "inf" : "-inf";
    // The shortest digits that round-trip, from the scientific form "d.ddde±x".
    char buffer[64];
    const auto end = std::to_chars(buffer, buffer + sizeof buffer, x, std::chars_format::scientific).ptr;
    std::string text(buffer, end);
    std::string sign;
    if (text[0] == '-') {
        sign = "-";
        text.erase(0, 1);
    }
    const auto e = text.find('e');
    std::string digits = text.substr(0, e);
    digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
    const int exponent = std::atoi(text.c_str() + e + 1);
    const int decimalPoint = exponent + 1;  // digits before the point
    const int count = int(digits.size());
    // Python's 'r' format: positional for -4 < decimalPoint <= 16, scientific otherwise.
    if (decimalPoint > -4 && decimalPoint <= 16) {
        if (decimalPoint <= 0) return sign + "0." + std::string(std::size_t(-decimalPoint), '0') + digits;
        if (decimalPoint >= count) return sign + digits + std::string(std::size_t(decimalPoint - count), '0') + ".0";
        return sign + digits.substr(0, std::size_t(decimalPoint)) + "." + digits.substr(std::size_t(decimalPoint));
    }
    std::string mantissa = digits.substr(0, 1);
    if (count > 1) mantissa += "." + digits.substr(1);
    const int shown = std::abs(exponent);
    return sign + mantissa + "e" + (exponent < 0 ? "-" : "+") + (shown < 10 ? "0" : "") + std::to_string(shown);
}

std::string numpyRepr(double x) { return "np.float64(" + pythonRepr(x) + ")"; }

std::string fixed(double x, int precision) {
    std::string buffer(400 + std::size_t(precision), '\0');
    const auto end = std::to_chars(buffer.data(), buffer.data() + buffer.size(), x, std::chars_format::fixed, precision).ptr;
    buffer.resize(std::size_t(end - buffer.data()));
    return buffer;
}

std::string shortFloat(double x) {
    std::string text = fixed(x, 6);
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

}  // namespace calculate_plot::support
