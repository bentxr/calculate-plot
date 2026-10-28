// Derived from NumPy 2.5.3: numpy/_core/arrayprint.py (array2string, FloatingFormat) and
// numpy/_core/src/multiarray/dragon4.c (its rounding of positional and scientific digits).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md

#include "support/numpy_str.hpp"

#include "support/pyformat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace calculate_plot::support {

namespace {

constexpr int kPrecision = 8;  // np.get_printoptions()["precision"]

// The shortest digits that round-trip (Dragon4's unique mode, as repr): |x| = d0.d1d2... * 10**exp
void shortestDigits(double x, std::string* digits, int* exp) {
    const std::string r = pythonRepr(std::fabs(x));
    const auto e = r.find('e');
    const std::string mantissa = r.substr(0, e);
    int exponent = e == std::string::npos ? 0 : std::atoi(r.c_str() + e + 1);
    const auto point = mantissa.find('.');
    std::string all = mantissa.substr(0, point) + (point == std::string::npos ? "" : mantissa.substr(point + 1));
    exponent += int(point == std::string::npos ? mantissa.size() : point) - 1;
    // leading zeros (0.001) move the exponent; trailing zeros (100.0) are not digits
    std::size_t lead = 0;
    while (lead + 1 < all.size() && all[lead] == '0') ++lead, --exponent;
    all.erase(0, lead);
    while (all.size() > 1 && all.back() == '0') all.pop_back();
    if (all == "0") exponent = 0;
    *digits = all;
    *exp = exponent;
}

// A number split as NumPy prints it: sign and integer part, fraction digits, exponent ("+05"; empty positionally)
struct Parts {
    std::string integer, fraction, exponent;
};

// dragon4_positional(x, precision=8, unique=True, fractional=True, trim='.'): the shortest digits, or the value
// correctly rounded to 8 decimals when those need more; trailing zeros trimmed
Parts positional(double x) {
    std::string d;
    int e = 0;
    shortestDigits(x, &d, &e);
    Parts p;
    const int fractionDigits = std::max(0, int(d.size()) - 1 - e);
    if (fractionDigits > kPrecision) {
        const std::string f = fixed(std::fabs(x), kPrecision);
        const auto point = f.find('.');
        p.integer = f.substr(0, point);
        p.fraction = f.substr(point + 1);
        while (!p.fraction.empty() && p.fraction.back() == '0') p.fraction.pop_back();
    } else if (e >= 0) {
        p.integer = d.substr(0, std::min(d.size(), std::size_t(e) + 1));
        p.integer.append(std::size_t(e) + 1 - p.integer.size(), '0');
        p.fraction = d.size() > std::size_t(e) + 1 ? d.substr(std::size_t(e) + 1) : "";
    } else {
        p.integer = "0";
        p.fraction = std::string(std::size_t(-e - 1), '0') + d;
    }
    if (std::signbit(x)) p.integer = "-" + p.integer;
    return p;
}

// dragon4_scientific(x, precision=8, unique=True, trim='.'): the shortest digits, or the value correctly rounded to
// 9 significant digits when those need more; the exponent with a sign and at least two digits
Parts scientific(double x) {
    std::string d;
    int e = 0;
    shortestDigits(x, &d, &e);
    if (int(d.size()) - 1 > kPrecision) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.*e", kPrecision, std::fabs(x));
        d.clear();
        const char* c = buf;
        for (; *c && *c != 'e'; ++c)
            if (*c >= '0' && *c <= '9') d += *c;
        e = std::atoi(c + 1);
        while (d.size() > 1 && d.back() == '0') d.pop_back();
    }
    Parts p;
    p.integer = (std::signbit(x) ? "-" : "") + d.substr(0, 1);
    p.fraction = d.substr(1);
    std::string digits = std::to_string(std::abs(e));
    if (digits.size() < 2) digits = "0" + digits;
    p.exponent = (e < 0 ? "-" : "+") + digits;
    return p;
}

}  // namespace

std::string numpyStr(const std::vector<std::vector<double>>& rows) {
    // FloatingFormat.fillFormat: scientific when the non-zero magnitudes reach 1e8, go below 1e-4, or span more
    // than three decades
    double maxVal = 0, minVal = 0;
    bool any = false;
    for (const auto& r : rows)
        for (double v : r) {
            if (v == 0) continue;
            const double a = std::fabs(v);
            maxVal = any ? std::max(maxVal, a) : a;
            minVal = any ? std::min(minVal, a) : a;
            any = true;
        }
    const bool exp = any && (maxVal >= 1.e8 || minVal < 0.0001 || maxVal / minVal > 1000.);
    std::vector<std::vector<Parts>> parts;
    std::size_t padLeft = 0, padRight = 0, expSize = 0;
    for (const auto& r : rows) {
        parts.emplace_back();
        for (double v : r) {
            parts.back().push_back(exp ? scientific(v) : positional(v));
            const Parts& p = parts.back().back();
            padLeft = std::max(padLeft, p.integer.size());
            padRight = std::max(padRight, p.fraction.size());
            expSize = std::max(expSize, p.exponent.size());
        }
    }
    // FloatingFormat.__call__: the integer parts padded to one width on the left; positionally the fractions padded
    // with spaces on the right, in scientific notation with zeros to one precision and the exponents to one size
    const auto format = [&](const Parts& p) {
        std::string s = std::string(padLeft - p.integer.size(), ' ') + p.integer + ".";
        if (!exp) return s + p.fraction + std::string(padRight - p.fraction.size(), ' ');
        std::string digits = p.exponent.substr(1);
        digits.insert(0, expSize - 1 - digits.size(), '0');
        return s + p.fraction + std::string(padRight - p.fraction.size(), '0') + "e" + p.exponent[0] + digits;
    };
    // array2string: rows in brackets, elements separated by a space, rows by a new line indented by one
    std::string out = "[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += "\n ";
        out += "[";
        for (std::size_t j = 0; j < parts[i].size(); ++j) out += (j ? " " : "") + format(parts[i][j]);
        out += "]";
    }
    return out + "]";
}

}  // namespace calculate_plot::support
