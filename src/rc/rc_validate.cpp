// Derived from Matplotlib 3.11.2: lib/matplotlib/rcsetup.py (the validators of the rcParams) and
// lib/matplotlib/colors.py (is_color_like).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "rc/rc_validate.hpp"

#include "colors/colors.hpp"
#include "rc/cycler_text.hpp"

#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "colors/colors.hpp"
#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/fontconfig_pattern.hpp"
#include "support/pyliteral.hpp"
#include "support/pyparse.hpp"

namespace calculate_plot::rc {

namespace {

using support::Json;
using State = Validated::State;

Validated taken(Json value) { return {State::Taken, std::move(value), std::string()}; }
Validated refused(std::string why) { return {State::Refused, Json(), std::move(why)}; }
Validated unsupported(const std::string& what) { return {State::Unsupported, Json(), what + " is not supported yet"}; }

// --- The values of a figure file as Python has them -------------------------------------------

bool isObjectOf(const Json& v, const char* name) {
    return v.isObject() && v.members().size() == 1 && v.members()[0].first == name;
}
bool isTuple(const Json& v) { return isObjectOf(v, "$tuple"); }
bool isSpecialFloat(const Json& v) { return isObjectOf(v, "$float"); }  // nan, inf, -inf
bool isSequence(const Json& v) { return v.isArray() || isTuple(v); }    // a list or a tuple
const std::vector<Json>& items(const Json& v) { return v.isArray() ? v.items() : v.members()[0].second.items(); }
bool isInt(const Json& v) { return v.isNumber() && v.isInteger(); }       // (a bool is told apart)
bool isReal(const Json& v) { return v.isBool() || v.isNumber() || isSpecialFloat(v); }  // numbers.Real

double realValue(const Json& v) {
    if (v.isBool()) return v.asBool() ? 1.0 : 0.0;
    if (v.isNumber()) return v.asNumber();
    const std::string& name = v.members()[0].second.asString();
    if (name == "nan") return std::numeric_limits<double>::quiet_NaN();
    return name == "inf" ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
}

// A value as it is stored: tuples are lists, and floats that are not finite are numbers.
Json plain(const Json& v) {
    if (isSpecialFloat(v)) return Json::real(realValue(v));
    if (isSequence(v)) {
        std::vector<Json> out;
        for (const Json& item : items(v)) out.push_back(plain(item));
        return Json::array(std::move(out));
    }
    return v;
}

std::string asciiLower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

// str.lower() as far as it can give a word of ASCII: the capitals of ASCII, and the Kelvin sign
// (U+212A), the one other character whose small letter is in ASCII.
std::string pythonLower(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s.compare(i, 3, "\xe2\x84\xaa") == 0) {
            out += 'k';
            i += 2;
        } else {
            out += s[i] >= 'A' && s[i] <= 'Z' ? char(s[i] - 'A' + 'a') : s[i];
        }
    }
    return out;
}

// --- int() and float() -------------------------------------------------------------------------

struct Float {
    State state = State::Refused;
    double value = 0;
};

// float(s)
Float pythonFloatOf(const Json& s) {
    if (isReal(s)) return {State::Taken, realValue(s)};
    if (s.isString()) {
        // (Python also reads the digits and the blanks of other scripts)
        if (!support::isAscii(s.asString())) return {State::Unsupported, 0};
        if (const auto f = support::pythonFloat(s.asString())) return {State::Taken, *f};
    }
    return {};
}

struct Int {
    State state = State::Refused;
    long long value = 0;
};

