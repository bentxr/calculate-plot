// Derived from Matplotlib 3.11.2: lib/matplotlib/font_manager.py (FontProperties' defaults, the
// font scoring and FontManager._find_fonts_by_props / _findfont_cached).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/font_manager.hpp"

#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>

namespace calculate_plot::text {

namespace {

const std::map<std::string, double> kFontScalings = {
    {"xx-small", 0.579}, {"x-small", 0.694}, {"small", 0.833}, {"medium", 1.0}, {"large", 1.200},
    {"x-large", 1.440},  {"xx-large", 1.728}, {"larger", 1.2},  {"smaller", 0.833}};

const std::map<std::string, int> kStretch = {
    {"ultra-condensed", 100}, {"extra-condensed", 200}, {"condensed", 300},     {"semi-condensed", 400},
    {"normal", 500},          {"semi-expanded", 600},   {"semi-extended", 600}, {"expanded", 700},
    {"extended", 700},        {"extra-expanded", 800},  {"extra-extended", 800}, {"ultra-expanded", 900},
    {"ultra-extended", 900}};

const std::map<std::string, int> kWeight = {
    {"ultralight", 100}, {"light", 200}, {"normal", 400},   {"regular", 400}, {"book", 400},
    {"medium", 500},     {"roman", 500}, {"semibold", 600}, {"demibold", 600}, {"demi", 600},
    {"bold", 700},       {"heavy", 800}, {"extra bold", 800}, {"black", 900}};

const char* const kAliases[] = {"serif", "sans-serif", "sans serif", "cursive", "fantasy", "monospace", "sans"};

const char* const kDefaultFamily = "DejaVu Sans";  // FontManager.defaultFamily['ttf']

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool isAlias(const std::string& family) {
    return std::find(std::begin(kAliases), std::end(kAliases), family) != std::end(kAliases);
}

// FontManager._expand_aliases
std::vector<std::string> expandAliases(std::string family, const rc::Params& rc) {
    if (family == "sans" || family == "sans serif") family = "sans-serif";
    return rc.strings("font." + family);
}

double scoreFamily(const std::vector<std::string>& families, const std::string& name, const rc::Params& rc) {
    if (families.empty()) return 1.0;
    const std::string family2 = lower(name);
    const double step = 1.0 / double(families.size());
    for (std::size_t i = 0; i < families.size(); ++i) {
        const std::string family1 = lower(families[i]);
        if (isAlias(family1)) {
            std::vector<std::string> options = expandAliases(family1, rc);
            for (auto& o : options) o = lower(o);
            const auto it = std::find(options.begin(), options.end(), family2);
            if (it != options.end()) {
                const double idx = double(it - options.begin());
                return (double(i) + (idx / double(options.size()))) * step;
            }
        } else if (family1 == family2) {
            // The score should be weighted by where in the list the font was found.
            return double(i) * step;
        }
    }
    return 1.0;
}

double scoreStyle(const std::string& style1, const std::string& style2) {
    if (style1 == style2) return 0.0;
    const auto slanted = [](const std::string& s) { return s == "italic" || s == "oblique"; };
    if (slanted(style1) && slanted(style2)) return 0.1;
    return 1.0;
}

double scoreVariant(const std::string& variant1, const std::string& variant2) {
    return variant1 == variant2 ? 0.0 : 1.0;
}

// int(stretch), else stretch_dict.get(stretch, 500)
int stretchValue(const FontValue& v) {
    if (const int* i = std::get_if<int>(&v)) return *i;
    const std::string& s = std::get<std::string>(v);
    char* end = nullptr;
    const long n = std::strtol(s.c_str(), &end, 10);
    if (!s.empty() && *end == '\0') return int(n);
    const auto it = kStretch.find(s);
    return it == kStretch.end() ? 500 : it->second;
}

double scoreStretch(const FontValue& stretch1, const FontValue& stretch2) {
    return std::abs(stretchValue(stretch1) - stretchValue(stretch2)) / 1000.0;
}

// _normalize_weight
int weightValue(const FontValue& v) {
    if (const int* i = std::get_if<int>(&v)) return *i;
    return kWeight.at(std::get<std::string>(v));
}

double scoreWeight(const FontValue& weight1, const FontValue& weight2) {
    // exact match of the weight names, e.g. weight1 == weight2 == "regular"
    const auto* s1 = std::get_if<std::string>(&weight1);
    const auto* s2 = std::get_if<std::string>(&weight2);
    if (s1 && s2 && *s1 == *s2) return 0.0;
    const int w1 = weightValue(weight1), w2 = weightValue(weight2);
    return 0.95 * (std::abs(w1 - w2) / 1000.0) + 0.05;
}

double scoreSize(double size1, const std::string& size2) {
    if (size2 == "scalable") return 0.0;
    char* end = nullptr;
    const double sizeval2 = std::strtod(size2.c_str(), &end);
    if (size2.empty() || *end != '\0') return 1.0;
    return std::abs(size1 - sizeval2) / 72;
}

// FontManager._findfont_cached for a prop with one family; nullopt where matplotlib fails.
std::optional<std::string> findFontCached(const FontProperties& prop, const std::vector<FontEntry>& fonts,
                                    bool fallbackToDefault, const rc::Params& rc) {  // _findfont_cached
    double bestScore = 1e64;
    const FontEntry* best = nullptr;
    for (const FontEntry& font : fonts) {
        // Matching family should have top priority, so multiply it by 10.
        const double score = scoreFamily(prop.family, font.name, rc) * 10 + scoreStyle(prop.style, font.style) +
                             scoreVariant(prop.variant, font.variant) + scoreWeight(prop.weight, font.weight) +
                             scoreStretch(prop.stretch, font.stretch) + scoreSize(prop.size, font.size);
        if (score < bestScore) {
            bestScore = score;
            best = &font;
        }
        if (score == 0) break;
    }
    if (!best || bestScore >= 10.0) {
        if (!fallbackToDefault) return std::nullopt;
        FontProperties defaultProp = prop;
        defaultProp.family = {kDefaultFamily};
        return findFontCached(defaultProp, fonts, false, rc);
    }
    return best->file;
}

FontValue fontValue(const support::Json& j) {
    if (j.isNumber()) return int(j.asInteger());
    return j.asString();
}

}  // namespace

FontProperties FontProperties::defaults(const rc::Params& rc) {
    FontProperties p;
    p.family = rc.strings("font.family");
    p.style = rc.text("font.style");
    p.variant = rc.text("font.variant");
    p.weight = fontValue(rc.value("font.weight"));
    p.stretch = fontValue(rc.value("font.stretch"));
    p.size = rc.number("font.size");
    return p;
}

const std::vector<FontEntry>& bundledFonts() {
    static const std::vector<FontEntry> fonts = [] {
        std::vector<FontEntry> out;
        const auto doc = support::parseJson(embedded::file("data/fonts.json"), nullptr);
        if (!doc) return out;
        const support::Json* list = doc->find("fonts");
        if (!list) return out;
        for (const support::Json& f : list->items())
            out.push_back(FontEntry{f.find("file")->asString(), f.find("name")->asString(),
                                    f.find("style")->asString(), f.find("variant")->asString(),
                                    fontValue(*f.find("weight")), fontValue(*f.find("stretch")),
                                    f.find("size")->asString()});
        return out;
    }();
    return fonts;
}

std::optional<double> fontSize(const std::string& name, const rc::Params& rc) {
    const auto it = kFontScalings.find(name);
    if (it == kFontScalings.end()) return std::nullopt;
    return it->second * rc.number("font.size");
}

std::string findFont(const FontProperties& prop, const rc::Params& rc, const std::vector<FontEntry>* fonts) {
    const auto found = findFontCached(prop, fonts ? *fonts : bundledFonts(), true, rc);
    return found ? *found : std::string();
}

std::vector<std::string> findFontsByProps(const FontProperties& prop, const rc::Params& rc,
                                          const std::vector<FontEntry>* fonts) {
    const std::vector<FontEntry>& list = fonts ? *fonts : bundledFonts();
    std::vector<std::string> paths;
    for (const std::string& family : prop.family) {
        FontProperties one = prop;
        one.family = {family};
        if (auto found = findFontCached(one, list, false, rc)) paths.push_back(*found);
    }
    // only add default family if no other font was found
    if (paths.empty()) {
        FontProperties fallback = prop;
        fallback.family = {kDefaultFamily};
        if (auto found = findFontCached(fallback, list, true, rc)) paths.push_back(*found);
    }
    return paths;
}

}  // namespace calculate_plot::text
