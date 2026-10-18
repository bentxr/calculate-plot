#include "support/pyformat.hpp"

#include <algorithm>
#include <charconv>
#if defined(__GLIBC__)
#include <locale.h>
#endif
#include <cmath>
#include <cstdio>
#include <clocale>
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

namespace {

Formatted raised(const std::string& what) { return {Formatted::Raised, what}; }
Formatted unsupported(const std::string& what) { return {Formatted::Unsupported, what}; }

// str() and repr() of the value as NumPy 2's scalars give them
std::string strOf(double v, bool isInt) {
    if (isInt) return std::to_string(static_cast<long long>(v));
    return pythonRepr(v);
}
std::string reprOf(double v, bool isInt) {
    if (isInt) return "np.int64(" + std::to_string(static_cast<long long>(v)) + ")";
    return numpyRepr(v);
}

// printf of one double as Python's % does it: whatever the process locale, the point is '.'
std::string cFormat(const std::string& spec, double v) {
    char buffer[512];
    const int n = std::snprintf(buffer, sizeof buffer, spec.c_str(), v);
    std::string out = n < 0 ? std::string() : std::string(buffer, std::size_t(std::min(n, int(sizeof buffer) - 1)));
    const std::string point = std::localeconv()->decimal_point;
    if (point != ".")
        if (const auto at = out.find(point); at != std::string::npos) out.replace(at, point.size(), ".");
    return out;
}
std::string cFormatInt(const std::string& spec, long long v) {
    char buffer[512];
    const int n = std::snprintf(buffer, sizeof buffer, spec.c_str(), v);
    return n < 0 ? std::string() : std::string(buffer, std::size_t(std::min(n, int(sizeof buffer) - 1)));
}

// one code point as UTF-8
std::string utf8(long long c) {
    std::string out;
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)), out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
    return out;
}

// Python's int(x) of a float, within 64 bits
bool toInteger(double v, long long* out, Formatted* failure) {
    if (std::isnan(v)) return *failure = raised("ValueError: cannot convert float NaN to integer"), false;
    if (std::isinf(v)) return *failure = raised("OverflowError: cannot convert float infinity to integer"), false;
    const double t = std::trunc(v);
    if (!(std::fabs(t) < 9.2e18)) return *failure = unsupported("integers beyond 64 bits"), false;
    *out = static_cast<long long>(t);
    return true;
}

}  // namespace

Formatted percentFormat(const std::string& fmt, double v, bool isInt, bool numpyScalar) {
    std::string out;
    int used = 0;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%') {
            out += fmt[i];
            continue;
        }
        if (++i >= fmt.size()) return raised("ValueError: incomplete format");
        if (fmt[i] == '(') return raised("TypeError: format requires a mapping");
        std::string flags;
        while (i < fmt.size() && std::string("#0- +").find(fmt[i]) != std::string::npos) flags += fmt[i++];
        std::string width, precision;
        bool hasPrecision = false;
        if (i < fmt.size() && fmt[i] == '*') return unsupported("'*' widths");
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) width += fmt[i++];
        if (i < fmt.size() && fmt[i] == '.') {
            hasPrecision = true;
            ++i;
            if (i < fmt.size() && fmt[i] == '*') return unsupported("'*' precisions");
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) precision += fmt[i++];
        }
        while (i < fmt.size() && (fmt[i] == 'h' || fmt[i] == 'l' || fmt[i] == 'L')) ++i;
        if (i >= fmt.size()) return raised("ValueError: incomplete format");
        const char conv = fmt[i];
        if (conv == '%') {
            out += '%';
            continue;
        }
        if (used++) return raised("TypeError: not enough arguments for format string");
        const std::string spec = "%" + flags + width + (hasPrecision ? "." + (precision.empty() ? std::string("0") : precision) : "");
        const auto pad = [&](std::string s) {
            if (hasPrecision) s = s.substr(0, std::size_t(precision.empty() ? 0 : std::stoi(precision)));
            const std::size_t w = width.empty() ? 0 : std::size_t(std::stoul(width));
            if (s.size() < w) s = flags.find('-') != std::string::npos ? s + std::string(w - s.size(), ' ') : std::string(w - s.size(), ' ') + s;
            return s;
        };
        switch (conv) {
        case 'd': case 'i': case 'u': {
            long long n = 0;
            Formatted failure;
            if (isInt) n = static_cast<long long>(v);
            else if (!toInteger(v, &n, &failure)) return failure;
            out += cFormatInt(spec + "lld", n);
            break;
        }
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
            out += cFormat(spec + conv, v);
            break;
        case 's': out += pad(strOf(v, isInt)); break;
        case 'r': out += pad(reprOf(v, isInt)); break;
        case 'x': case 'X': case 'o': case 'c':
            if (!isInt) {
                if (conv == 'c') return raised("TypeError: %c requires an int or a unicode character, not numpy.float64");
                return raised(std::string("TypeError: %") + conv + " format: an integer is required, not numpy.float64");
            }
            if (!flags.empty() || !width.empty() || hasPrecision) return unsupported("flags with %x, %o or %c");
            if (conv == 'c') {
                if (v < 0 || v >= 0x110000) return raised("OverflowError: %c arg not in range(0x110000)");
                out += utf8(static_cast<long long>(v));
            } else {
                const long long n = static_cast<long long>(v);
                out += (n < 0 ? "-" : "") + cFormatInt(std::string("%ll") + (conv == 'X' ? 'X' : conv == 'x' ? 'x' : 'o'), n < 0 ? -n : n);
            }
            break;
        default:
            return raised(std::string("ValueError: unsupported format character"));
        }
    }
    // (a NumPy scalar can be indexed, so % takes it as a mapping: nothing is left unconverted)
    if (!used && !numpyScalar) return raised("TypeError: not all arguments converted during string formatting");
    return {Formatted::Text, out};
}