// int(s). An integer that does not fit 64 bits is not known here.
Int pythonIntOf(const Json& s) {
    if (s.isBool()) return {State::Taken, s.asBool() ? 1 : 0};
    if (isInt(s)) {
        long long value = 0;
        const std::string& text = s.numberText();
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc() || result.ptr != text.data() + text.size()) return {State::Unsupported, 0};
        return {State::Taken, value};
    }
    if (s.isNumber()) {
        const double x = std::trunc(s.asNumber());
        if (!(std::fabs(x) < 9.2e18)) return {State::Unsupported, 0};
        return {State::Taken, static_cast<long long>(x)};
    }
    if (s.isString()) {
        if (!support::isAscii(s.asString())) return {State::Unsupported, 0};
        const auto i = support::pythonInt(s.asString());
        if (!i) return {};
        // (the ends of the range stand for what does not fit)
        if (*i == std::numeric_limits<long long>::max() || *i == std::numeric_limits<long long>::min()) return {State::Unsupported, 0};
        return {State::Taken, *i};
    }
    return {};  // None, a list (TypeError), nan (ValueError), inf (OverflowError)
}

// --- _make_type_validator ------------------------------------------------------------------------

bool isNoneWord(const Json& s) { return s.isString() && asciiLower(s.asString()) == "none"; }  // _str_lower_equal(s, "none")

Validated validateFloat(const Json& s) {
    const Float f = pythonFloatOf(s);
    if (f.state == State::Unsupported) return unsupported("a number in the digits of another script");
    if (f.state != State::Taken) return refused("could not convert the value to float");
    return taken(Json::real(f.value));
}

Validated validateInt(const Json& s) {
    const Int i = pythonIntOf(s);
    if (i.state == State::Unsupported) return unsupported("an integer of more than 64 bits, or in the digits of another script,");
    if (i.state != State::Taken) return refused("could not convert the value to int");
    return taken(Json::integer(i.value));
}

Validated validateString(const Json& s) {
    if (!s.isString()) return refused("could not convert the value to str");
    return taken(s);
}

using Validator = std::function<Validated(const Json&)>;

Validator orNone(Validator validator) {
    return [validator](const Json& s) { return s.isNull() || isNoneWord(s) ? taken(Json()) : validator(s); };
}

// --- _listify_validator --------------------------------------------------------------------------

Validated listify(const Json& s, const Validator& scalar, std::optional<std::size_t> n) {
    std::vector<Json> out;
    if (s.isString()) {
        // the pieces between commas, without their blanks; the empty ones go
        const std::string& text = s.asString();
        for (std::size_t a = 0; a <= text.size();) {
            const std::size_t comma = std::min(text.find(',', a), text.size());
            const std::string piece = support::pythonStrip(text.substr(a, comma - a));
            a = comma + 1;
            if (piece.empty()) continue;
            Validated v = scalar(Json::string(piece));
            if (v.state != State::Taken) return v;
            out.push_back(std::move(v.value));
        }
    } else if (isSequence(s)) {
        for (const Json& item : items(s)) {
            if (item.isString() && item.asString().empty()) continue;
            Validated v = scalar(item);
            if (v.state != State::Taken) return v;
            out.push_back(std::move(v.value));
        }
    } else {
        return refused("expected a str or a list");
    }
    if (n && out.size() != *n) return refused("expected " + std::to_string(*n) + " values, but there are " + std::to_string(out.size()));
    return taken(Json::array(std::move(out)));
}

// --- ValidateInStrings ---------------------------------------------------------------------------

Validated inStrings(const std::string& key, const std::vector<std::string>& valid, bool ignorecase, const Json& s) {
    std::string choices;
    for (const std::string& v : valid) choices += " " + v;
    const std::string why = "not a valid value for " + key + "; supported values are" + choices;
    if (!s.isString()) return refused("the value is " + why);
    const std::string wanted = ignorecase ? pythonLower(s.asString()) : s.asString();
    const std::string* found = nullptr;
    for (const std::string& v : valid)
        if ((ignorecase ? asciiLower(v) : v) == wanted) found = &v;  // (of two that differ in case only, the last)
    if (!found) return refused("'" + s.asString() + "' is " + why);
    return taken(Json::string(*found));
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> out;
    for (std::size_t a = 0; a <= text.size();) {
        const std::size_t b = std::min(text.find(separator, a), text.size());
        out.push_back(text.substr(a, b - a));
        a = b + 1;
    }
    return out;
}

