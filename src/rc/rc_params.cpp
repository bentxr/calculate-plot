#include "rc/rc_params.hpp"

#include <algorithm>
#include <cmath>

#include "rc/rc_validate.hpp"
#include "support/embedded.hpp"

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

// Whether a stored value is the default of its key (a number by its value), for the keys that take
// their default only.
bool same(const Json& a, const Json& b) {
    if (a.type() != b.type()) return false;
    if (a.isBool()) return a.asBool() == b.asBool();
    if (a.isNumber()) return a.asNumber() == b.asNumber();
    if (a.isString()) return a.asString() == b.asString();
    if (a.isArray()) return a.size() == 0 && b.size() == 0;  // (the lists compared here are empty by default)
    return true;
}

// No number in the value is a nan or an infinity.
bool finite(const Json& v) {
    if (v.isNumber()) return std::isfinite(v.asNumber());
    for (const Json& item : v.items())
        if (!finite(item)) return false;
    return true;
}

// No integer in the value is beyond 32 bits (the engine reads them as int).
bool fitsInt(const Json& v) {
    if (v.isNumber()) return !v.isInteger() || std::fabs(v.asNumber()) <= 2147483647.0;
    for (const Json& item : v.items())
        if (!fitsInt(item)) return false;
    return true;
}

// Whether the engine goes through a property cycle: one with entries, its colours texts or RGB(A)
// numbers (not a colour with an alpha apart), its line styles and markers names, its numbers finite.
bool cycleIsRead(const Json& value) {
    for (const auto& [property, values] : value.find("$cycler")->members()) {
        if (values.size() == 0) return false;
        const bool isColor = property.size() >= 5 && property.compare(property.size() - 5, 5, "color") == 0;
        const bool isName = property == "linestyle" || property == "marker";
        for (const Json& v : values.items()) {
            if (isColor) {
                if (!v.isString() && !(v.isArray() && v[0].isNumber())) return false;
            } else if (isName ? !v.isString() : !std::isfinite(v.asNumber())) {
                return false;
            }
        }
    }
    return true;
}

// Whether the engine reads a value of kind `got` for a key whose default is of kind `want`: the
// same kind as the default, anything where the default is None, and for a few keys whose default is
// a name a number as well (savefig.dpi: 'figure'; the font sizes: 'medium', 'large'; the font
// weights and the stretch: 'normal') or None (mathtext.fallback, legend.framealpha).
bool readable(const std::string& key, Kind want, Kind got) {
    static const char* const kNumberOrName[] = {
        "savefig.dpi",     "axes.labelsize",   "axes.titlesize",  "figure.titlesize",   "legend.fontsize",
        "xtick.labelsize", "ytick.labelsize",  "axes.labelweight", "axes.titleweight",  "figure.titleweight",
        "font.weight",     "font.stretch"};
    if (want == Kind::Null || got == want) return true;
    if (got == Kind::Null) return key == "mathtext.fallback" || key == "legend.framealpha";
    if (key == "xtick.minor.ndivs" || key == "ytick.minor.ndivs") return true;  // 'auto' or a number
    if (key == "axes.axisbelow") return true;                                    // 'line', True or False
    // (their validators give a name or a number, nothing else)
    for (const char* k : kNumberOrName)
        if (key == k) return true;
    return false;
}

std::set<std::string>* readLog = nullptr;

}  // namespace

