// Derived from Matplotlib 3.11.2: lib/matplotlib/style/__init__.py (use, _STYLE_BLACKLIST).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "rc/styles.hpp"

#include "support/embedded.hpp"
#include "svg/svg_renderer.h"

#include <map>
#include <set>
#include <utility>

namespace calculate_plot::rc {

namespace {

using support::Json;
using Settings = std::vector<std::pair<std::string, std::string>>;

// The rcParams that are not related to style: a style sheet does not change them.
const std::set<std::string>& blacklist() {
    static const std::set<std::string> keys = {
        "interactive",        "backend",           "webagg.port",      "webagg.address",
        "webagg.port_retries", "webagg.open_in_browser", "backend_fallback", "toolbar",
        "timezone",           "figure.max_open_warning", "figure.raise_window", "savefig.directory",
        "tk.window_focus",    "docstring.hardcopy", "date.epoch"};
    return keys;
}

void load(std::map<std::string, Settings>& library, const char* file) {
    const auto doc = support::parseJson(embedded::file(file), nullptr);
    if (!doc) return;
    if (const Json* styles = doc->find("styles"))
        for (const auto& [name, settings] : styles->members()) {
            Settings& s = library[name];
            for (const auto& [key, text] : settings.members()) s.emplace_back(key, text.asString());
        }
}

const std::map<std::string, Settings>& library() {
    static const std::map<std::string, Settings> loaded = [] {
        std::map<std::string, Settings> l;
        load(l, "data/styles.json");
        load(l, "data/styles_solarized.json");
        return l;
    }();
    return loaded;
}

bool useOne(Params& params, std::string name, std::string* error) {
    if (name == "mpl20") name = "default";
    if (name == "mpl15") name = "classic";
    if (name == "default") {
        for (const std::string& key : params.keys())
            if (!blacklist().count(key)) params.reset(key);
        return true;
    }
    const auto it = library().find(name);
    if (it == library().end()) {
        // The figure file names no style files and no packages: only the library's styles.
        if (error)
            *error = svg::pythonStrRepr(name) +
                     " is not a valid package style, path of style file, URL of style file, or library style name "
                     "(library styles are listed in `style.available`)";
        return false;
    }
    for (const auto& [key, text] : it->second) {
        if (blacklist().count(key)) continue;  // matplotlib warns and ignores it
        std::string e;
        if (!params.set(key, Json::string(text), &e)) {
            if (error) *error = "style " + name + ": " + e;
            return false;
        }
    }
    return true;
}

}  // namespace

const std::vector<std::string>& styleNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> n;
        for (const auto& entry : library()) n.push_back(entry.first);
        return n;
    }();
    return names;
}

bool useStyle(Params& params, const Json& spec, std::string* error) {
    std::vector<std::string> names;
    bool valid = spec.isString() || spec.isArray();
    if (spec.isString()) names.push_back(spec.asString());
    if (spec.isArray())
        for (const Json& name : spec.items()) {
            valid = valid && name.isString();
            if (name.isString()) names.push_back(name.asString());
        }
    if (!valid) {
        if (error) *error = "a style is a style name or a list of style names";
        return false;
    }
    for (const std::string& name : names)
        if (!useOne(params, name, error)) return false;
    return true;
}

}  // namespace calculate_plot::rc