// --- Colours -------------------------------------------------------------------------------------

// _is_nth_color: "C" and digits
bool isNthColor(const Json& c) {
    if (!c.isString() || c.asString().size() < 2 || c.asString()[0] != 'C') return false;
    for (std::size_t i = 1; i < c.asString().size(); ++i)
        if (c.asString()[i] < '0' || c.asString()[i] > '9') return false;
    return true;
}

// Whether an RGB(A) sequence is one: three or four real numbers from 0 to 1. With an alpha given
// apart, the fourth is not looked at.
bool isRgbSequence(const Json& c, bool alphaGiven) {
    if (!isSequence(c)) return false;
    const auto& v = items(c);
    if (v.size() != 3 && v.size() != 4) return false;
    for (const Json& x : v)
        if (!isReal(x)) return false;
    for (std::size_t i = 0; i < (alphaGiven ? 3 : v.size()); ++i) {
        const double x = realValue(v[i]);
        if (x < 0 || x > 1) return false;  // (a nan passes, as in matplotlib)
    }
    return true;
}

// to_rgba(c) without its tuple of colour and alpha: Taken where it is a colour.
State colorWithoutAlpha(const Json& c, bool alphaGiven) {
    if (isNthColor(c)) return State::Taken;
    if (c.isString()) {
        if (colors::toRgba(c.asString(), std::nullopt, Params::defaults(), nullptr)) return State::Taken;
        // (a grey level may be written in the digits of another script)
        return support::isAscii(c.asString()) ? State::Refused : State::Unsupported;
    }
    return isRgbSequence(c, alphaGiven) ? State::Taken : State::Refused;
}

// is_color_like(c)
State colorLike(const Json& c) {
    if (isTuple(c) && items(c).size() == 2) {
        // (colour, alpha)
        const Json& alpha = items(c)[1];
        if (alpha.isNull()) return colorWithoutAlpha(items(c)[0], false);
        if (!isReal(alpha)) return State::Refused;
        const double a = realValue(alpha);
        if (!(0 <= a && a <= 1)) return State::Refused;
        return colorWithoutAlpha(items(c)[0], true);
    }
    return colorWithoutAlpha(c, false);
}

Validated validateColor(const Json& s) {
    const char* const why = "the value does not look like a color arg";
    if (s.isString()) {
        const std::string& text = s.asString();
        if (asciiLower(text) == "none") return taken(Json::string("none"));
        if ((text.size() == 6 || text.size() == 8) && support::isAscii(text)) {
            const Json hex = Json::string("#" + text);
            if (colorLike(hex) == State::Taken) return taken(hex);
        }
    }
    const State direct = colorLike(s);
    if (direct == State::Taken) return taken(plain(s));
    if (direct == State::Unsupported) return unsupported("a colour written with characters outside ASCII");
    if (s.isString()) {
        // If it is still valid, it must be a tuple (as a string from matplotlibrc).
        const auto literal = support::pythonLiteral(s.asString());
        if (literal.state == support::PythonLiteral::State::Unsupported) return unsupported("this spelling of a Python literal");
        if (literal.state == support::PythonLiteral::State::Value) {
            const State inner = colorLike(literal.value);
            if (inner == State::Taken) return taken(plain(literal.value));
            if (inner == State::Unsupported) return unsupported("a colour written with characters outside ASCII");
        }
    }
    return refused(why);
}

// validate_color_or_inherit, validate_color_or_auto, _validate_color_or_edge
Validator colorOr(const char* word) {
    return [word](const Json& s) { return s.isString() && s.asString() == word ? taken(s) : validateColor(s); };
}

Validated validateColorOrNone(const Json& s) {
    if (s.isNull() || (s.isString() && s.asString() == "None")) return taken(Json());
    return validateColor(s);
}