const Params& Params::standard() {
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

Params Params::defaults() { return standard(); }

void Params::reset(const std::string& key) {
    const auto it = standard().values_.find(key);
    if (it != standard().values_.end()) values_[key] = it->second;
}

std::vector<std::string> Params::keys() const {
    std::vector<std::string> out;
    for (const auto& [key, value] : values_) out.push_back(key);
    return out;
}

const Json& Params::value(const std::string& key) const {
    static const Json null;
    if (readLog) readLog->insert(key);
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

std::vector<std::vector<std::pair<std::string, Json>>> Params::propCycle() const {
    std::vector<std::vector<std::pair<std::string, Json>>> out;
    for (const auto& [property, values] : value("axes.prop_cycle").find("$cycler")->members()) {
        out.resize(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) out[i].emplace_back(property, values[i]);
    }
    return out;
}

std::vector<Json> Params::colorCycle() const {
    const Json* colors = value("axes.prop_cycle").find("$cycler")->find("color");
    return colors ? colors->items() : std::vector<Json>{Json::string("k")};
}

const std::vector<std::string>& Params::inertKeys() {
    // Animations, 3D axes, box plots, contours, dates, images, the keys and windows of the interactive
    // backends, PDF, PGF and PostScript output, and the like. And figure.frameon, which pyplot's figures
    // never read (pyplot.figure has its own frameon=True).
    static const std::vector<std::string> keys = {
        "animation.bitrate", "animation.codec", "animation.convert_args", "animation.convert_path",
        "animation.embed_limit", "animation.ffmpeg_args", "animation.ffmpeg_path", "animation.frame_format",
        "animation.html", "animation.writer", "axes.zmargin", "axes3d.automargin", "axes3d.depthshade",
        "axes3d.depthshade_minalpha", "axes3d.grid", "axes3d.mouserotationstyle", "axes3d.snap_rotation",
        "axes3d.trackballborder", "axes3d.trackballsize", "axes3d.xaxis.panecolor", "axes3d.yaxis.panecolor",
        "axes3d.zaxis.panecolor", "backend_fallback", "boxplot.bootstrap", "boxplot.boxprops.color",
        "boxplot.boxprops.linestyle", "boxplot.boxprops.linewidth", "boxplot.capprops.color",
        "boxplot.capprops.linestyle", "boxplot.capprops.linewidth", "boxplot.flierprops.color",
        "boxplot.flierprops.linestyle", "boxplot.flierprops.linewidth", "boxplot.flierprops.marker",
        "boxplot.flierprops.markeredgecolor", "boxplot.flierprops.markeredgewidth",
        "boxplot.flierprops.markerfacecolor", "boxplot.flierprops.markersize", "boxplot.meanline",
        "boxplot.meanprops.color", "boxplot.meanprops.linestyle", "boxplot.meanprops.linewidth",
        "boxplot.meanprops.marker", "boxplot.meanprops.markeredgecolor", "boxplot.meanprops.markerfacecolor",
        "boxplot.meanprops.markersize", "boxplot.medianprops.color", "boxplot.medianprops.linestyle",
        "boxplot.medianprops.linewidth", "boxplot.notch", "boxplot.patchartist", "boxplot.showbox",
        "boxplot.showcaps", "boxplot.showfliers", "boxplot.showmeans", "boxplot.vertical",
        "boxplot.whiskerprops.color", "boxplot.whiskerprops.linestyle", "boxplot.whiskerprops.linewidth",
        "boxplot.whiskers", "contour.algorithm", "contour.corner_mask", "contour.linewidth",
        "contour.negative_linestyle", "date.autoformatter.day", "date.autoformatter.hour",
        "date.autoformatter.microsecond", "date.autoformatter.minute", "date.autoformatter.month",
        "date.autoformatter.second", "date.autoformatter.year", "date.converter", "date.epoch",
        "date.interval_multiples", "docstring.hardcopy", "figure.constrained_layout.h_pad",
        "figure.constrained_layout.hspace", "figure.constrained_layout.w_pad", "figure.constrained_layout.wspace",
        "figure.frameon", "figure.labelsize", "figure.labelweight", "figure.max_open_warning", "figure.raise_window",
        "image.aspect", "image.composite_image", "image.interpolation",
        "image.interpolation_stage", "image.lut", "image.origin", "image.resample", "interactive", "keymap.back",
        "keymap.copy", "keymap.forward", "keymap.fullscreen", "keymap.grid", "keymap.grid_minor", "keymap.help",
        "keymap.home", "keymap.pan", "keymap.quit", "keymap.quit_all", "keymap.save", "keymap.xscale",
        "keymap.yscale", "keymap.zoom", "macosx.window_mode", "pcolor.shading", "pcolormesh.snap", "pdf.compression",
        "pdf.fonttype", "pdf.inheritcolor", "pdf.use14corefonts", "pgf.preamble", "pgf.rcfonts", "pgf.texsystem",
        "polaraxes.grid", "ps.distiller.res", "ps.fonttype", "ps.papersize", "ps.useafm", "ps.usedistiller",
        "savefig.directory", "savefig.format", "savefig.orientation", "text.latex.engine", "text.latex.preamble",
        "timezone", "tk.window_focus", "toolbar", "webagg.address", "webagg.open_in_browser", "webagg.port",
        "webagg.port_retries"};
    return keys;
}

const std::vector<std::string>& Params::unsupportedKeys() {
    static const std::vector<std::string> keys = {
        "axes.formatter.use_locale", "backend",
        "figure.autolayout", "figure.constrained_layout.use", "figure.hooks", "font.enable_last_resort",
        "path.effects", "path.sketch", "path.snap", "text.hinting_factor", "text.kerning_factor", "text.usetex"};
    return keys;
}

void Params::logReads(std::set<std::string>* log) { readLog = log; }

bool Params::set(const std::string& key, const Json& v, std::string* error) {
    const auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    auto it = values_.find(key);
    if (it == values_.end()) return fail(key + " is not a valid rc parameter");
    // rcParams[key] = v: the validator of the key takes the value, in the form it stores, or refuses it
    Validated checked = validate(key, v);
    if (checked.state != Validated::State::Taken) return fail(checked.error);
    const auto listed = [&](const std::vector<std::string>& keys) { return std::binary_search(keys.begin(), keys.end(), key); };
    // FontProperties.set_weight and set_stretch raise for a number that is not from 0 to 1000, as soon
    // as a text takes these (every Axes makes its title and labels)
    if (key == "font.weight" || key == "font.stretch" || key == "axes.titleweight" || key == "axes.labelweight")
        if (checked.value.isNumber() && !(0 <= checked.value.asNumber() && checked.value.asNumber() <= 1000))
            return fail("the value given for " + key + " is not valid: FontProperties takes a name or a number from 0 to 1000");
    // The same for a style or a variant that is not one (they are validated as any string)
    const std::string& word = checked.value.asString();
    if (key == "font.style" && word != "normal" && word != "italic" && word != "oblique")
        return fail("the value given for font.style is not valid: FontProperties takes normal, italic or oblique");
    if (key == "font.variant" && word != "normal" && word != "small-caps")
        return fail("the value given for font.variant is not valid: FontProperties takes normal or small-caps");
    // Every tick has a grid line, made with the grid's alpha: Artist.set_alpha takes 0 to 1
    if (key == "grid.alpha" || key == "grid.major.alpha" || key == "grid.minor.alpha")
        if (checked.value.isNumber() && !(0 <= checked.value.asNumber() && checked.value.asNumber() <= 1))
            return fail("the value given for " + key + " is not valid: an alpha is from 0 to 1");
    if (!listed(inertKeys())) {
        const Json& standard = Params::standard().values_.at(key);
        if (listed(unsupportedKeys())) {
            if (!same(checked.value, standard)) return fail("rcParams[\"" + key + "\"] is not supported yet");
        } else if (!finite(checked.value) || !fitsInt(checked.value) || !readable(key, kindOf(standard), kindOf(checked.value)) ||
                   (key == "axes.formatter.offset_threshold" && checked.value.asNumber() > 23) ||
                   ((key == "xtick.minor.ndivs" || key == "ytick.minor.ndivs") && checked.value.isString() &&
                    checked.value.asString() != "auto") ||
                   ((key == "grid.major.linestyle" || key == "grid.minor.linestyle" || key == "grid.major.color" ||
                     key == "grid.minor.color") &&
                    checked.value.isArray()) ||
                   (key == "axes.prop_cycle" && !cycleIsRead(checked.value))) {
            // (10 ** (offset_threshold - 1) is an integer of any size in Python, and a double here, exact up to
            // 1e22. 'auto' in capitals is taken by the validator and breaks AutoMinorLocator, which looks for 'auto'.
            // An on-off sequence as the line style of the grid, or a tuple as its colour, is not read yet.)
            return fail("this value of " + key + " is not supported yet");
        }
    }
    it->second = std::move(checked.value);
    return true;
}

}  // namespace calculate_plot::rc
