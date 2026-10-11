#include "support/pyparse.hpp"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace calculate_plot::support {

namespace {

bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }

// The text without the blanks around it; nullopt if a character is not ASCII.
std::optional<std::string> stripped(const std::string& text) {
    for (unsigned char c : text)
        if (c >= 0x80) return std::nullopt;
    std::size_t a = 0, b = text.size();
    while (a < b && isBlank(text[a])) ++a;
    while (b > a && isBlank(text[b - 1])) --b;
    return text.substr(a, b - a);
}

// digitpart: digit (["_"] digit)*, appended to `out` without its underscores; false if there is none.
bool digitPart(const std::string& s, std::size_t* i, std::string* out) {
    if (*i >= s.size() || !isDigit(s[*i])) return false;
    for (;;) {
        *out += s[(*i)++];
        if (*i < s.size() && isDigit(s[*i])) continue;
        if (*i + 1 < s.size() && s[*i] == '_' && isDigit(s[*i + 1])) {
            ++*i;
            continue;
        }
        return true;
    }
}

}  // namespace

std::optional<double> pythonFloat(const std::string& text) {
    const auto body = stripped(text);
    if (!body) return std::nullopt;
    const std::string& s = *body;
    std::size_t i = 0;
    std::string number;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) number += s[i++];
    std::string word;
    for (std::size_t k = i; k < s.size(); ++k) word += lower(s[k]);
    if (word == "inf" || word == "infinity" || word == "nan") {
        const double value = word == "nan" ? std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::infinity();
        return number == "-" ? -value : value;
    }
    const bool whole = digitPart(s, &i, &number);
    bool fraction = false;
    if (i < s.size() && s[i] == '.') {
        number += s[i++];
        fraction = digitPart(s, &i, &number);
    }
    if (!whole && !fraction) return std::nullopt;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        number += s[i++];
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) number += s[i++];
        if (!digitPart(s, &i, &number)) return std::nullopt;
    }
    if (i != s.size()) return std::nullopt;
    return std::strtod(number.c_str(), nullptr);
}

std::optional<long long> pythonInt(const std::string& text) {
    const auto body = stripped(text);
    if (!body) return std::nullopt;
    const std::string& s = *body;
    std::size_t i = 0;
    std::string number;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) number += s[i++];
    if (!digitPart(s, &i, &number) || i != s.size()) return std::nullopt;
    return std::strtoll(number.c_str(), nullptr, 10);  // (the ends of the range for what does not fit)
}

}  // namespace calculate_plot::support