Validated validateColorOrLinecolor(const Json& s) {
    const char* const why = "the value does not look like a color arg";
    if (s.isString()) {
        const std::string& text = s.asString();
        if (text == "linecolor") return taken(s);
        if (text == "mfc" || text == "markerfacecolor") return taken(Json::string("markerfacecolor"));
        if (text == "mec" || text == "markeredgecolor") return taken(Json::string("markeredgecolor"));
    }
    if (s.isNull()) return taken(Json());
    // isinstance(s, str) and len(s) == 6 or len(s) == 8: a string of six or eight characters is a
    // hexadecimal colour without its "#", or nothing; anything else of eight items breaks at '#' + s,
    // and what has no length breaks at len(s)
    if (s.isString()) {
        const std::string& text = s.asString();
        if (!support::isAscii(text)) return unsupported("a colour written with characters outside ASCII");
        if (text.size() == 6 || text.size() == 8) {
            const Json hex = Json::string("#" + text);
            return colorLike(hex) == State::Taken ? taken(hex) : refused(why);
        }
    } else if (!isSequence(s) || items(s).size() == 8) {
        return refused(why);
    }
    const State direct = colorLike(s);
    if (direct == State::Taken) return taken(plain(s));
    return refused(why);
}

// --- Line styles ---------------------------------------------------------------------------------

Validated validateLinestyle(const Json& given) {
    const char* const why = "the value is not a valid on-off ink sequence";
    Json ls = given;
    if (ls.isString()) {
        // Look first for a valid named line style, like '--' or 'solid'.
        static const std::vector<std::string> names = {"-", "--", "-.", ":", "solid", "dashed", "dashdot", "dotted",
                                                       "None", "none", " ", ""};
        Validated named = inStrings("linestyle", names, true, ls);
        if (named.state == State::Taken) return named;
        const auto literal = support::pythonLiteral(ls.asString());  // Parsing matplotlibrc.
        if (literal.state == support::PythonLiteral::State::Unsupported) return unsupported("this spelling of a Python literal");
        if (literal.state == support::PythonLiteral::State::Value) ls = literal.value;
    }
    if (isSequence(ls)) {
        const auto& v = items(ls);
        // (offset, (on, off, on, off, ...)), or for backcompat (on, off, on, off, ...) with no offset
        const bool withOffset = v.size() == 2 && isSequence(v[1]);
        const Json offset = withOffset ? v[0] : Json::integer(0);
        const Json& onoff = withOffset ? v[1] : ls;
        bool numbers = items(onoff).size() % 2 == 0 && isReal(offset);
        for (const Json& x : items(onoff)) numbers = numbers && isReal(x);
        if (numbers) return taken(Json::array({plain(offset), plain(onoff)}));
    }
    return refused(why);
}

// --- The rest of rcsetup.py's validators -----------------------------------------------------------

Validated validateBool(const Json& b) {
    if (b.isString()) {
        const std::string s = asciiLower(b.asString());
        for (const char* yes : {"t", "y", "yes", "on", "true", "1"})
            if (s == yes) return taken(Json::boolean(true));
        for (const char* no : {"f", "n", "no", "off", "false", "0"})
            if (s == no) return taken(Json::boolean(false));
    } else if (isReal(b)) {
        // (1 and 1.0 are True, 0 and 0.0 False)
        const double x = realValue(b);
        if (x == 1) return taken(Json::boolean(true));
        if (x == 0) return taken(Json::boolean(false));
    }
    return refused("cannot convert the value to bool");
}

Validated validateAxisbelow(const Json& s) {
    Validated b = validateBool(s);
    if (b.state == State::Taken) return b;
    if (s.isString() && s.asString() == "line") return taken(s);
    return refused("the value cannot be interpreted as True, False, or \"line\"");
}

