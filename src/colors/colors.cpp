// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (to_rgba, _to_rgba_no_colorcycle).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "colors/colors.hpp"

#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"
#include "support/pyformat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace calculate_plot::colors {

using support::Json;

namespace {

// _colors_full_map: names to hex strings or (for the base colours) RGB tuples.
const std::map<std::string, Json>& namedColors() {
    static const std::map<std::string, Json> names = [] {
        std::map<std::string, Json> m;
        const auto doc = support::parseJson(embedded::file("data/colors.json"), nullptr);
        if (doc)
            if (const Json* colors = doc->find("colors"))
                for (const auto& [name, value] : colors->members()) m[name] = value;
        return m;
    }();
    return names;
}

std::string lower(std::string s) {
    for (auto& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<Rgba> fromHex(const std::string& c, std::optional<double> alpha, std::string* error) {
    if (c.size() < 2) return fail(error, "Invalid RGBA argument: " + c), std::nullopt;
    for (std::size_t i = 1; i < c.size(); ++i)
        if (hexDigit(c[i]) < 0) return std::nullopt;  // not "#[a-fA-F0-9]+": tried as a grey level next
    std::vector<double> v;
    if (c.size() == 7 || c.size() == 9) {  // #rrggbb, #rrggbbaa
        for (std::size_t i = 1; i < c.size(); i += 2) v.push_back((hexDigit(c[i]) * 16 + hexDigit(c[i + 1])) / 255.0);
    } else if (c.size() == 4 || c.size() == 5) {  // #rgb, #rgba
        for (std::size_t i = 1; i < c.size(); ++i) v.push_back(hexDigit(c[i]) / 15.0);
    } else {
        return fail(error, "Invalid hex color specifier: " + c), std::nullopt;
    }
    if (v.size() == 3) return Rgba{v[0], v[1], v[2], alpha ? *alpha : 1.0};
    return Rgba{v[0], v[1], v[2], alpha ? *alpha : v[3]};
}

bool isHexSpec(const std::string& c) {
    if (c.size() < 2 || c[0] != '#') return false;
    for (std::size_t i = 1; i < c.size(); ++i)
        if (hexDigit(c[i]) < 0) return false;
    return true;
}

}  // namespace

std::optional<Rgba> toRgba(const std::vector<double>& c, std::optional<double> alpha, std::string* error) {
    if (alpha && !(0 <= *alpha && *alpha <= 1)) return fail(error, "'alpha' must be between 0 and 1, inclusive"), std::nullopt;
    if (c.size() != 3 && c.size() != 4) return fail(error, "RGBA sequence should have length 3 or 4"), std::nullopt;
    Rgba out{c[0], c[1], c[2], c.size() == 4 ? c[3] : 1.0};
    if (alpha) out[3] = *alpha;
    for (double e : out)
        if (e < 0 || e > 1) return fail(error, "RGBA values should be within 0-1 range"), std::nullopt;
    return out;
}

std::optional<Rgba> toRgba(const std::string& spec, std::optional<double> alpha, const rc::Params& rc,
                           std::string* error) {
    // The nth colour of the property cycle: "C0", "C12", ...
    if (spec.size() >= 2 && spec[0] == 'C' &&
        std::all_of(spec.begin() + 1, spec.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
        const auto cycle = rc.colorCycle();
        if (cycle.empty()) return toRgba(std::string("k"), alpha, rc, error);
        const unsigned long n = std::stoul(spec.substr(1));
        return toRgba(cycle[n % cycle.size()], alpha, error);
    }
    if (alpha && !(0 <= *alpha && *alpha <= 1)) return fail(error, "'alpha' must be between 0 and 1, inclusive"), std::nullopt;
    if (lower(spec) == "none") return Rgba{0.0, 0.0, 0.0, 0.0};
    // Named colour: as written, else in lower case (except one-letter names).
    const auto& names = namedColors();
    const Json* named = nullptr;
    if (auto it = names.find(spec); it != names.end()) named = &it->second;
    else if (spec.size() != 1)
        if (auto lit = names.find(lower(spec)); lit != names.end()) named = &lit->second;
    if (named && named->isArray()) {
        std::vector<double> v;
        for (const Json& x : named->items()) v.push_back(x.asNumber());
        return toRgba(v, alpha, error);
    }
    const std::string c = named ? named->asString() : spec;
    if (isHexSpec(c)) return fromHex(c, alpha, error);
    // A grey level, as Python's float() reads it.
    std::string trimmed = c;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v"));
    double grey = 0;
    const std::string low = lower(trimmed);
    const bool special = low.find("inf") != std::string::npos || low.find("nan") != std::string::npos;
    if (!trimmed.empty() && !special && (std::isdigit(static_cast<unsigned char>(trimmed[0])) || trimmed[0] == '.' ||
                                         trimmed[0] == '-' || trimmed[0] == '+') &&
        low.find('x') == std::string::npos && support::readDouble(trimmed, grey)) {
        if (!(0 <= grey && grey <= 1))
            return fail(error, "Invalid string grayscale value " + spec + ". Value must be within 0-1 range"), std::nullopt;
        return Rgba{grey, grey, grey, alpha ? *alpha : 1.0};
    }
    return fail(error, "Invalid RGBA argument: " + spec), std::nullopt;
}

std::optional<Rgba> toRgba(const Spec& spec, std::optional<double> alpha, const rc::Params& rc, std::string* error) {
    if (const auto* text = std::get_if<std::string>(&spec)) return toRgba(*text, alpha, rc, error);
    return toRgba(std::get<std::vector<double>>(spec), alpha, error);
}

bool isNone(const Spec& spec) {
    const auto* text = std::get_if<std::string>(&spec);
    if (!text) return false;
    std::string lower = *text;
    for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    return lower == "none";
}

}  // namespace calculate_plot::colors
