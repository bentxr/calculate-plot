#include "rc/rc_params.hpp"

#include <algorithm>
#include <cstring>

#include "support/embedded.hpp"
#include "support/fontconfig_pattern.hpp"
#include "support/json_writer.hpp"

namespace calculate_plot::rc {

using support::Json;

namespace {

// The kind of a value, for checking a new value against the default's.
enum class Kind { Null, Bool, Number, String, List, Other };

Kind kindOf(const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: return Kind::Null;
    case Json::Type::Bool: return Kind::Bool;
    case Json::Type::Number: return Kind::Number;
    case Json::Type::String: return Kind::String;
    case Json::Type::Array: return Kind::List;
    default: return Kind::Other;
    }
}

}  // namespace

Params Params::defaults() {
    static const Params loaded = [] {
        Params p;
        const auto doc = support::parseJson(embedded::file("data/rcdefaults.json"), nullptr);
        if (doc)
            if (const Json* values = doc->find("rcParams"))
                for (const auto& [key, value] : values->members()) p.values_[key] = value;
        return p;
    }();
    return loaded;
}

const Json& Params::value(const std::string& key) const {
    static const Json null;
    auto it = values_.find(key);
    return it == values_.end() ? null : it->second;
}

double Params::number(const std::string& key) const { return value(key).asNumber(); }
bool Params::boolean(const std::string& key) const { return value(key).asBool(); }
std::string Params::text(const std::string& key) const { return value(key).asString(); }

std::vector<double> Params::numbers(const std::string& key) const {
    std::vector<double> out;
    for (const Json& v : value(key).items()) out.push_back(v.asNumber());
    return out;
}

std::vector<std::string> Params::strings(const std::string& key) const {
    std::vector<std::string> out;
    for (const Json& v : value(key).items()) out.push_back(v.asString());
    return out;
}

std::vector<std::vector<double>> Params::colorCycle() const {
    std::vector<std::vector<double>> out;
    const Json* cycler = value("axes.prop_cycle").find("$cycler");
    const Json* colors = cycler ? cycler->find("color") : nullptr;
    if (!colors) return out;
    for (const Json& c : colors->items()) {
        std::vector<double> rgb;
        for (const Json& x : c.items()) rgb.push_back(x.asNumber());
        out.push_back(rgb);
    }
    return out;
}

bool Params::set(const std::string& key, const Json& v, std::string* error) {
    auto it = values_.find(key);
    if (it == values_.end()) {
        if (error) *error = key + " is not a valid rc parameter";
        return false;
    }
    // The mathtext settings that are a choice (ValidateInStrings, _validate_mathtext_fallback)
    static const std::vector<std::string> kFontsets = {"dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"};
    static const std::vector<std::string> kMathDefaults = {"rm", "cal", "bfit", "it", "tt", "sf", "bf", "default", "bb",
                                                           "frak", "scr", "regular", "normal"};
    if (key == "mathtext.fontset" || key == "mathtext.default") {
        const auto& valid = key == "mathtext.fontset" ? kFontsets : kMathDefaults;
        if (v.isString() && std::find(valid.begin(), valid.end(), v.asString()) == valid.end()) {
            if (error) {
                *error = "'" + v.asString() + "' is not a valid value for " + key + "; supported values are";
                for (const auto& name : valid) *error += " " + name;
            }
            return false;
        }
    }
    // validate_font_properties: the fonts of the custom math fontset are fontconfig patterns
    static const char* const kPatterns[] = {"mathtext.rm", "mathtext.it", "mathtext.bf", "mathtext.bfit",
                                            "mathtext.cal", "mathtext.sf", "mathtext.tt"};
    if (std::find_if(std::begin(kPatterns), std::end(kPatterns), [&](const char* k) { return key == k; }) != std::end(kPatterns) &&
        v.isString() && !support::parseFontconfigPattern(v.asString())) {
        if (error) *error = "'" + v.asString() + "' is not a fontconfig pattern";
        return false;
    }
    // validate_stringlist: the font lists take a list of strings (the empty ones go), or one
    // string with the names between commas
    static const char* const kFontLists[] = {"font.family", "font.serif", "font.sans-serif",
                                             "font.cursive", "font.fantasy", "font.monospace"};
    if (std::find_if(std::begin(kFontLists), std::end(kFontLists), [&](const char* k) { return key == k; }) != std::end(kFontLists)) {
        std::vector<std::string> names;
        if (v.isString()) {
            const std::string& text = v.asString();
            for (std::size_t a = 0; a <= text.size();) {
                const std::size_t comma = std::min(text.find(',', a), text.size());
                std::size_t from = a, to = comma;
                // (str.strip(): the blanks of ASCII)
                while (from < to && std::strchr(" \t\n\r\f\v", text[from])) ++from;
                while (to > from && std::strchr(" \t\n\r\f\v", text[to - 1])) --to;
                if (to > from) names.push_back(text.substr(from, to - from));
                a = comma + 1;
            }
        } else if (v.isArray()) {
            for (const Json& item : v.items()) {
                if (!item.isString()) {
                    if (error) *error = "the value given for " + key + " is not a list of strings";
                    return false;
                }
                if (!item.asString().empty()) names.push_back(item.asString());
            }
        } else {
            if (error) *error = "the value given for " + key + " is not of the right kind";
            return false;
        }
        support::JsonWriter list;
        list.beginArray();
        for (const std::string& name : names) list.value(name);
        list.endArray();
        it->second = *support::parseJson(list.text(), nullptr);
        return true;
    }
    if (key == "mathtext.fallback") {
        if (!v.isString() && !v.isNull()) {
            if (error) *error = "the value given for " + key + " is not of the right kind";
            return false;
        }
        std::string name = v.isString() ? v.asString() : "none";
        for (char& c : name) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        if (name == "none") {
            it->second = Json();
        } else if (name == "cm" || name == "stix" || name == "stixsans") {
            it->second = *support::parseJson("\"" + name + "\"", nullptr);
        } else {
            if (error)
                *error = name + " is not a valid fallback font name. Valid fallback font names are cm,stix,stixsans. "
                                "Passing 'None' will turn fallback off.";
            return false;
        }
        return true;
    }
    const Kind want = kindOf(it->second), got = kindOf(v);
    // A default of None accepts anything matplotlib would validate; otherwise the kinds must agree.
    // A few defaults are names where a number is as good: savefig.dpi ('figure'), the font sizes
    // ('medium', 'large') and the font weights ('normal').
    static const char* const kNumberOrName[] = {
        "savefig.dpi",      "axes.labelsize",    "axes.titlesize",   "figure.labelsize",   "figure.titlesize",
        "legend.fontsize",  "xtick.labelsize",   "ytick.labelsize",  "axes.labelweight",   "axes.titleweight",
        "figure.labelweight", "figure.titleweight", "font.weight"};
    bool numberForName = false;
    if (got == Kind::Number)
        for (const char* k : kNumberOrName) numberForName = numberForName || key == k;
    if (want != Kind::Null && got != want && !numberForName) {
        if (error) *error = "the value given for " + key + " is not of the right kind";
        return false;
    }
    it->second = v;
    return true;
}

}  // namespace calculate_plot::rc