Validated validateDpi(const Json& s) {
    if (s.isString() && s.asString() == "figure") return taken(s);
    Validated f = validateFloat(s);
    if (f.state == State::Refused) f.error = "the value is not the string \"figure\" and could not be converted to float";
    return f;
}

Validated validateMarker(const Json& s) {
    Validated i = validateInt(s);
    if (i.state != State::Refused) return i;
    if (isSpecialFloat(s)) return i;  // (int(inf) is an OverflowError, which nothing catches; nan is not a string either)
    Validated text = validateString(s);
    if (text.state == State::Taken) return text;
    return refused("supported markers are [string, int]");
}

Validated validateFonttype(const Json& s) {
    const Validated i = validateInt(s);
    if (i.state == State::Unsupported) return i;
    if (i.state == State::Taken) {
        if (i.value.asInteger() != 3 && i.value.asInteger() != 42) return refused("supported Postscript/PDF font types are 3, 42");
        return i;
    }
    if (s.isString()) {
        const std::string name = pythonLower(s.asString());
        if (name == "type3") return taken(Json::integer(3));
        if (name == "truetype") return taken(Json::integer(42));
    }
    return refused("supported Postscript/PDF font types are type3, truetype");
}

Validated validateToolbar(const Json& s) { return inStrings("toolbar", {"None", "toolbar2", "toolmanager"}, true, s); }

Validated validateAspect(const Json& s) {
    if (s.isString() && (s.asString() == "auto" || s.asString() == "equal")) return taken(s);
    Validated f = validateFloat(s);
    if (f.state == State::Refused) f.error = "not a valid aspect specification";
    return f;
}

Validated validateFontsize(const Json& given) {
    static const char* const sizes[] = {"xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large", "smaller", "larger"};
    if (given.isString()) {
        const std::string s = pythonLower(given.asString());
        for (const char* name : sizes)
            if (s == name) return taken(Json::string(s));
    }
    Validated f = validateFloat(given);
    if (f.state == State::Refused)
        f.error = "the value is not a valid font size. Valid font sizes are xx-small, x-small, small, medium, large, x-large, "
                  "xx-large, smaller, larger.";
    return f;
}

Validated validateFontsizeNone(const Json& s) {
    if (s.isNull() || (s.isString() && s.asString() == "None")) return taken(Json());
    return validateFontsize(s);
}

// validate_fontweight, validate_fontstretch: a name (in the case it is written in), or int(s)
Validated nameOrInt(const Json& s, const std::vector<std::string>& names, const char* what) {
    if (s.isString())
        for (const std::string& name : names)
            if (s.asString() == name) return taken(s);
    Validated i = validateInt(s);
    if (i.state == State::Refused) i.error = std::string("the value is not a valid font ") + what;
    return i;
}

Validated validateFontProperties(const Json& s) {
    if (!s.isString()) return refused("the value is not a fontconfig pattern");
    if (!support::parseFontconfigPattern(s.asString())) return refused("'" + s.asString() + "' is not a fontconfig pattern");
    return taken(s);
}

Validated validateMathtextFallback(const Json& given) {
    if (given.isNull()) return taken(Json());
    if (given.isString()) {
        const std::string s = pythonLower(given.asString());
        if (s == "none") return taken(Json());
        if (s == "cm" || s == "stix" || s == "stixsans") return taken(Json::string(s));
    }
    return refused((given.isString() ? given.asString() : std::string("the value")) +
                   " is not a valid fallback font name. Valid fallback font names are cm,stix,stixsans. Passing 'None' "
                   "will turn fallback off.");
}

Validated validateWhiskers(const Json& s) {
    Validated pair = listify(s, validateFloat, 2);
    if (pair.state != State::Refused) return pair;
    Validated one = validateFloat(s);
    if (one.state == State::Refused) one.error = "not a valid whisker value [float, (float, float)]";
    return one;
}

