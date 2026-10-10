// Python's round(x, ndigits) and %-formatting of doubles: specified behaviour, written for
// calculate-plot (no code taken from CPython).
#include "support/pynum.hpp"
#include "support/pyformat.hpp"

#include <charconv>
#include <cmath>
#include <string>

namespace calculate_plot::support {

double pythonRound(double x, int ndigits) {
    if (!std::isfinite(x)) return x;
    // The exactly rounded decimal with ndigits after the point (ties to even on the exact binary
    // value), read back as the nearest double: what CPython's double_round computes.
    const std::string text = fixed(x, ndigits);
    double value = 0;
    readDouble(text, value);
    if (value == 0) return std::copysign(0.0, x);
    return value;
}

std::string percentF(double x, int precision) { return fixed(x, precision); }

std::string percentG(double x, int precision) {
    if (std::isnan(x)) return "nan";
    if (std::isinf(x)) return x > 0 ? "inf" : "-inf";
    // As printf("%.*g"): scientific when the exponent is < -4 or >= precision; trailing zeros removed.
    const int p = precision == 0 ? 1 : precision;
    char buffer[512];
    auto end = std::to_chars(buffer, buffer + sizeof buffer, x, std::chars_format::scientific, p - 1).ptr;
    std::string sci(buffer, end);
    const auto e = sci.find('e');
    const int exponent = std::atoi(sci.c_str() + e + 1);
    std::string out;
    if (exponent < -4 || exponent >= p) {
        std::string mantissa = sci.substr(0, e);
        if (mantissa.find('.') != std::string::npos) {
            while (mantissa.back() == '0') mantissa.pop_back();
            if (mantissa.back() == '.') mantissa.pop_back();
        }
        const int shown = std::abs(exponent);
        out = mantissa + "e" + (exponent < 0 ? "-" : "+") + (shown < 10 ? "0" : "") + std::to_string(shown);
    } else {
        out = fixed(x, p - 1 - exponent);
        if (out.find('.') != std::string::npos) {
            while (out.back() == '0') out.pop_back();
            if (out.back() == '.') out.pop_back();
        }
    }
    return out;
}

}  // namespace calculate_plot::support
