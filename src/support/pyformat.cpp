#include "support/pyformat.hpp"

#include <algorithm>
#include <charconv>
#if defined(__GLIBC__)
#include <locale.h>
#endif
#include <cmath>
#include <cstdio>
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

bool readDouble(const std::string& text, double& value) {
    // The decimal point is always '.', whatever locale the program set (Qt applications set the
    // user's). glibc honours LC_NUMERIC, so it reads through a fixed "C" locale; other C libraries
    // (musl, used by Emscripten) always read '.'.
    char* end = nullptr;
#if defined(__GLIBC__)
    static const locale_t c = newlocale(LC_ALL_MASK, "C", locale_t(nullptr));
    value = strtod_l(text.c_str(), &end, c);
#else
    value = std::strtod(text.c_str(), &end);
#endif
    return !text.empty() && end == text.c_str() + text.size();
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

std::string pythonStrRepr(const std::string& s) {
    const bool single = s.find('\'') != std::string::npos && s.find('"') == std::string::npos;
    const char q = single ? '"' : '\'';
    std::string out(1, q);
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == q) out += std::string("\\") + char(q);
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20 || c == 0x7f) {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\x%02x", unsigned(c));
            out += buffer;
        } else {
            out.push_back(char(c));  // UTF-8 bytes of printable characters pass through
        }
    }
    return out + char(q);
}

}  // namespace calculate_plot::support