Validated validatePsDistiller(const Json& given) {
    // s in ('none', None, 'false', False): 0 and 0.0 are equal to False
    if (given.isNull() || (isReal(given) && realValue(given) == 0)) return taken(Json());
    if (given.isString()) {
        const std::string s = pythonLower(given.asString());
        if (s == "none" || s == "false") return taken(Json());
        return inStrings("ps.usedistiller", {"ghostscript", "xpdf"}, false, Json::string(s));
    }
    return inStrings("ps.usedistiller", {"ghostscript", "xpdf"}, false, given);
}

Validated validateBbox(const Json& given) {
    if (given.isNull()) return taken(Json());
    if (given.isString()) {
        const std::string s = pythonLower(given.asString());
        if (s == "tight") return taken(Json::string(s));
        if (s == "standard") return taken(Json());
    }
    return refused("bbox should be 'tight' or 'standard'");
}

Validated validateSketch(const Json& given) {
    Json s = given;
    if (s.isString()) {
        std::string text = support::pythonStrip(pythonLower(s.asString()));
        if (!support::isAscii(text)) return unsupported("a sketch written with characters outside ASCII");
        if (text.size() >= 2 && text.front() == '(' && text.back() == ')') text = text.substr(1, text.size() - 2);
        if (text == "none") return taken(Json());
        s = Json::string(text);
    }
    if (s.isNull()) return taken(Json());
    Validated triple = listify(s, validateFloat, 3);
    if (triple.state == State::Refused) triple.error = "expected a (scale, length, randomness) tuple";
    return triple;
}

// _validate_greaterthan_minushalf, _validate_greaterequal0_lessequal1
Validated floatWithin(const Json& s, bool aboveMinusHalf) {
    Validated f = validateFloat(s);
    if (f.state != State::Taken) return f;
    const double x = f.value.asNumber();
    if (aboveMinusHalf ? x > -0.5 : (0 <= x && x <= 1)) return f;
    return refused(aboveMinusHalf ? "the value must be >-0.5" : "the value must be >=0 and <=1");
}

Validated validateMinorTickNdivs(const Json& n) {
    if (n.isString() && asciiLower(n.asString()) == "auto") return taken(n);
    Validated i = validateInt(n);
    if (i.state == State::Unsupported) return i;
    if (i.state == State::Taken && i.value.asInteger() >= 0) return i;
    return refused("'tick.minor.ndivs' must be 'auto' or non-negative int");
}

Validated validateLegendLoc(const Json& given) {
    Json loc = given;
    if (loc.isString()) {
        static const std::vector<std::string> names = {"best",        "upper right",  "upper left",   "lower left",
                                                       "lower right", "right",        "center left",  "center right",
                                                       "lower center", "upper center", "center"};
        Validated named = inStrings("legend.loc", names, true, loc);
        if (named.state == State::Taken) return named;
        const auto literal = support::pythonLiteral(loc.asString());
        if (literal.state == support::PythonLiteral::State::Unsupported) return unsupported("this spelling of a Python literal");
        if (literal.state == support::PythonLiteral::State::Value) loc = literal.value;
    }
    // an int from 0 to 10 (a bool is an int), or a tuple of two numbers
    if (loc.isBool()) return taken(loc);
    if (isInt(loc)) {
        const Int i = pythonIntOf(loc);
        if (i.state == State::Taken && 0 <= i.value && i.value <= 10) return taken(loc);
    }
    if (isTuple(loc) && items(loc).size() == 2 && isReal(items(loc)[0]) && isReal(items(loc)[1])) return taken(plain(loc));
    return refused("the value is not a valid legend location");
}

Validated validateHistBins(const Json& s) {
    for (const char* name : {"auto", "sturges", "fd", "doane", "scott", "rice", "sqrt"})
        if (s.isString() && s.asString() == name) return taken(s);
    if (isSpecialFloat(s) && std::isinf(realValue(s))) return refused("an infinite number of bins");  // (OverflowError)
    const Validated i = validateInt(s);
    if (i.state != State::Refused) return i;
    Validated list = listify(s, validateFloat, std::nullopt);
    if (list.state == State::Refused) list.error = "'hist.bins' must be one of auto, sturges, fd, doane, scott, rice, sqrt, an int or a sequence of floats";
    return list;
}