namespace {

// format(value, spec) of a float or an int (the part after ':' of a replacement field)
Formatted formatNumber(const std::string& spec, double v, bool isInt) {
    std::size_t i = 0;
    char fill = ' ', align = 0;
    if (spec.size() >= 2 && std::string("<>=^").find(spec[1]) != std::string::npos) fill = spec[0], align = spec[1], i = 2;
    else if (!spec.empty() && std::string("<>=^").find(spec[0]) != std::string::npos) align = spec[0], i = 1;
    char sign = '-';
    if (i < spec.size() && std::string("+- ").find(spec[i]) != std::string::npos) sign = spec[i++];
    if (i < spec.size() && spec[i] == 'z') return unsupported("the 'z' option");
    bool alternate = false;
    if (i < spec.size() && spec[i] == '#') alternate = true, ++i;
    if (i < spec.size() && spec[i] == '0') {
        if (!align) fill = '0', align = '=';
        ++i;
    }
    std::string width, precision;
    bool hasPrecision = false;
    while (i < spec.size() && std::isdigit(static_cast<unsigned char>(spec[i]))) width += spec[i++];
    if (i < spec.size() && (spec[i] == ',' || spec[i] == '_')) return unsupported("grouping");
    if (i < spec.size() && spec[i] == '.') {
        hasPrecision = true;
        ++i;
        while (i < spec.size() && std::isdigit(static_cast<unsigned char>(spec[i]))) precision += spec[i++];
        if (precision.empty()) return raised("ValueError: Format specifier missing precision");
    }
    std::string type = spec.substr(i);
    if (type.size() > 1) return raised("ValueError: Invalid format specifier '" + spec + "' for object of type '" + (isInt ? "int" : "float") + "'");
    char t = type.empty() ? 0 : type[0];
    if (t == 'n' && !isInt) t = 'g';  // ('n' is 'g' with the locale's separators: none in Python's C numeric locale)
    const int p = hasPrecision ? std::stoi(precision) : 6;
    const double a = std::fabs(v);
    std::string body;
    if (isInt && (t == 0 || t == 'd')) {
        if (hasPrecision) return raised("ValueError: Precision not allowed in integer format specifier");
        body = cFormatInt("%lld", static_cast<long long>(a));
    } else if (isInt && (t == 'x' || t == 'X' || t == 'o' || t == 'b' || t == 'c' || t == 'n')) {
        return unsupported("integer presentation types");
    } else if (t == 0) {
        if (alternate) return unsupported("'#' without a type");
        if (!hasPrecision) {
            body = isInt ? cFormatInt("%lld", static_cast<long long>(a)) : pythonRepr(a);
        } else {
            body = cFormat("%." + std::to_string(p == 0 ? 1 : p) + "g", a);
            if (std::isfinite(a) && body.find_first_of(".e") == std::string::npos) body += ".0";
        }
    } else if (std::string("eEfFgG%").find(t) != std::string::npos) {
        const std::string c = std::string("%") + (alternate ? "#" : "") + "." + std::to_string(p) + (t == '%' ? 'f' : t);
        body = cFormat(c, t == '%' ? a * 100 : a);
        if (t == '%') body += '%';
    } else {
        return raised(std::string("ValueError: Unknown format code '") + t + "' for object of type '" + (isInt ? "int" : "float") + "'");
    }
    std::string signText;
    if (std::signbit(v) && !std::isnan(v)) signText = "-";
    else if (sign == '+') signText = "+";
    else if (sign == ' ') signText = " ";
    const std::size_t w = width.empty() ? 0 : std::size_t(std::stoul(width));
    const std::size_t len = signText.size() + body.size();
    if (len >= w) return {Formatted::Text, signText + body};
    const std::size_t padding = w - len;
    switch (align ? align : '>') {
    case '<': return {Formatted::Text, signText + body + std::string(padding, fill)};
    case '^': return {Formatted::Text, std::string(padding / 2, fill) + signText + body + std::string(padding - padding / 2, fill)};
    case '=': return {Formatted::Text, signText + std::string(padding, fill) + body};
    default: return {Formatted::Text, std::string(padding, fill) + signText + body};
    }
}

// format(text, spec) of a string (after !r or !s): fill, align, width and precision
Formatted formatString(const std::string& spec, std::string s) {
    std::size_t i = 0;
    char fill = ' ', align = '<';
    if (spec.size() >= 2 && std::string("<>^").find(spec[1]) != std::string::npos) fill = spec[0], align = spec[1], i = 2;
    else if (!spec.empty() && std::string("<>^").find(spec[0]) != std::string::npos) align = spec[0], i = 1;
    std::string width, precision;
    while (i < spec.size() && std::isdigit(static_cast<unsigned char>(spec[i]))) width += spec[i++];
    if (i < spec.size() && spec[i] == '.') {
        ++i;
        while (i < spec.size() && std::isdigit(static_cast<unsigned char>(spec[i]))) precision += spec[i++];
        s = s.substr(0, std::size_t(precision.empty() ? 0 : std::stoi(precision)));
    }
    if (i < spec.size() && spec.substr(i) != "s") return unsupported("string format specifiers");
    const std::size_t w = width.empty() ? 0 : std::size_t(std::stoul(width));
    if (s.size() >= w) return {Formatted::Text, s};
    const std::size_t padding = w - s.size();
    if (align == '>') return {Formatted::Text, std::string(padding, fill) + s};
    if (align == '^') return {Formatted::Text, std::string(padding / 2, fill) + s + std::string(padding - padding / 2, fill)};
    return {Formatted::Text, s + std::string(padding, fill)};
}

}  // namespace