// validate_color_for_prop_cycle: the Nth colour of the cycle cannot be in the cycle
Validated validateColorForCycle(const Json& s) {
    if (s.isString() && s.asString().size() == 2 && s.asString()[0] == 'C' && s.asString()[1] >= '0' && s.asString()[1] <= '9')
        return refused("Cannot put cycle reference ('" + s.asString() + "') in prop_cycler");
    return validateColor(s);
}

// _listify_validator(..., allow_stringlist=True): a text that is no list of colours between commas
// may be the name of a colour sequence, or one-letter colours side by side ("rgb").
Validated colorList(const Json& s, const Validator& scalar) {
    Validated list = listify(s, scalar, std::nullopt);
    if (!s.isString() || list.state != State::Refused) return list;
    const std::string& text = s.asString();
    // mpl.color_sequences[s]: its colours as they are, not validated
    if (const std::vector<Json>* sequence = colors::colorSequence(text)) return taken(Json::array(*sequence));
    std::vector<Json> out;
    for (const char c : text) {
        if (support::pythonStrip(std::string(1, c)).empty()) continue;
        Validated v = scalar(Json::string(std::string(1, c)));
        if (v.state == State::Unsupported) return v;
        if (v.state == State::Refused) return refused("'" + text + "' is neither a color sequence name nor can it be interpreted as a list of colors");
        out.push_back(std::move(v.value));
    }
    return taken(Json::array(std::move(out)));
}

// rcsetup._prop_validators: the values of one property of a cycle.
Validated cycleValues(const std::string& property, const Json& values) {
    if (property == "color") return colorList(values, validateColorForCycle);
    if (property == "facecolor" || property == "edgecolor" || property == "markerfacecolor" || property == "markeredgecolor")
        return colorList(values, validateColor);
    if (property == "linewidth" || property == "markersize" || property == "markeredgewidth" || property == "alpha")
        return listify(values, validateFloat, std::nullopt);
    if (property == "linestyle") return listify(values, validateLinestyle, std::nullopt);
    if (property == "marker") return listify(values, validateMarker, std::nullopt);
    // joinstyle, capstyle, fillstyle, markevery, hatch, dashes
    return unsupported("the " + property + " of a cycle");
}

Validated validateEnum(const std::string& key, const std::vector<std::string>& names, const Json& s) {
    // JoinStyle(s), CapStyle(s): the name as it is
    return inStrings(key, names, false, s);
}

// The validator of a name of data/rcdefaults.json's "validators".
Validated apply(const std::string& key, const std::string& name, const Json& s) {
    if (name.compare(0, 3, "in:") == 0) return inStrings(key, split(name.substr(3), '|'), false, s);
    if (name.compare(0, 4, "in~:") == 0) return inStrings(key, split(name.substr(4), '|'), true, s);
    static const std::vector<std::string> weights = {"ultralight", "light",    "normal", "regular", "book",       "medium", "roman",
                                                     "semibold",   "demibold", "demi",   "bold",    "heavy",      "extra bold", "black"};
    static const std::vector<std::string> stretches = {"ultra-condensed", "extra-condensed", "condensed",      "semi-condensed", "normal",
                                                       "semi-expanded",   "expanded",        "extra-expanded", "ultra-expanded"};
    static const std::map<std::string, Validator> validators = {
        {"bool", validateBool},
        {"float", validateFloat},
        {"int", validateInt},
        {"str", validateString},
        {"float_or_none", orNone(validateFloat)},
        {"int_or_none", orNone(validateInt)},
        {"str_or_none", orNone(validateString)},
        {"strlist", [](const Json& v) { return listify(v, validateString, std::nullopt); }},
        {"floatlist", [](const Json& v) { return listify(v, validateFloat, std::nullopt); }},
        {"floatlist/2", [](const Json& v) { return listify(v, validateFloat, 2); }},
        {"intlist/2", [](const Json& v) { return listify(v, validateInt, 2); }},
        {"anylist", [](const Json& v) { return listify(v, [](const Json& item) { return taken(plain(item)); }, std::nullopt); }},
        {"color", validateColor},
        {"color_or_auto", colorOr("auto")},
        {"color_or_inherit", colorOr("inherit")},
        {"color_or_edge", colorOr("edge")},
        {"color_or_none", validateColorOrNone},
        {"color_or_linecolor", validateColorOrLinecolor},
        {"linestyle", validateLinestyle},
        {"linestyle_or_none",
         [](const Json& v) { return v.isNull() || (v.isString() && v.asString() == "None") ? taken(Json()) : validateLinestyle(v); }},
        {"marker", validateMarker},
        {"fontsize", validateFontsize},
        {"fontsize_none", validateFontsizeNone},
        {"fontweight", [](const Json& v) { return nameOrInt(v, weights, "weight"); }},
        {"fontstretch", [](const Json& v) { return nameOrInt(v, stretches, "stretch"); }},
        {"font_properties", validateFontProperties},
        {"pathlike", validateString},
        {"cmap", validateString},
        {"greaterthan_minushalf", [](const Json& v) { return floatWithin(v, true); }},
        {"greaterequal0_lessequal1", [](const Json& v) { return floatWithin(v, false); }},
        {"minor_tick_ndivs", validateMinorTickNdivs},
        {"axisbelow", validateAxisbelow},
        {"dpi", validateDpi},
        {"bbox", validateBbox},
        {"sketch", validateSketch},
        {"legend_loc", validateLegendLoc},
        {"mathtext_fallback", validateMathtextFallback},
        {"fonttype", validateFonttype},
        {"toolbar", validateToolbar},
        {"aspect", validateAspect},
        {"whiskers", validateWhiskers},
        {"hist_bins", validateHistBins},
        {"ps_distiller", validatePsDistiller},
        // validate_cycler: a Cycler object, which a figure file cannot hold, or the text that makes one
        {"cycler", [](const Json& v) { return v.isString() ? cyclerFromText(v.asString(), cycleValues) : refused("the value is not a cycler"); }},
        // numpy reads the date; the names of the backends are those of the Python modules there are
        // _validate_date: np.datetime64(s) parses it (not ported); the default epoch is taken as it is written
        {"date", [](const Json& v) {
             if (v.isString() && v.asString() == "1970-01-01T00:00:00") return taken(v);
             return unsupported("a date");
         }},
        {"backend", [](const Json&) { return unsupported("the choice of a backend"); }},
    };
    if (name == "joinstyle") return validateEnum(key, {"miter", "round", "bevel"}, s);
    if (name == "capstyle") return validateEnum(key, {"butt", "projecting", "round"}, s);
    const auto it = validators.find(name);
    if (it == validators.end()) return unsupported("the kind of value of " + key);
    return it->second(s);
}

const std::map<std::string, std::string>& validatorNames() {
    static const std::map<std::string, std::string> names = [] {
        std::map<std::string, std::string> out;
        const auto doc = support::parseJson(embedded::file("data/rcdefaults.json"), nullptr);
        if (doc)
            if (const Json* validators = doc->find("validators"))
                for (const auto& [key, name] : validators->members()) out[key] = name.asString();
        return out;
    }();
    return names;
}

}  // namespace

Validated validate(const std::string& key, const Json& value) {
    const auto& names = validatorNames();
    const auto it = names.find(key);
    if (it == names.end()) return refused(key + " is not a valid rc parameter");
    Validated v = apply(key, it->second, value);
    if (v.state == State::Refused) v.error = "the value given for " + key + " is not valid: " + v.error;
    return v;
}

}  // namespace calculate_plot::rc