namespace {

// str.format (one positional argument) or string.Formatter (keyword arguments only): the replacement fields, each
// through format_field (`fixMinus` replaces '-' with U+2212 in what a field gives)
Formatted braceCore(const std::string& fmt, const std::vector<FormatArgument>& positional, const std::vector<FormatArgument>& kwargs,
                    bool stringFormatter, bool fixMinus) {
    std::string out;
    int automatic = -1;  // -1: not decided; 1: automatic numbering, 0: manual
    int next = 0;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        const char c = fmt[i];
        if (c == '}') {
            if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
                out += '}';
                ++i;
                continue;
            }
            return raised("ValueError: Single '}' encountered in format string");
        }
        if (c != '{') {
            out += c;
            continue;
        }
        if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
            out += '{';
            ++i;
            continue;
        }
        const std::size_t close = fmt.find('}', i + 1);
        if (close == std::string::npos) {
            if (i + 1 == fmt.size()) return raised("ValueError: Single '{' encountered in format string");
            return raised("ValueError: expected '}' before end of string");
        }
        const std::string field = fmt.substr(i + 1, close - i - 1);
        if (field.find('{') != std::string::npos) return unsupported("nested replacement fields");
        i = close;
        std::size_t nameEnd = field.find_first_of("!:");
        const std::string name = field.substr(0, nameEnd);
        std::string conversion, spec;
        if (nameEnd != std::string::npos && field[nameEnd] == '!') {
            const std::size_t colon = field.find(':', nameEnd);
            conversion = field.substr(nameEnd + 1, colon == std::string::npos ? std::string::npos : colon - nameEnd - 1);
            if (colon != std::string::npos) spec = field.substr(colon + 1);
        } else if (nameEnd != std::string::npos) {
            spec = field.substr(nameEnd + 1);
        }
        const FormatArgument* arg = nullptr;
        if (name.empty() || std::all_of(name.begin(), name.end(), [](char d) { return std::isdigit(static_cast<unsigned char>(d)); })) {
            int index = 0;
            if (name.empty()) {
                if (automatic == 0) return raised("ValueError: cannot switch from manual field specification to automatic field numbering");
                automatic = 1;
                index = next++;
            } else {
                if (automatic == 1) return raised("ValueError: cannot switch from automatic field numbering to manual field specification");
                automatic = 0;
                index = std::stoi(name);
            }
            if (index >= int(positional.size()))
                return raised(stringFormatter ? std::string("IndexError: tuple index out of range")
                                              : "IndexError: Replacement index " + std::to_string(index) + " out of range for positional args tuple");
            arg = &positional[std::size_t(index)];
        } else {
            for (const auto& k : kwargs)
                if (k.name == name) arg = &k;
            if (!arg) return raised("KeyError: '" + name + "'");
        }
        Formatted piece;
        if (conversion == "r" || conversion == "s")
            piece = formatString(spec, conversion == "r" ? reprOf(arg->value, arg->isInt) : strOf(arg->value, arg->isInt));
        else if (!conversion.empty()) return unsupported("conversions other than !r and !s");
        else piece = formatNumber(spec, arg->value, arg->isInt);
        if (piece.kind != Formatted::Text) return piece;
        if (fixMinus) {
            std::string fixed;
            for (char ch : piece.text) fixed += ch == '-' ? std::string("\xe2\x88\x92") : std::string(1, ch);
            piece.text = fixed;
        }
        out += piece.text;
    }
    return {Formatted::Text, out};
}

}  // namespace

Formatted braceFormat(const std::string& fmt, double v, bool isInt) {
    return braceCore(fmt, {FormatArgument{"", v, isInt}}, {}, false, false);
}

Formatted keywordFormat(const std::string& fmt, const std::vector<FormatArgument>& kwargs, bool fixMinus) {
    return braceCore(fmt, {}, kwargs, true, fixMinus);
}

}  // namespace calculate_plot::support
