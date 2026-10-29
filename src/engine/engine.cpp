#include "engine/engine.hpp"
#include "tri/triangulation.hpp"

#include "agg/path_text.h"
#include "axes/dates.hpp"
#include "engine/annotation.hpp"
#include "engine/arrows.hpp"
#include "engine/axes.hpp"
#include "engine/colorbar.hpp"
#include "engine/contour_labels.hpp"
#include "support/log10.hpp"
#include "engine/contour_set.hpp"
#include "engine/table.hpp"
#include "support/blas_dot.hpp"
#include "support/npy_correlate.hpp"
#include "engine/quiver.hpp"
#include "engine/streamplot.hpp"
#include "engine/figure.hpp"
#include "engine/image.hpp"
#include "engine/inset.hpp"
#include "engine/legend.hpp"
#include "engine/tight_layout.hpp"
#include "engine/violin.hpp"
#include "rc/rc_params.hpp"
#include "rc/styles.hpp"
#include "support/histogram.hpp"
#include "support/pyformat.hpp"
#include "support/raised.hpp"
#include "svg/svg_renderer.h"
#include "support/pyparse.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>

namespace calculate_plot::engine {

namespace {

using support::Json;

// A tuple or list value's elements ({"$tuple": [...]} or [...]); nullptr for anything else.
const Json* sequence(const Json& v) {
    if (v.isArray()) return &v;
    if (v.isObject() && v.members().size() == 1 && v.members()[0].first == "$tuple") return &v.members()[0].second;
    return nullptr;
}

// A date of a figure file: {"$datetime": "YYYY-MM-DDTHH:MM:SS[.ffffff]"}
bool isDatetime(const Json& v) { return v.isObject() && v.members().size() == 1 && v.members()[0].first == "$datetime"; }

// How many dates a value holds, at any depth
std::size_t datetimes(const Json& v) {
    if (isDatetime(v)) return 1;
    std::size_t n = 0;
    if (v.isArray())
        for (const Json& item : v.items()) n += datetimes(item);
    if (v.isObject())
        for (const auto& member : v.members()) n += datetimes(member.second);
    return n;
}

// A number, or a date as its day number (date2num: what the axes' DateConverter makes of it; which
// calls take dates is decided before, see Executor::datesTaken)
bool readNumber(const Json& v, double* out) {
    if (isDatetime(v)) {
        axes::dates::DateTime d;
        if (!v.members()[0].second.isString() || !axes::dates::parseIso(v.members()[0].second.asString(), &d)) return false;
        *out = axes::dates::date2num(d);
        return true;
    }
    if (v.isObject() && v.members().size() == 1 && v.members()[0].first == "$float") {
        const std::string& f = v.members()[0].second.asString();
        *out = f == "nan" ? std::numeric_limits<double>::quiet_NaN()
                          : (f == "inf" ? 1 : -1) * std::numeric_limits<double>::infinity();
        return true;
    }
    if (!v.isNumber()) return false;
    *out = v.asNumber();
    return true;
}

// Whether an integer written in decimal is one numpy holds as such: from -2**63 to 2**64 - 1.
bool fitsIn64Bits(const std::string& text) {
    const bool negative = !text.empty() && text[0] == '-';
    const std::string digits = text.substr(text.find_first_not_of("+-") == std::string::npos ? text.size() : text.find_first_not_of("+-"));
    const std::string most = negative ? "9223372036854775808" : "18446744073709551615";
    return digits.size() < most.size() || (digits.size() == most.size() && digits <= most);
}

// A width, a size or a zorder: a finite number. (matplotlib takes nan and infinity there; what it
// then draws, or breaks on, is not ported.)
bool readFinite(const Json& v, double* out) { return readNumber(v, out) && std::isfinite(*out); }

// Artist.set_alpha: a number from 0 to 1.
bool readAlpha(const Json& v, double* out) { return readNumber(v, out) && 0 <= *out && *out <= 1; }

// The line widths of a collection: at least one (matplotlib breaks on an empty list), all finite.
bool finiteWidths(const std::vector<double>& widths) {
    return !widths.empty() && std::all_of(widths.begin(), widths.end(), [](double w) { return std::isfinite(w); });
}

// The figure file's calls, run against the objects they make.
class Executor {
public:
    Executor(rc::Params rc, DrawOptions options) : rc_(std::move(rc)), options_(std::move(options)) {}

    bool run(const model::Call& call, std::string* error) {
        if (!positionalArguments(call, error)) return false;
        // dates go only to the Axes' calls (datesTaken says which of those take them)
        std::size_t dates = 0;
        for (const Json& a : call.args) dates += datetimes(a);
        for (const auto& kw : call.kwargs) dates += datetimes(kw.second);
        if (dates > 0 && (call.on.empty() ? pyplotOfAxes().count(call.method) == 0 : axes_.count(call.on) == 0))
            return *error = (call.on.empty() ? std::string("plt") : call.on) + "." + call.method + ": dates are not supported yet", false;
        if (call.on.empty() && call.method == "figure") return figure(call, error);
        if (call.on.empty() && call.method == "subplots") return subplots(call, error);
        if (call.on.empty()) {
            if (call.method == "gcf") {  // the current figure, made if there is none
                gcf();
                return nameResult(call, nullptr, error);
            }
            if (call.method == "gca") return noArguments(call, error) && nameResult(call, &gca(), error);
            if (call.method == "twinx" || call.method == "twiny") {  // pyplot: of the current Axes (ax=None)
                const Json* a = call.args.empty() ? keyword(call, "ax") : &call.args[0];
                if ((a && !a->isNull()) || call.args.size() > 1 || call.kwargs.size() > (call.args.empty() && a ? 1u : 0u))
                    return *error = "plt." + call.method + ": only ax=None is supported yet", false;
                Axes* t = gca().twin(call.method == "twinx", error);
                if (!t) return *error = "plt." + call.method + ": " + *error, false;
                current_ = t;
                return nameResult(call, t, error);
            }
            if (call.method == "subplot") return subplot(call, error);
            if (call.method == "sca") {  // plt.sca(ax): the current Axes
                const Json* a = call.args.empty() ? keyword(call, "ax") : &call.args[0];
                Axes* target = a ? axesOf(*a) : nullptr;
                if (!target || call.args.size() + call.kwargs.size() != 1) return *error = "plt.sca: give one Axes of the figure ({\"$ref\": name})", false;
                current_ = target;
                return true;
            }
            if (call.method == "Polygon") return polygon(call, error);
            if (call.method == "axes") {
                // plt.axes(arg=None, **kwargs): add_subplot() without a rect, else add_axes(rect); of the keywords
                // the projection so far
                if (call.args.size() > 1) return *error = "plt.axes: too many positional arguments", false;
                const Json* rect = call.args.empty() ? keyword(call, "arg") : &call.args[0];
                if (!rect || rect->isNull()) rect = keyword(call, "position");
                for (const auto& kw : call.kwargs)
                    if (kw.first != "arg" && kw.first != "position" && kw.first != "projection" && kw.first != "polar")
                        return *error = "plt.axes: keyword arguments are not supported yet", false;
                if (!rect || rect->isNull()) {
                    bool isPolar = false;
                    if (!projectionOf(keyword(call, "projection"), keyword(call, "polar"), "plt.axes", &isPolar, error)) return false;
                    gcf();
                    current_ = figure_->addSubplot(1, 1, 0, 0, isPolar);
                    return nameResult(call, current_, error);
                }
                gcf();
                return addAxes(call, rect, error);
            }
            if (call.method == "polar") {
                // pyplot.polar(*args, **kwargs): plot on the current Axes when the figure has one (polar or not, as
                // matplotlib still allows), else on axes(projection='polar')
                Figure& f = gcf();
                if (f.axes.empty()) current_ = f.addSubplot(1, 1, 0, 0, true);
                model::Call c = call;
                c.on = "plt";
                c.method = "plot";
                if (!positionalArguments(c, error)) return false;
                return axesCall(gca(), c, error);
            }
            // the pyplot functions of the current Axes and of the current figure
            const auto& ofAxes = pyplotOfAxes();
            static const std::map<std::string, std::string> ofFigure = {
                {"suptitle", "suptitle"}, {"figtext", "text"}, {"subplots_adjust", "subplots_adjust"}, {"tight_layout", "tight_layout"},
                {"figimage", "figimage"}, {"colorbar", "colorbar"}};
            const auto a = ofAxes.find(call.method);
            const auto f = ofFigure.find(call.method);
            if (a != ofAxes.end() || f != ofFigure.end()) {
                if ((call.method == "xlim" || call.method == "ylim") && call.args.empty() && call.kwargs.empty())
                    return *error = "plt." + call.method + "() without arguments (the limits as they are) is not supported yet", false;
                model::Call c = call;
                c.on = "plt";
                c.method = a != ofAxes.end() ? a->second : f->second;
                if (!positionalArguments(c, error)) return false;
                if (a != ofAxes.end()) return axesCall(gca(), c, error);
                gcf();
                return figureCall(c, error);
            }
        }
        if (call.on == "colors") return colorsCall(call, error);
        if (call.on == "mtri") return triCall(call, error);
        if (contourSets_.count(call.on)) {
            const std::string where = call.on + "." + call.method;
            if (call.method != "clabel") return *error = "unsupported call: " + where, false;
            if (call.args.size() > 1) return *error = where + ": clabel() takes from 1 to 2 positional arguments but " + std::to_string(call.args.size() + 1) + " were given", false;
            return clabel(call.on, argument(call, 0, "levels"), call, where, error);
        }
        const auto ax = axes_.find(call.on);
        if (ax != axes_.end()) return axesCall(*ax->second, call, error);
        if (figureNames_.count(call.on)) return figureCall(call, error);
        const auto dot = call.on.rfind('.');
        if (dot != std::string::npos) {  // ax.xaxis, ax.yaxis
            const auto owner = axes_.find(call.on.substr(0, dot));
            const std::string attribute = call.on.substr(dot + 1);
            if (owner != axes_.end() && (attribute == "xaxis" || attribute == "yaxis"))
                return axisCall(attribute == "xaxis" ? *owner->second->xaxis : *owner->second->yaxis, call, error);
        }
        *error = "unsupported call: " + (call.on.empty() ? "plt" : call.on) + "." + call.method;
        return false;
    }

    // The pyplot functions of the current Axes, with the Axes' methods they call
    static const std::map<std::string, std::string>& pyplotOfAxes() {
        static const std::map<std::string, std::string> ofAxes = {
            {"plot", "plot"}, {"scatter", "scatter"}, {"fill_between", "fill_between"}, {"fill_betweenx", "fill_betweenx"}, {"errorbar", "errorbar"},
            {"axhline", "axhline"}, {"axvline", "axvline"}, {"axhspan", "axhspan"}, {"axvspan", "axvspan"},
            {"hlines", "hlines"}, {"vlines", "vlines"}, {"grid", "grid"}, {"legend", "legend"}, {"text", "text"},
            {"title", "set_title"}, {"xlabel", "set_xlabel"}, {"ylabel", "set_ylabel"}, {"xlim", "set_xlim"},
            {"ylim", "set_ylim"}, {"xscale", "set_xscale"}, {"yscale", "set_yscale"}, {"bar", "bar"}, {"barh", "barh"},
            {"hist", "hist"}, {"fill", "fill"}, {"step", "step"}, {"stairs", "stairs"}, {"stem", "stem"}, {"boxplot", "boxplot"}, {"violinplot", "violinplot"}, {"pie", "pie"}, {"annotate", "annotate"}, {"bar_label", "bar_label"}, {"loglog", "loglog"}, {"semilogx", "semilogx"}, {"semilogy", "semilogy"}, {"imshow", "imshow"}, {"matshow", "matshow"}, {"spy", "spy"}, {"pcolormesh", "pcolormesh"}, {"pcolor", "pcolor"}, {"contour", "contour"}, {"contourf", "contourf"}, {"clabel", "clabel"}, {"hexbin", "hexbin"}, {"quiver", "quiver"}, {"quiverkey", "quiverkey"}, {"barbs", "barbs"}, {"streamplot", "streamplot"}, {"triplot", "triplot"}, {"tripcolor", "tripcolor"}, {"tricontour", "tricontour"}, {"tricontourf", "tricontourf"}, {"stackplot", "stackplot"}, {"broken_barh", "broken_barh"}, {"axline", "axline"}, {"eventplot", "eventplot"}, {"arrow", "arrow"}, {"table", "table"}, {"ecdf", "ecdf"}, {"xcorr", "xcorr"}, {"acorr", "acorr"}};
        return ofAxes;
    }

    // The data of a call ported with units, as matplotlib passes them through the axes' units
    // (_process_unit_info, or update_units in plot's _plot_args), in its order: each a list of parts
    // taken together (on the x axis or not). nullopt for a call not ported with units.
    struct UnitData {
        bool onX;
        std::vector<const Json*> parts;
    };
    std::optional<std::vector<UnitData>> unitData(const model::Call& call) {
        const std::string& m = call.method;
        std::vector<UnitData> out;
        const auto one = [&](bool onX, std::size_t i, const char* name) {
            if (const Json* v = argument(call, i, name)) out.push_back({onX, {v}});
        };
        if (m == "plot" || m == "step") {  // (x, y, fmt) groups
            std::size_t i = 0;
            while (i < call.args.size()) {  // as plot() reads them
                std::vector<const Json*> group;
                for (std::size_t k = i; k < std::min(i + 2, call.args.size()); ++k) group.push_back(&call.args[k]);
                i += group.size();
                if (i < call.args.size() && call.args[i].isString()) group.push_back(&call.args[i++]);
                if (group.size() > 1 && group.back()->isString()) group.pop_back();
                if (group.size() == 2) out.push_back({true, {group[0]}});
                out.push_back({false, {group.back()}});
            }
        } else if (m == "scatter") {
            one(true, 0, "x");
            one(false, 1, "y");
        } else if (m == "fill_between" || m == "fill_betweenx") {
            const bool x = m == "fill_between";
            one(x, 0, x ? "x" : "y");
            one(!x, 1, x ? "y1" : "x1");
            one(!x, 2, x ? "y2" : "x2");
        } else if (m == "axvline" || m == "axhline") {
            one(m == "axvline", 0, m == "axvline" ? "x" : "y");
        } else if (m == "axvspan" || m == "axhspan") {  // [xmin, xmax] together
            const bool x = m == "axvspan";
            UnitData d{x, {}};
            for (std::size_t i = 0; i < 2; ++i)
                if (const Json* v = argument(call, i, x ? (i ? "xmax" : "xmin") : (i ? "ymax" : "ymin"))) d.parts.push_back(v);
            out.push_back(d);
        } else if (m == "vlines" || m == "hlines") {
            const bool x = m == "vlines";
            one(x, 0, x ? "x" : "y");
            one(!x, 1, x ? "ymin" : "xmin");
            one(!x, 2, x ? "ymax" : "xmax");
        } else if (m == "set_xlim" || m == "set_ylim") {  // (v0, v1) together
            const bool x = m == "set_xlim";
            UnitData d{x, {}};
            if (call.args.size() == 1 && sequence(call.args[0])) {
                for (const Json& v : sequence(call.args[0])->items()) d.parts.push_back(&v);
            } else {
                for (std::size_t i = 0; i < 2; ++i)
                    if (const Json* v = argument(call, i, x ? (i ? "right" : "left") : (i ? "top" : "bottom"))) d.parts.push_back(v);
            }
            out.push_back(d);
        } else {
            return std::nullopt;
        }
        return out;
    }

    // Whether the call takes the dates it holds: only in its data, and not mixed with numbers in one list
    // (matplotlib picks the converter by the first finite value, then converts or breaks by the
    // others' types: not ported)
    bool datesTaken(const model::Call& call, std::string* error) {
        std::size_t all = 0;
        for (const Json& a : call.args) all += datetimes(a);
        for (const auto& kw : call.kwargs) all += datetimes(kw.second);
        if (all == 0) return true;
        const std::string where = call.on + "." + call.method;
        const auto data = unitData(call);
        if (!data) return *error = where + ": dates are not supported yet", false;
        std::size_t inData = 0;
        for (const UnitData& d : *data) {
            std::size_t n = 0, values = 0;
            for (const Json* part : d.parts) {
                n += datetimes(*part);
                const Json* seq = sequence(*part);
                if (!seq) values += !part->isNull();
                else
                    for (const Json& item : seq->items()) values += !item.isNull();
            }
            if (n > 0 && n != values) return *error = where + ": a list that mixes dates with numbers is not supported yet", false;
            inData += n;
        }
        if (inData != all) return *error = where + ": dates are not supported yet outside the data", false;
        return true;
    }

    // Axis.update_units of the call's data that are dates: the axis takes the DateConverter (and the
    // first time its tickers), as matplotlib does before the data are added
    void markDateUnits(Axes& ax, const model::Call& call) {
        const auto data = unitData(call);
        if (!data) return;
        for (const UnitData& d : *data)
            for (const Json* part : d.parts)
                if (datetimes(*part) > 0) (d.onX ? *ax.xaxis : *ax.yaxis).setDateUnits();
    }

    // Axis.set_major_formatter / set_minor_formatter, with NullFormatter() so far. The formatter is the
    // one of the Ticker the Axes sharing this axis share.
    bool axisCall(Axis& axis, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.method == "set_tick_params") return tickParams(nullptr, &axis, call, error);
        // tick_top / tick_bottom (x), tick_left / tick_right (y); set_ticks_position, set_label_position,
        // set_offset_position (y) of one position
        const bool sideCall = call.method == "tick_top" || call.method == "tick_bottom" || call.method == "tick_left" || call.method == "tick_right";
        if (sideCall || call.method == "set_ticks_position" || call.method == "set_label_position" || call.method == "set_offset_position") {
            const std::string side = sideCall ? call.method.substr(5) : "";
            if (sideCall && (axis.isX() != (side == "top" || side == "bottom")))
                return *error = where + ": '" + (axis.isX() ? "XAxis" : "YAxis") + "' object has no attribute '" + call.method + "'", false;
            if (call.method == "set_offset_position" && axis.isX())
                return *error = where + ": 'XAxis' object has no attribute 'set_offset_position'", false;
            if (sideCall) {
                if (!call.args.empty() || !call.kwargs.empty()) return *error = where + ": takes no arguments", false;
                if (!axis.tickSide(side, error)) return *error = where + ": " + *error, false;
                return true;
            }
            const Json* position = call.args.empty() ? keyword(call, "position") : &call.args[0];
            if (!position || call.args.size() > 1 || call.kwargs.size() > (call.args.empty() ? 1u : 0u))
                return *error = where + ": takes one position", false;
            if (!position->isString()) return *error = where + ": " + repr(*position) + " is not a valid value for position", false;
            const std::string& pos = position->asString();
            const bool ok = call.method == "set_ticks_position" ? axis.setTicksPosition(pos, error)
                            : call.method == "set_label_position" ? axis.setLabelPosition(pos, error)
                                                                  : axis.setOffsetPosition(pos, error);
            if (!ok) return *error = where + ": " + *error, false;
            return true;
        }
        const bool major = call.method == "set_major_formatter";
        if (!major && call.method != "set_minor_formatter") return *error = "unsupported call: " + where, false;
        const Json* formatter = call.args.empty() ? keyword(call, "formatter") : &call.args[0];
        for (const auto& [name, value] : call.kwargs)
            if (name != "formatter") return *error = where + ": got an unexpected keyword argument '" + name + "'", false;
        if (!formatter)
            return *error = where + ": missing 1 required positional argument: 'formatter'", false;
        if (!(formatter->isObject() && formatter->members()[0].first == "$new" &&
              formatter->members()[0].second.asString() == "NullFormatter"))
            return *error = where + ": a formatter other than NullFormatter() is not supported yet", false;
        (major ? axis.tickerAxis().nullMajorFormatter : axis.tickerAxis().nullMinorFormatter) = true;
        return true;
    }

    // Axes.tick_params(axis='both', **kwargs) (`ax`) or Axis.set_tick_params(which='major', reset=False,
    // **kwargs) (`one`): the keywords translated as _translate_tick_params translates them; for the x axis
    // of an Axes left, right, labelleft and labelright are dropped (top, bottom... for the y axis)
    bool tickParams(Axes* ax, Axis* one, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (!call.args.empty()) return *error = where + ": takes its parameters as keywords", false;
        std::string axisName = "both", which = "major";
        bool reset = false;
        std::vector<std::pair<std::string, const Json*>> kwargs;
        for (const auto& [name, value] : call.kwargs) {
            if (ax && name == "axis") {
                if (!value.isString() || (value.asString() != "x" && value.asString() != "y" && value.asString() != "both"))
                    return *error = where + ": axis must be 'x', 'y' or 'both'", false;
                axisName = value.asString();
            } else if (name == "which") {
                if (!value.isString() || (value.asString() != "major" && value.asString() != "minor" && value.asString() != "both"))
                    return *error = where + ": " + repr(value) + " is not a valid value for which; supported values are 'major', 'minor', 'both'", false;
                which = value.asString();
            } else if (name == "reset") {
                if (!value.isBool()) return *error = where + ": reset must be True or False", false;
                reset = value.asBool();
            } else {
                kwargs.emplace_back(name, &value);
            }
        }
        const auto translate = [&](bool isX, bool dropOther, TickParams* p) {
            static const char* const yKeys[] = {"left", "right", "labelleft", "labelright"};
            static const char* const xKeys[] = {"top", "bottom", "labeltop", "labelbottom"};
            std::vector<std::pair<std::string, const Json*>> kw;
            for (const auto& entry : kwargs) {
                const auto& dropped = isX ? yKeys : xKeys;
                if (dropOther && std::find_if(std::begin(dropped), std::end(dropped), [&](const char* k) { return entry.first == k; }) != std::end(dropped))
                    continue;
                kw.push_back(entry);
            }
            // keymap: the later of two keys for one parameter wins (bottom over left, ...)
            static const std::pair<const char*, const char*> keymap[] = {
                {"length", "size"}, {"direction", "tickdir"}, {"rotation", "labelrotation"}, {"rotation_mode", "labelrotation_mode"},
                {"left", "tick1On"}, {"bottom", "tick1On"}, {"right", "tick2On"}, {"top", "tick2On"},
                {"labelleft", "label1On"}, {"labelbottom", "label1On"}, {"labelright", "label2On"}, {"labeltop", "label2On"}};
            std::vector<std::pair<std::string, const Json*>> trans;
            const auto put = [&](const std::string& key, const Json* v) {
                for (auto& t : trans)
                    if (t.first == key) return void(t.second = v);
                trans.emplace_back(key, v);
            };
            for (const auto& [oldKey, newKey] : keymap)
                for (const auto& entry : kw)
                    if (entry.first == oldKey) put(newKey, entry.second);
            for (const auto& entry : kw)
                if (entry.first == "colors") put("color", entry.second), put("labelcolor", entry.second);
            static const char* const allowed[] = {"size", "width", "color", "tickdir", "pad", "labelsize", "labelcolor", "labelfontfamily",
                                                  "zorder", "gridOn", "tick1On", "tick2On", "label1On", "label2On", "labelrotation",
                                                  "labelrotation_mode", "grid_color", "grid_c", "grid_linestyle", "grid_ls",
                                                  "grid_linewidth", "grid_lw", "grid_alpha"};
            for (const auto& entry : kw) {
                const std::string& k = entry.first;
                if (k == "colors" || std::any_of(std::begin(keymap), std::end(keymap), [&](const auto& m) { return k == m.first; })) continue;
                if (std::find_if(std::begin(allowed), std::end(allowed), [&](const char* a) { return k == a; }) == std::end(allowed))
                    return *error = where + ": keyword " + k + " is not recognized (or not supported yet)", false;
                put(k, entry.second);
            }
            for (const auto& entry : trans) {
                const std::string& key = entry.first;
                const Json& v = *entry.second;
                double n = 0;
                const auto number = [&](std::optional<double>* out) {
                    if (!readFinite(v, &n)) return *error = where + ": " + key + " must be a finite number", false;
                    return *out = n, true;
                };
                const auto flag = [&](std::optional<bool>* out) {
                    if (!v.isBool()) return *error = where + ": " + key + " must be True or False", false;
                    return *out = v.asBool(), true;
                };
                const auto colour = [&](std::optional<colors::Spec>* out) {
                    colors::Spec c;
                    if (!color(v, &c)) return *error = where + ": " + key + " is not a colour", false;
                    return *out = c, true;
                };
                bool ok = true;
                if (key == "size") ok = number(&p->size);
                else if (key == "width") ok = number(&p->width);
                else if (key == "pad") ok = number(&p->pad);
                else if (key == "zorder") ok = number(&p->zorder);
                else if (key == "tick1On") ok = flag(&p->tick1On);
                else if (key == "tick2On") ok = flag(&p->tick2On);
                else if (key == "label1On") ok = flag(&p->label1On);
                else if (key == "label2On") ok = flag(&p->label2On);
                else if (key == "gridOn") ok = flag(&p->gridOn);
                else if (key == "color") ok = colour(&p->color);
                else if (key == "labelcolor") ok = colour(&p->labelcolor);
                else if (key == "grid_color" || key == "grid_c") ok = colour(&p->gridColor);
                else if (key == "grid_linewidth" || key == "grid_lw") ok = number(&p->gridLinewidth);
                else if (key == "grid_alpha") {
                    if (!readAlpha(v, &n)) return *error = where + ": grid_alpha must be a number from 0 to 1", false;
                    p->gridAlpha = n;
                } else if (key == "grid_linestyle" || key == "grid_ls") {
                    Line2D probe(rc_, {}, {});
                    if (!v.isString() || !probe.setLinestyle(v.asString(), error)) return *error = where + ": bad grid_linestyle", false;
                    p->gridLinestyle = v.asString();
                } else if (key == "tickdir") {
                    if (!v.isString() || (v.asString() != "in" && v.asString() != "out" && v.asString() != "inout"))
                        return *error = where + ": " + repr(v) + " is not a valid value for tickdir; supported values are 'in', 'out', 'inout'", false;
                    p->tickdir = v.asString();
                } else if (key == "labelsize") {
                    if (readNumber(v, &n)) {
                        p->labelsize = n < 1.0 ? 1.0 : n;  // (FontProperties.set_size)
                    } else if (v.isString()) {
                        const auto parsed = support::pythonFloat(v.asString());
                        const auto size = parsed ? parsed : text::fontSize(v.asString(), rc_);
                        if (!size) return *error = where + ": labelsize is not a font size: " + v.asString(), false;
                        p->labelsize = *size < 1.0 ? 1.0 : *size;
                    } else {
                        return *error = where + ": labelsize must be a number or the name of a size", false;
                    }
                } else if (key == "labelrotation") {
                    // _set_labelrotation: a number ('default'); a mode or (mode, angle) only as 'default'
                    if (readNumber(v, &n)) {
                        p->labelrotation = n;
                    } else if (v.isString() && v.asString() == "default") {
                        p->labelrotation = 0.0;
                    } else {
                        return *error = where + ": labelrotation other than a number is not supported yet", false;
                    }
                } else if (key == "labelrotation_mode") {
                    if (v.isNull()) p->labelrotationMode = std::string("default");
                    else if (v.isString() && (v.asString() == "default" || v.asString() == "anchor" || v.asString() == "xtick" || v.asString() == "ytick"))
                        p->labelrotationMode = v.asString();
                    else return *error = where + ": labelrotation_mode must be 'default', 'anchor', 'xtick', 'ytick' or None", false;
                } else if (key == "labelfontfamily") {
                    if (v.isString()) p->labelfontfamily = std::vector<std::string>{v.asString()};
                    else return *error = where + ": labelfontfamily must be a name", false;
                }
                if (!ok) return false;
            }
            return true;
        };
        for (int k = 0; k < 2; ++k) {
            const bool isX = k == 0;
            Axis* target = one ? (k == 0 ? one : nullptr) : (isX ? (axisName != "y" ? ax->xaxis.get() : nullptr) : (axisName != "x" ? ax->yaxis.get() : nullptr));
            if (!target) continue;
            TickParams p;
            if (!translate(isX, ax != nullptr, &p)) return false;  // (one Axis: nothing dropped)
            target->setTickParams(which, reset, p);
        }
        return true;
    }

    Figure* currentFigure() { return figure_.get(); }

private:
    // Python's rules for the arguments given by position: a keyword cannot name one of them again,
    // and there cannot be more than the call has parameters for. (The parameters are listed as far
    // as the calls here take them by position; most < 0: the call counts them itself.)
    bool positionalArguments(const model::Call& call, std::string* error) {
        struct Signature {
            const char* method;
            std::vector<const char*> names;
            int most;
        };
        static const Signature signatures[] = {
            {"subplots", {"nrows", "ncols"}, 2},
            {"suptitle", {"t"}, 1},
            {"text", {"x", "y", "s", "fontdict"}, 4},
            {"subplots_adjust", {"left", "bottom", "right", "top", "wspace", "hspace"}, 6},
            {"scatter", {"x", "y", "s", "c"}, -1},
            {"errorbar", {"x", "y", "yerr", "xerr", "fmt"}, -1},
            {"fill_between", {"x", "y1", "y2"}, -1},
            {"fill_betweenx", {"y", "x1", "x2"}, -1},
            {"axhline", {"y", "xmin", "xmax"}, 3},
            {"axvline", {"x", "ymin", "ymax"}, 3},
            {"axhspan", {"ymin", "ymax", "xmin", "xmax"}, 4},
            {"axvspan", {"xmin", "xmax", "ymin", "ymax"}, 4},
            {"vlines", {"x", "ymin", "ymax", "colors", "linestyles", "label"}, 6},
            {"hlines", {"y", "xmin", "xmax", "colors", "linestyles", "label"}, 6},
            {"grid", {"visible"}, -1},
            {"set_title", {"label"}, -1},
            {"set_xlabel", {"xlabel"}, -1},
            {"set_ylabel", {"ylabel"}, -1},
            {"set_xlim", {"left", "right"}, 2},
            {"set_ylim", {"bottom", "top"}, 2},
            {"bar", {"x", "height", "width", "bottom"}, 4},
            {"barh", {"y", "width", "height", "left"}, 4},
            {"hist", {"x", "bins", "range", "density", "weights", "cumulative", "bottom", "histtype", "align", "orientation",
                      "rwidth", "log", "color", "label", "stacked"}, 15},
            {"set_major_formatter", {"formatter"}, 1},
            {"set_minor_formatter", {"formatter"}, 1},
            {"set_xticks", {"ticks", "labels"}, 2},
            {"set_yticks", {"ticks", "labels"}, 2},
            {"set_frame_on", {"b"}, 1},
            {"Polygon", {"xy"}, 1},
            {"annotate", {"text", "xy", "xytext", "xycoords", "textcoords", "arrowprops", "annotation_clip"}, 7},
            {"bar_label", {"container", "labels"}, 2},
            {"stairs", {"values", "edges"}, 2},
            {"add_patch", {"p"}, 1},
            {"imshow", {"X", "cmap", "norm"}, 3},
        };
        const std::string where = (call.on.empty() ? "plt" : call.on) + "." + call.method;
        for (const Signature& signature : signatures) {
            if (call.method != signature.method) continue;
            if (signature.most >= 0 && call.args.size() > std::size_t(signature.most))
                return *error = where + ": too many positional arguments", false;
            for (std::size_t i = 0; i < call.args.size() && i < signature.names.size(); ++i)
                if (keyword(call, signature.names[i]))
                    return *error = where + ": got multiple values for argument '" + signature.names[i] + "'", false;
        }
        return true;
    }

    // cbook.normalize_kwargs: a property given by two of its names (one an alias of the other) is refused.
    using Aliases = std::vector<std::pair<const char*, const char*>>;
    static const Aliases& lineAliases() {
        static const Aliases aliases = {
            {"aa", "antialiased"}, {"c", "color"}, {"ds", "drawstyle"}, {"ls", "linestyle"}, {"lw", "linewidth"},
            {"mec", "markeredgecolor"}, {"mew", "markeredgewidth"}, {"mfc", "markerfacecolor"},
            {"mfcalt", "markerfacecoloralt"}, {"ms", "markersize"}};
        return aliases;
    }
    static const Aliases& collectionAliases() {
        static const Aliases aliases = {
            {"aa", "antialiased"}, {"antialiaseds", "antialiased"}, {"ec", "edgecolor"}, {"edgecolors", "edgecolor"},
            {"fc", "facecolor"}, {"facecolors", "facecolor"}, {"dashes", "linestyle"}, {"linestyles", "linestyle"},
            {"ls", "linestyle"}, {"linewidths", "linewidth"}, {"lw", "linewidth"}};
        return aliases;
    }
    static std::string fullName(const std::string& name, const Aliases& aliases) {
        for (const auto& [alias, property] : aliases)
            if (name == alias) return property;
        return name;
    }
    bool givenOnce(const model::Call& call, const Aliases& aliases, const std::string& where, std::string* error,
                   const std::function<bool(const std::string&, const Json&)>& skipped = nullptr) {
        std::vector<std::string> seen;
        for (const auto& [name, value] : call.kwargs) {
            if (skipped && skipped(name, value)) continue;
            const std::string property = fullName(name, aliases);
            if (std::find(seen.begin(), seen.end(), property) != seen.end())
                return *error = where + ": " + name + " is given twice (it is an alias of " + property + ")", false;
            seen.push_back(property);
        }
        return true;
    }

    // The parameters of Line2D itself, for which None is the default (the other keywords go to their setters).
    static bool isLineParameter(const std::string& name) {
        static const char* const parameters[] = {
            "linewidth", "linestyle", "color", "gapcolor", "marker", "markersize", "markeredgewidth", "markeredgecolor",
            "markerfacecolor", "markerfacecoloralt", "fillstyle", "antialiased", "dash_capstyle", "solid_capstyle",
            "dash_joinstyle", "solid_joinstyle", "pickradius", "drawstyle", "markevery"};
        return std::find_if(std::begin(parameters), std::end(parameters), [&](const char* p) { return name == p; }) != std::end(parameters);
    }

    // The keywords in the order a constructor takes them: its own parameters first (set while it
    // builds), then the others in the order given (Artist._internal_update, each through its setter).
    static std::vector<std::pair<std::string, const Json*>> ownFirst(const model::Call& call, std::initializer_list<const char*> own) {
        std::vector<std::pair<std::string, const Json*>> first, rest;
        for (const auto& [name, value] : call.kwargs) {
            bool isOwn = false;
            for (const char* o : own) isOwn = isOwn || name == o;
            (isOwn ? first : rest).emplace_back(name, &value);
        }
        first.insert(first.end(), rest.begin(), rest.end());
        return first;
    }

    // A keyword of plt.figure (also taken by plt.subplots); false with *error if it is not one or is invalid.
    bool figureKeyword(const std::string& name, const Json& value, FigureOptions& options, const std::string& where,
                       std::string* error) {
        if (name == "figsize") {
            const Json* s = sequence(value);
            std::array<double, 2> size{};
            if (!s || s->size() != 2 || !readNumber((*s)[0], &size[0]) || !readNumber((*s)[1], &size[1])) {
                *error = where + ": figsize must be (width, height) in inches";
                return false;
            }
            // "figure size must be positive finite" (zero is taken: such a figure has no pixels)
            if (!std::isfinite(size[0]) || !std::isfinite(size[1]) || size[0] < 0 || size[1] < 0)
                return *error = where + ": figsize must be finite and not negative", false;
            options.figsize = size;
        } else if (name == "dpi") {
            double dpi = 0;
            if (!readNumber(value, &dpi)) {
                *error = where + ": dpi must be a number";
                return false;
            }
            // "dpi must be positive"; and what is not finite breaks when it is drawn
            if (!(dpi > 0) || !std::isfinite(dpi)) return *error = where + ": dpi must be positive and finite", false;
            options.dpi = dpi;
        } else if (name == "facecolor" || name == "edgecolor") {
            if (value.isNull()) return true;  // the rcParams'
            colors::Spec spec;
            if (!color(value, &spec)) return *error = where + ": bad " + name, false;
            (name == "facecolor" ? options.facecolor : options.edgecolor) = spec;
        } else if (name == "frameon") {
            if (!value.isBool()) {
                *error = where + ": frameon must be True or False";
                return false;
            }
            options.frameon = value.asBool();
        } else if (name == "layout" || name == "constrained_layout" || name == "tight_layout") {
            (name == "layout" ? layoutKeywords_.layout : name == "tight_layout" ? layoutKeywords_.tight : layoutKeywords_.constrained) = &value;
        } else {
            *error = where + ": unsupported keyword " + name;
            return false;
        }
        return true;
    }

    // Figure.__init__'s layout, constrained_layout and tight_layout keywords (read after the others)
    struct LayoutKeywords {
        const Json* layout = nullptr;
        const Json* constrained = nullptr;
        const Json* tight = nullptr;
    };
    LayoutKeywords layoutKeywords_;

    bool figureLayout(const std::string& where, std::string* error) {
        const LayoutKeywords k = layoutKeywords_;
        layoutKeywords_ = LayoutKeywords();
        const auto given = [](const Json* v) { return v && !v->isNull(); };
        const auto dictItems = [](const Json& v) {
            std::vector<std::pair<std::string, const Json*>> items;
            for (const auto& [key, value] : v.members()[0].second.members()) items.emplace_back(key, &value);
            return items;
        };
        const auto isDict = [](const Json& v) { return v.isObject() && v.members().size() == 1 && v.members()[0].first == "$dict"; };
        if (given(k.layout)) return setLayoutEngine(k.layout, {}, false, where, error);
        if (given(k.tight)) {  // (tight_layout=False too sets the tight engine)
            if (!setLayoutEngine(nullptr, {}, false, where, error, "tight")) return false;
            return !isDict(*k.tight) || setLayoutEngine(nullptr, dictItems(*k.tight), true, where, error, "tight");
        }
        if (given(k.constrained)) {
            if (isDict(*k.constrained))
                return setLayoutEngine(nullptr, {}, false, where, error, "constrained") &&
                       setLayoutEngine(nullptr, dictItems(*k.constrained), true, where, error, "constrained");
            if (!k.constrained->isBool()) return *error = where + ": constrained_layout must be True, False or a dict", false;
            return !k.constrained->asBool() || setLayoutEngine(nullptr, {}, false, where, error, "constrained");
        }
        return setLayoutEngine(nullptr, {}, false, where, error);
    }

    // Figure.set_layout_engine(layout, **kwargs) (`set`: get_layout_engine().set(**kwargs) on the engine there is,
    // whose kind is `name`); `name` stands for a layout given as text
    bool setLayoutEngine(const Json* layout, const std::vector<std::pair<std::string, const Json*>>& kwargs, bool set,
                         const std::string& where, std::string* error, const std::string& name = std::string()) {
        Figure& fig = gcf();
        std::string kind = name;
        if (kind.empty()) {
            if (!layout || layout->isNull()) {
                if (rc_.boolean("figure.autolayout")) kind = "tight";
                else if (rc_.boolean("figure.constrained_layout.use")) kind = "constrained";
                else return fig.layoutEngine.reset(), true;
            } else if (!layout->isString()) {
                return *error = where + ": a layout engine object is not supported yet", false;
            } else {
                kind = layout->asString();
            }
        }
        LayoutEngine e;
        if (set) e = *fig.layoutEngine;
        const auto number = [&](const std::string& key, const Json& v, double* out) {
            if (!readNumber(v, out) || !std::isfinite(*out)) return *error = where + ": " + key + " must be a finite number", false;
            return true;
        };
        const auto rect = [&](const Json& v, std::array<double, 4>* out) {
            const Json* seq = sequence(v);
            if (!seq || seq->size() != 4) return *error = where + ": rect must be four numbers", false;
            for (std::size_t i = 0; i < 4; ++i)
                if (!number("rect", (*seq)[i], &(*out)[i])) return false;
            return true;
        };
        if (kind == "tight") {
            if (!set) e = LayoutEngine{LayoutEngine::Kind::Tight, {}, {}, true}, e.tight.rect = std::array<double, 4>{0, 0, 1, 1};
            for (const auto& [key, value] : kwargs) {
                if (value->isNull()) continue;  // (None leaves the setting as it is)
                double n = 0;
                if (key == "pad") {
                    if (!number(key, *value, &n)) return false;
                    e.tight.pad = n;
                } else if (key == "h_pad" || key == "w_pad") {
                    if (!number(key, *value, &n)) return false;
                    (key == "h_pad" ? e.tight.hPad : e.tight.wPad) = n;
                } else if (key == "rect") {
                    std::array<double, 4> r{};
                    if (!rect(*value, &r)) return false;
                    e.tight.rect = r;
                } else {
                    return *error = where + ": got an unexpected keyword argument '" + key + "'", false;
                }
            }
        } else if (kind == "constrained" || kind == "compressed") {
            if (!set) {
                e = LayoutEngine{LayoutEngine::Kind::Constrained, {}, {}, false};
                e.constrained.wPad = rc_.number("figure.constrained_layout.w_pad");
                e.constrained.hPad = rc_.number("figure.constrained_layout.h_pad");
                e.constrained.wspace = rc_.number("figure.constrained_layout.wspace");
                e.constrained.hspace = rc_.number("figure.constrained_layout.hspace");
                e.constrained.compress = kind == "compressed";
            }
            for (const auto& [key, value] : kwargs) {
                if (key == "compress" && !set && kind == "constrained") {
                    if (!value->isBool()) return *error = where + ": compress must be True or False", false;
                    e.constrained.compress = value->asBool();
                    continue;
                }
                if (value->isNull()) continue;
                double n = 0;
                if (key == "h_pad" || key == "w_pad" || key == "hspace" || key == "wspace") {
                    if (!number(key, *value, &n)) return false;
                    (key == "h_pad" ? e.constrained.hPad : key == "w_pad" ? e.constrained.wPad : key == "hspace" ? e.constrained.hspace
                                                                                                       : e.constrained.wspace) = n;
                } else if (key == "rect") {
                    if (!rect(*value, &e.constrained.rect)) return false;
                } else {
                    return *error = where + ": got an unexpected keyword argument '" + key + "'", false;
                }
            }
        } else if (kind == "none") {
            if (!fig.layoutEngine) return true;
            e = LayoutEngine{LayoutEngine::Kind::Placeholder, {}, {}, fig.layoutEngine->adjustCompatible};
        } else {
            return *error = where + ": Invalid value for 'layout': " + repr(*layout), false;
        }
        fig.layoutEngine = e;
        return true;
    }

    // plt.figure(figsize=, dpi=, facecolor=, edgecolor=, frameon=)
    bool figure(const model::Call& call, std::string* error) {
        if (!call.args.empty()) {
            *error = "plt.figure: positional arguments are not supported";
            return false;
        }
        FigureOptions options;
        layoutKeywords_ = LayoutKeywords();
        for (const auto& [name, value] : call.kwargs)
            if (!figureKeyword(name, value, options, "plt.figure", error)) return false;
        figure_ = std::make_unique<Figure>(rc_, options);
        if (!figure_->subplotpars.valid(error)) return *error = "plt.figure: " + *error, false;
        if (!figureLayout("plt.figure", error)) return false;
        axes_.clear();
        figureNames_.clear();
        current_ = nullptr;
        if (call.as.size() == 1) figureNames_.insert(call.as[0]);
        return true;
    }

    // plt.subplots(nrows=1, ncols=1, **fig_kw) -> fig, ax (one Axes; grids of Axes come later)
    // Figure._process_projection_requirements(projection=, polar=): a PolarAxes or a rectilinear Axes; matplotlib's
    // errors for an unknown projection or two that disagree
    bool projectionOf(const Json* projection, const Json* polarFlag, const std::string& where, bool* isPolar, std::string* error) {
        *isPolar = false;
        if (polarFlag && !polarFlag->isNull()) {
            if (!polarFlag->isBool()) return *error = where + ": polar must be True or False", false;
            *isPolar = polarFlag->asBool();
        }
        const bool named = projection && !projection->isNull();
        if (named && !projection->isString()) return *error = where + ": projection must be a name", false;
        if (*isPolar && named && projection->asString() != "polar")
            return *error = where + ": polar=True, yet projection=" + repr(*projection) + ". Only one of these arguments should be supplied.", false;
        if (!named || *isPolar) return true;
        const std::string name = projection->asString();
        if (name == "polar") return *isPolar = true, true;
        if (name == "rectilinear") return true;
        if (name == "aitoff" || name == "hammer" || name == "lambert" || name == "mollweide" || name == "3d")
            return *error = where + ": the projection '" + name + "' is not supported yet", false;
        return *error = where + ": Unknown projection " + repr(*projection), false;
    }

    bool subplots(const model::Call& call, std::string* error) {
        double grid[2] = {1, 1};
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (!readNumber(call.args[i], &grid[i]) || call.args[i].numberText().find_first_of(".eE") != std::string::npos) {
                *error = "plt.subplots: nrows and ncols must be integers";
                return false;
            }
        FigureOptions options;
        bool squeeze = true, polarSubplots = false;
        std::string sharex = "none", sharey = "none";
        layoutKeywords_ = LayoutKeywords();
        GridSpec ratios(1, 1);  // (width_ratios and height_ratios: the grid's)
        const Json* ratioValues[2] = {nullptr, nullptr};
        for (const auto& [name, value] : call.kwargs) {
            if (name == "nrows" || name == "ncols") {
                // "Number of rows must be a positive integer": an int, not a float that is whole
                if (!value.isNumber() || !value.isInteger() || !readNumber(value, &grid[name == "nrows" ? 0 : 1])) {
                    *error = "plt.subplots: " + name + " must be an integer";
                    return false;
                }
            } else if (name == "squeeze") {
                if (!value.isBool()) return *error = "plt.subplots: squeeze must be True or False", false;
                squeeze = value.asBool();
            } else if (name == "sharex" || name == "sharey") {
                // True and False mean 'all' and 'none'
                std::string share;
                if (value.isBool()) share = value.asBool() ? "all" : "none";
                else if (value.isString()) share = value.asString();
                if (share != "all" && share != "row" && share != "col" && share != "none")
                    return *error = "plt.subplots: " + name + " must be True, False, 'all', 'row', 'col' or 'none'", false;
                (name == "sharex" ? sharex : sharey) = share;
            } else if (name == "width_ratios" || name == "height_ratios") {
                if (!value.isNull()) ratioValues[name == "width_ratios" ? 0 : 1] = &value;
            } else if (name == "subplot_kw") {
                // the keywords each add_subplot takes: so far its projection
                if (value.isNull()) continue;
                if (!(value.isObject() && value.members().size() == 1 && value.members()[0].first == "$dict"))
                    return *error = "plt.subplots: subplot_kw must be a dict", false;
                const Json *projection = nullptr, *polarFlag = nullptr;
                for (const auto& [key, item] : value.members()[0].second.members()) {
                    if (key == "projection") projection = &item;
                    else if (key == "polar") polarFlag = &item;
                    else return *error = "plt.subplots: subplot_kw " + key + " is not supported yet", false;
                }
                if (!projectionOf(projection, polarFlag, "plt.subplots", &polarSubplots, error)) return false;
            } else if (!figureKeyword(name, value, options, "plt.subplots", error)) {
                return false;
            }
        }
        const int nrows = int(grid[0]), ncols = int(grid[1]);
        if (nrows < 1 || ncols < 1) return *error = "plt.subplots: nrows and ncols must be positive", false;
        // GridSpec.set_width_ratios / set_height_ratios: one positive number for each column (row)
        for (int k = 0; k < 2; ++k) {
            if (!ratioValues[k]) continue;
            const Json* seq = sequence(*ratioValues[k]);
            const int n = k == 0 ? ncols : nrows;
            if (!seq || int(seq->size()) != n)
                return *error = std::string("plt.subplots: Expected the given number of ") + (k == 0 ? "width" : "height") +
                                " ratios to match the number of " + (k == 0 ? "columns" : "rows") + " of the grid",
                       false;
            auto& values = k == 0 ? ratios.widthRatios : ratios.heightRatios;
            auto& ints = k == 0 ? ratios.widthRatiosInt : ratios.heightRatiosInt;
            for (const Json& r : seq->items()) {
                double v = 0;
                if (!readNumber(r, &v) || !std::isfinite(v) || !(v > 0))
                    return *error = std::string("plt.subplots: ") + (k == 0 ? "width" : "height") + " ratios must be positive numbers", false;
                values.push_back(v);
                ints.push_back(r.isNumber() && r.isInteger());
            }
        }
        figure_ = std::make_unique<Figure>(rc_, options);
        if (!figure_->subplotpars.valid(error)) return *error = "plt.subplots: " + *error, false;
        if (!figureLayout("plt.subplots", error)) return false;
        const std::vector<Axes*> made = figure_->subplots(nrows, ncols, sharex, sharey, &ratios, polarSubplots);
        axes_.clear();
        figureNames_.clear();
        mappables_.clear();
        currentImages_.clear();
        current_ = made.back();  // (each add_subplot makes its Axes the current one)
        if (call.as.empty()) return true;
        if (call.as.size() != 2) return *error = "plt.subplots returns (fig, axs): name both", false;
        figureNames_.insert(call.as[0]);
        // the names the returned array answers to: squeezed to a single Axes or a 1-d array when it can be
        const std::string& n = call.as[1];
        axesArrays_.clear();
        axesArrays_[n] = made;
        for (int r = 0; r < nrows; ++r)
            for (int c = 0; c < ncols; ++c) {
                Axes* ax = made[std::size_t(r * ncols + c)];
                if (squeeze && nrows == 1 && ncols == 1) axes_[n] = ax;
                else if (squeeze && (nrows == 1 || ncols == 1)) axes_[n + "[" + std::to_string(nrows == 1 ? c : r) + "]"] = ax;
                else axes_[n + "[" + std::to_string(r) + ", " + std::to_string(c) + "]"] = ax;
            }
        return true;
    }

    // plt.gcf(): the figure, made with the defaults when there is none
    Figure& gcf() {
        if (!figure_) {
            figure_ = std::make_unique<Figure>(rc_, FigureOptions{});
            axes_.clear();
            figureNames_.clear();
            current_ = nullptr;
        }
        return *figure_;
    }

    // Figure.gca(): the current Axes, else a new add_subplot()
    Axes& gca() {
        Figure& f = gcf();
        if (!current_) current_ = f.addSubplot(1, 1, 0, 0);
        return *current_;
    }

    bool noArguments(const model::Call& call, std::string* error) {
        if (call.args.empty() && call.kwargs.empty()) return true;
        return *error = (call.on.empty() ? "plt" : call.on) + "." + call.method + " takes no arguments", false;
    }

    // What a call that returns an Axes (or, with none, the figure) is named in the figure file
    bool nameResult(const model::Call& call, Axes* ax, std::string* error) {
        if (call.as.empty()) return true;
        if (call.as.size() != 1)
            return *error = (call.on.empty() ? "plt" : call.on) + "." + call.method + " returns one object: name it once", false;
        if (ax) axes_[call.as[0]] = ax;
        else figureNames_.insert(call.as[0]);
        return true;
    }

    // Line2D.set_markevery(every): kept as given; what _mark_every_path makes of it (and raises) when the
    // line is drawn
    bool markevery(const Json& v, std::optional<Line2D::Markevery>* out, const std::string& where, std::string* error) {
        out->reset();
        if (v.isNull()) return true;
        Line2D::Markevery every;
        every.repr = repr(v);
        const auto integral = [](const Json& j) { return j.isBool() || (j.isNumber() && j.isInteger()); };
        const auto integer = [](const Json& j) { return j.isBool() ? (long long)j.asBool() : (long long)j.asNumber(); };
        const auto real = [&](const Json& j) { return integral(j) || j.isNumber() || (j.isObject() && j.members()[0].first == "$float"); };
        const auto realValue = [&](const Json& j) {
            if (j.isObject()) return std::strtod(j.members()[0].second.asString().c_str(), nullptr);
            return j.isBool() ? double(j.asBool()) : j.asNumber();
        };
        const bool tuple = v.isObject() && v.members()[0].first == "$tuple";
        if (integral(v) || (real(v) && !tuple)) {  // N is (0, N); a float f is (0.0, f)
            if (integral(v)) every.kind = Line2D::Markevery::Kind::Slice, every.start = 0, every.step = integer(v);
            else every.kind = Line2D::Markevery::Kind::Along, every.startAlong = 0, every.stepAlong = realValue(v);
        } else if (tuple) {
            const Json& items = v.members()[0].second;
            if (items.size() != 2) {
                every.error = "`markevery` is a tuple but its len is not 2; markevery=" + every.repr;
            } else if (integral(items[1])) {
                if (!integral(items[0]))
                    every.error = "`markevery` is a tuple with len 2 and second element is an int, but the first element is not an int; markevery=" + every.repr;
                else
                    every.kind = Line2D::Markevery::Kind::Slice, every.start = integer(items[0]), every.step = integer(items[1]);
            } else if (real(items[1])) {
                if (!real(items[0]))
                    every.error = "`markevery` is a tuple with len 2 and second element is a float, but the first element is not a float or an int; markevery=" + every.repr;
                else
                    every.kind = Line2D::Markevery::Kind::Along, every.startAlong = realValue(items[0]), every.stepAlong = realValue(items[1]);
            } else {
                every.error = "markevery=" + every.repr + " is a tuple with len 2, but its second element is not an int or a float";
            }
        } else if (v.isArray() || v.isString()) {  // a fancy index: integers, or booleans as long as the points
            const std::string bad = "markevery=" + every.repr + " is iterable but not a valid numpy fancy index";
            bool allBool = v.isArray(), allIntegral = v.isArray();
            if (v.isArray())
                for (const Json& j : v.items()) allBool = allBool && j.isBool(), allIntegral = allIntegral && integral(j);
            if (v.isArray() && v.size() > 0 && allBool) {
                every.kind = Line2D::Markevery::Kind::Mask;
                for (const Json& j : v.items()) every.mask.push_back(j.asBool());
            } else if (v.isArray() && allIntegral) {  // ([] too: an empty index)
                every.kind = Line2D::Markevery::Kind::Indices;
                for (const Json& j : v.items()) every.indices.push_back(integer(j));
            } else {
                every.error = bad;
            }
        } else {
            return *error = where + ": markevery must be a number, a tuple or a list", false;
        }
        *out = every;
        return true;
    }

    // A value as Python writes it, for messages
    static std::string repr(const Json& v) {
        if (v.isBool()) return v.asBool() ? "True" : "False";
        if (v.isNumber()) return v.isInteger() ? v.numberText() : support::pythonRepr(v.asNumber());
        if (v.isString()) return svg::pythonStrRepr(v.asString());
        if (v.isNull()) return "None";
        if (v.isObject() && v.members()[0].first == "$float") return v.members()[0].second.asString();
        const Json* seq = sequence(v);
        if (!seq) return "?";
        const bool tuple = v.isObject();
        std::string out = tuple ? "(" : "[";
        for (std::size_t i = 0; i < seq->size(); ++i) out += (i ? ", " : "") + repr((*seq)[i]);
        return out + (tuple && seq->size() == 1 ? ",)" : tuple ? ")" : "]");
    }

    // SubplotSpec._from_subplot_args: the grid's rows and columns, and the first and last cells
    // (from 0) of subplot(*args) and add_subplot(*args); no arguments are (1, 1, 1)
    bool subplotSpecOf(const model::Call& call, int out[4], std::string* error) {
        const std::string where = (call.on.empty() ? "plt" : call.on) + "." + call.method + ": ";
        const auto integer = [](const Json& v) { return v.isNumber() && v.isInteger() && !v.isBool(); };
        Json rows = Json::integer(1), cols = Json::integer(1), num = Json::integer(1);
        if (call.args.size() == 1) {
            const Json& arg = call.args[0];
            const std::string why = "Single argument to subplot must be a three-digit integer, not " + repr(arg);
            if (!integer(arg)) return *error = where + why, false;
            const std::string digits = arg.numberText();
            if (digits.size() != 3 || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
                return *error = where + why, false;
            rows = Json::integer(digits[0] - '0'), cols = Json::integer(digits[1] - '0'), num = Json::integer(digits[2] - '0');
        } else if (call.args.size() == 3) {
            rows = call.args[0], cols = call.args[1], num = call.args[2];
        } else if (!call.args.empty()) {
            return *error = where + "subplot() takes 1 or 3 positional arguments but " + std::to_string(call.args.size()) +
                            " were given",
                   false;
        }
        // GridSpec(rows, cols)
        if (!integer(rows) || rows.asNumber() <= 0) return *error = where + "Number of rows must be a positive integer, not " + repr(rows), false;
        if (!integer(cols) || cols.asNumber() <= 0) return *error = where + "Number of columns must be a positive integer, not " + repr(cols), false;
        if (rows.asNumber() > 1000 || cols.asNumber() > 1000) return *error = where + "grids of more than 1000 rows or columns are not supported yet", false;
        const int r = int(rows.asNumber()), c = int(cols.asNumber()), size = r * c;
        int first = 0, last = 0;
        const Json* pair = num.isObject() ? sequence(num) : nullptr;
        if (pair && pair->size() == 2) {
            if (!integer((*pair)[0]) || !integer((*pair)[1]))
                return *error = where + "Subplot specifier tuple must contain integers, not " + repr(num), false;
            // gs[i - 1:j]: the slice's indices, then at least one cell
            const auto clamp = [size](double k) { return k < 0 ? std::max(k + size, 0.0) : std::min(k, double(size)); };
            const double start = clamp((*pair)[0].asNumber() - 1), stop = clamp((*pair)[1].asNumber());
            if (!(stop > start)) return *error = where + "GridSpec slice would result in no space allocated for subplot", false;
            first = int(start), last = int(stop) - 1;
        } else {
            if (!integer(num) || num.asNumber() < 1 || num.asNumber() > size)
                return *error = where + "num must be an integer with 1 <= num <= " + std::to_string(size) + ", not " + repr(num), false;
            first = last = int(num.asNumber()) - 1;
        }
        out[0] = r, out[1] = c, out[2] = first, out[3] = last;
        return true;
    }

    // Figure.add_axes(rect): (left, bottom, width, height), finite, not negative in size; the new Axes is current
    bool addAxes(const model::Call& call, const Json* rect, std::string* error) {
        const std::string where = (call.on.empty() ? "plt" : call.on) + "." + call.method + ": ";
        bool isPolar = false;
        if (call.method == "add_axes") {
            if (call.args.size() != 1) return *error = where + "a rect (left, bottom, width, height) is supported so far", false;
            for (const auto& [name, value] : call.kwargs)
                if (name != "projection" && name != "polar") return *error = where + "the keyword " + name + " is not supported yet", false;
        }
        if (!projectionOf(keyword(call, "projection"), keyword(call, "polar"), where.substr(0, where.size() - 2), &isPolar, error))
            return false;
        const Json* seq = rect ? sequence(*rect) : nullptr;
        double r[4];
        if (!seq || seq->size() != 4) return *error = where + "rect must be (left, bottom, width, height)", false;
        for (int i = 0; i < 4; ++i)
            if (!readNumber((*seq)[std::size_t(i)], &r[i])) return *error = where + "rect must be (left, bottom, width, height)", false;
        for (double v : r)
            if (!std::isfinite(v)) return *error = where + "all entries in rect must be finite not " + repr(*rect), false;
        if (r[2] < 0 || r[3] < 0) return *error = where + "Width and height specified must be non-negative", false;
        current_ = gcf().addAxes(r[0], r[1], r[2], r[3], isPolar);
        return nameResult(call, current_, error);
    }

    // plt.subplot(*args): the figure's Axes of that grid and cells, else a new one; either is current
    bool subplot(const model::Call& call, std::string* error) {
        bool isPolar = false;
        if (!keywords(call, {"projection", "polar"}, error)) return false;
        if (!projectionOf(keyword(call, "projection"), keyword(call, "polar"), "plt.subplot", &isPolar, error)) return false;
        int spec[4];
        if (!subplotSpecOf(call, spec, error)) return false;
        Figure& f = gcf();
        Axes* found = nullptr;
        for (const auto& a : f.axes)
            if (a->subplotSpec && a->subplotSpec->gridspec->nrows == spec[0] && a->subplotSpec->gridspec->ncols == spec[1] &&
                a->subplotSpec->num1 == spec[2] && a->subplotSpec->num2 == spec[3]) {
                // (the same grid object: the first of that shape, which add_subplot would take too)
                const GridSpec* first = nullptr;
                for (const auto& b : f.axes)
                    if (b->subplotSpec && b->subplotSpec->gridspec->nrows == spec[0] && b->subplotSpec->gridspec->ncols == spec[1]) {
                        first = b->subplotSpec->gridspec;
                        break;
                    }
                // (with keywords, only an Axes of the projection they ask for: _projection_init)
                if (a->subplotSpec->gridspec == first && (call.kwargs.empty() || bool(a->polar) == isPolar)) {
                    found = a.get();
                    break;
                }
            }
        current_ = found ? found : f.addSubplot(spec[0], spec[1], spec[2], spec[3], isPolar);
        return nameResult(call, current_, error);
    }

    bool figureCall(const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.method == "add_subplot") {
            bool isPolar = false;
            if (!keywords(call, {"projection", "polar"}, error)) return false;
            if (!projectionOf(keyword(call, "projection"), keyword(call, "polar"), where, &isPolar, error)) return false;
            int spec[4];
            if (!subplotSpecOf(call, spec, error)) return false;
            current_ = figure_->addSubplot(spec[0], spec[1], spec[2], spec[3], isPolar);
            return nameResult(call, current_, error);
        }
        if (call.method == "gca") return noArguments(call, error) && nameResult(call, &gca(), error);
        if (call.method == "add_axes") return addAxes(call, call.args.empty() ? nullptr : &call.args[0], error);
        if (call.method == "text") return figureText(call, error);
        if (call.method == "figimage") return figimage(call, error);
        if (call.method == "colorbar") return colorbar(call, error);
        if (call.method == "suptitle") {
            // FigureBase._suplabels with suptitle's info: (0.5, 0.98), centred, top, figure.titlesize/titleweight
            const Json* t = argument(call, 0, "t");
            if (!t || !t->isString()) return *error = where + ": the title must be a string", false;
            double x = 0.5, y = 0.98;
            for (const char* name : {"x", "y"})
                if (const Json* v = keyword(call, name); v && !v->isNull() && !readNumber(*v, name[0] == 'x' ? &x : &y))
                    return *error = where + ": " + name + " must be a number", false;
            const Json* yGiven = keyword(call, "y");
            const bool autopos = !yGiven || yGiven->isNull();  // (a layout engine places it)
            // normalize_kwargs: every alias as its property (the two together are an error); then the
            // defaults for what is not given, and Figure.text
            static const std::pair<const char*, const char*> aliases[] = {
                {"c", "color"},           {"family", "fontfamily"},   {"name", "fontname"},
                {"size", "fontsize"},     {"stretch", "fontstretch"}, {"style", "fontstyle"},
                {"variant", "fontvariant"}, {"weight", "fontweight"}, {"ha", "horizontalalignment"},
                {"va", "verticalalignment"}, {"ma", "multialignment"}};
            std::vector<std::pair<std::string, const Json*>> given, normal;
            if (!otherKeywords(call, {"t", "x", "y"}, &given, error)) return false;
            const auto has = [&](const std::string& name) {
                return std::find_if(normal.begin(), normal.end(), [&](const auto& g) { return g.first == name; }) != normal.end();
            };
            for (const auto& [name, value] : given) {
                std::string property = name;
                for (const auto& [alias, canonical] : aliases)
                    if (name == alias) property = canonical;
                if (has(property)) return *error = where + ": " + name + " is given twice (it is an alias of " + property + ")", false;
                normal.emplace_back(property, value);
            }
            const Json center = *support::parseJson("\"center\"", nullptr), top = *support::parseJson("\"top\"", nullptr),
                       horizontal = *support::parseJson("\"horizontal\"", nullptr);
            const Json& size = rc_.value("figure.titlesize");
            const Json& weight = rc_.value("figure.titleweight");
            const std::pair<const char*, const Json*> defaults[] = {{"horizontalalignment", &center},
                                                                    {"verticalalignment", &top},
                                                                    {"rotation", &horizontal},
                                                                    {"fontsize", &size},
                                                                    {"fontweight", &weight}};
            for (const auto& [name, value] : defaults)
                if (!has(name)) normal.emplace_back(name, value);
            if (figure_->suptitle) return *error = where + ": setting the suptitle again is not supported yet", false;
            auto label = std::make_unique<Text>(rc_, x, y, t->asString());
            if (!textProperties(*label, normal, TextKeywords::Construct, where, error)) return false;
            label->transform = figure_->transSubfigure;
            label->figure = figure_.get();
            figure_->suptitle = label.get();
            figure_->suptitleAutopos = autopos;
            figure_->texts.push_back(std::move(label));
            return true;
        }
        if (call.method == "subplots_adjust") {
            // subplots_adjust(left=None, bottom=None, right=None, top=None, wspace=None, hspace=None)
            const char* names[6] = {"left", "bottom", "right", "top", "wspace", "hspace"};
            if (!keywords(call, {"left", "bottom", "right", "top", "wspace", "hspace"}, error)) return false;
            // a layout engine that does not take it (the constrained one): a warning, and nothing done
            if (figure_->layoutEngine && !figure_->layoutEngine->adjustCompatible) return true;
            std::optional<double> v[6];
            for (std::size_t i = 0; i < 6; ++i) {
                const Json* a = argument(call, i, names[i]);
                if (!a || a->isNull()) continue;
                double n = 0;
                if (!readNumber(*a, &n)) return *error = where + ": " + names[i] + " must be a number", false;
                // (matplotlib takes some of them: what it then draws, or breaks on, is not ported)
                if (!std::isfinite(n)) return *error = where + ": " + names[i] + " must be finite", false;
                v[i] = n;
            }
            if (!subplotsAdjust(*figure_, {v[0], v[1], v[2], v[3], v[4], v[5]}, error)) return *error = where + ": " + *error, false;
            return true;
        }
        if (call.method == "tight_layout") {
            // tight_layout(*, pad=1.08, h_pad=None, w_pad=None, rect=None)
            if (!call.args.empty()) return *error = where + ": the arguments are keywords", false;
            if (!keywords(call, {"pad", "h_pad", "w_pad", "rect"}, error)) return false;
            TightLayoutOptions o;
            double n = 0;
            if (const Json* v = keyword(call, "pad")) {
                if (!readNumber(*v, &n)) return *error = where + ": pad must be a number", false;
                if (!std::isfinite(n)) return *error = where + ": pad must be finite", false;
                o.pad = n;
            }
            for (const char* name : {"h_pad", "w_pad"})
                if (const Json* v = keyword(call, name); v && !v->isNull()) {
                    if (!readNumber(*v, &n)) return *error = where + ": " + name + " must be a number", false;
                    if (!std::isfinite(n)) return *error = where + ": " + name + " must be finite", false;
                    (name[0] == 'h' ? o.hPad : o.wPad) = n;
                }
            if (const Json* v = keyword(call, "rect"); v && !v->isNull()) {
                const Json* r = sequence(*v);
                std::array<double, 4> rect{};
                if (!r || r->size() != 4) return *error = where + ": rect must be (left, bottom, right, top)", false;
                for (std::size_t i = 0; i < 4; ++i)
                    if (!readFinite((*r)[i], &rect[i])) return *error = where + ": rect must be (left, bottom, right, top)", false;
                o.rect = rect;
            }
            if (!tightLayout(*figure_, o, options_.locale, options_.fontDirectory, error)) return *error = where + ": " + *error, false;
            // (the TightLayoutEngine set for it, then set_layout_engine('none'): a placeholder taking subplots_adjust)
            figure_->layoutEngine = LayoutEngine{LayoutEngine::Kind::Placeholder, {}, {}, true};
            return true;
        }
        if (call.method == "set_layout_engine") {
            // set_layout_engine(layout=None, **kwargs)
            if (call.args.size() > 1) return *error = where + ": takes one positional argument", false;
            const Json* layout = argument(call, 0, "layout");
            std::vector<std::pair<std::string, const Json*>> kwargs;
            for (const auto& [name, value] : call.kwargs)
                if (name != "layout") kwargs.emplace_back(name, &value);
            if (!kwargs.empty() && !(layout && layout->isString()))
                return *error = where + ": keyword arguments go with a layout given by name", false;
            return setLayoutEngine(layout, kwargs, false, where, error);
        }
        *error = "unsupported call: " + where;
        return false;
    }

    // The keyword arguments of a call, checked against the ones it takes.
    bool keywords(const model::Call& call, std::initializer_list<const char*> allowed, std::string* error) {
        for (const auto& [name, value] : call.kwargs) {
            bool ok = false;
            for (const char* a : allowed) ok = ok || name == a;
            if (!ok) {
                *error = call.on + "." + call.method + ": unsupported keyword " + name;
                return false;
            }
        }
        return true;
    }

    const Json* keyword(const model::Call& call, const std::string& name) {
        for (const auto& [n, v] : call.kwargs)
            if (n == name) return &v;
        return nullptr;
    }

    bool text(const Json* v, std::string* out) {
        if (!v || !v->isString()) return false;
        *out = v->asString();
        return true;
    }

    bool axesCall(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (!datesTaken(call, error)) return false;
        if (ax.secondary) {
            // SecondaryAxis: its labels, tick parameters, ticks, colour and alignment
            const std::string& m = call.method;
            const bool x = ax.secondary->x;
            if ((m == "set_xticks" && !x) || (m == "set_yticks" && x))
                return *error = where + ": Cannot set " + (x ? "yticks on a secondary x-axis" : "xticks on a secondary y-axis"), false;
            if (m == "set_color") {
                const Json* c = argument(call, 0, "color");
                colors::Spec spec;
                if (!c || call.args.size() + call.kwargs.size() != 1 || !color(*c, &spec)) return *error = where + ": color must be a colour", false;
                ax.setSecondaryColor(spec);
                return true;
            }
            if (m == "set_alignment") {
                const Json* a = argument(call, 0, "align");
                if (!a || call.args.size() + call.kwargs.size() != 1 || !a->isString()) return *error = where + ": align must be a string", false;
                if (!ax.setAlignment(a->asString(), error)) return *error = where + ": " + *error, false;
                return true;
            }
            if (m == "set_xticks" || m == "set_yticks") {
                if (!axesCallOn(ax, call, error)) return false;
                ax.secondary->ticksSet = true;
                return true;
            }
            if (m != "set_xlabel" && m != "set_ylabel" && m != "tick_params")
                return *error = where + ": " + m + " on a secondary axis is not supported yet", false;
        }
        bool handled = false;
        if (!polarCall(ax, call, &handled, error)) return false;
        if (handled) return true;
        return axesCallOn(ax, call, error);
    }

    // PolarAxes' own methods (an AttributeError on another Axes); set_rlim, set_rscale and set_rticks are set_ylim,
    // set_yscale and set_yticks
    bool polarCall(Axes& ax, const model::Call& call, bool* handled, std::string* error) {
        static const char* const methods[] = {"set_theta_offset", "set_theta_zero_location", "set_theta_direction",
                                              "set_thetamin", "set_thetamax", "set_thetalim", "set_rorigin", "set_rmin",
                                              "set_rmax", "set_rlim", "set_rlabel_position", "set_rscale", "set_rticks",
                                              "set_rgrids", "set_thetagrids"};
        const std::string& m = call.method;
        if (std::find(std::begin(methods), std::end(methods), m) == std::end(methods)) return true;
        *handled = true;
        const std::string where = call.on + "." + m;
        if (!ax.polar) return *error = where + ": 'Axes' object has no attribute '" + m + "'", false;
        PolarAxesState& p = *ax.polar;
        const auto number = [&](std::size_t i, const char* name, double* out) {
            const Json* v = argument(call, i, name);
            if (!v) return *error = where + ": " + name + " is required", false;
            if (!readNumber(*v, out)) return *error = where + ": " + name + " must be a number", false;
            return true;
        };
        const auto only = [&](std::initializer_list<const char*> names) {
            if (call.args.size() > names.size()) return *error = where + ": too many positional arguments", false;
            return keywords(call, names, error);
        };
        const auto setOffset = [&](double offset) {
            auto mtx = p.offset->matrix();
            mtx.tx = offset;
            p.offset->setMatrix(mtx);
        };
        double v = 0;
        if (m == "set_theta_offset") return only({"offset"}) && number(0, "offset", &v) && (setOffset(v), true);
        if (m == "set_theta_zero_location") {
            if (!only({"loc", "offset"})) return false;
            const Json* loc = argument(call, 0, "loc");
            if (!loc) return *error = where + ": loc is required", false;
            static const std::pair<const char*, double> mapping[] = {
                {"N", M_PI * 0.5}, {"NW", M_PI * 0.75}, {"W", M_PI}, {"SW", M_PI * 1.25},
                {"S", M_PI * 1.5}, {"SE", M_PI * 1.75}, {"E", 0}, {"NE", M_PI * 0.25}};
            const double* base = nullptr;
            for (const auto& [name, angle] : mapping)
                if (loc->isString() && loc->asString() == name) base = &angle;
            if (!base) return *error = where + ": " + repr(*loc), false;  // (KeyError)
            double offset = 0;
            if (const Json* o = argument(call, 1, "offset"); o && !readNumber(*o, &offset)) return *error = where + ": offset must be a number", false;
            setOffset(*base + offset * (M_PI / 180.0));  // mapping[loc] + np.deg2rad(offset)
            return true;
        }
        if (m == "set_theta_direction") {
            if (!only({"direction"})) return false;
            const Json* d = argument(call, 0, "direction");
            if (!d) return *error = where + ": direction is required", false;
            double sign = 0;
            if (d->isString()) {
                const std::string name = d->asString();
                sign = name == "clockwise" ? -1 : (name == "counterclockwise" || name == "anticlockwise") ? 1 : 0;
            } else if (d->isBool()) {
                sign = d->asBool() ? 1 : 0;  // (True == 1)
            } else if (readNumber(*d, &v) && (v == 1 || v == -1)) {
                sign = v;
            }
            if (sign == 0)
                return *error = where + ": " + repr(*d) +
                                " is not a valid value for direction; supported values are -1, 1, 'clockwise', 'counterclockwise', 'anticlockwise'",
                       false;
            auto mtx = p.direction->matrix();
            mtx.sx = sign;
            p.direction->setMatrix(mtx);
            return true;
        }
        // set_thetamin / set_thetamax / set_rmin / set_rmax: the view limits themselves (read through viewLim)
        const auto setView = [&](std::size_t index, double value) {
            ax.unstaleViewLim();
            auto pts = ax.viewLim->points();
            pts[index] = value;
            ax.viewLim->setPoints(pts);
        };
        if (m == "set_thetamin" || m == "set_thetamax") {
            const char* name = m == "set_thetamin" ? "thetamin" : "thetamax";
            if (!only({name}) || !number(0, name, &v)) return false;
            setView(m == "set_thetamin" ? 0 : 2, v * (M_PI / 180.0));  // np.deg2rad
            return true;
        }
        if (m == "set_rmin" || m == "set_rmax") {
            const char* name = m == "set_rmin" ? "rmin" : "rmax";
            if (!only({name}) || !number(0, name, &v)) return false;
            setView(m == "set_rmin" ? 1 : 3, v);
            return true;
        }
        if (m == "set_rorigin") {
            if (!only({"rorigin"})) return false;
            const Json* r = argument(call, 0, "rorigin");
            if (!r) return *error = where + ": rorigin is required", false;
            if (r->isNull()) p.originViewLim->locked[1].reset();
            else if (readNumber(*r, &v)) p.originViewLim->locked[1] = v;
            else return *error = where + ": rorigin must be a number or None", false;
            return true;
        }
        if (m == "set_rlabel_position") {
            if (!only({"value"}) || !number(0, "value", &v)) return false;
            p.rLabelPosition->clear().translate(v * (M_PI / 180.0), 0.0);
            return true;
        }
        if (m == "set_thetalim") {
            // set_xlim(*args, **kwargs) with thetamin / thetamax in degrees as xmin / xmax; a range beyond a full circle
            // is taken back
            if (call.args.size() > 2) return *error = where + ": too many positional arguments", false;
            if (!keywords(call, {"thetamin", "thetamax"}, error)) return false;
            const auto orig = ax.xaxis->viewInterval();
            double lim[2] = {orig[0], orig[1]};
            std::vector<double> pair;
            if (call.args.size() == 1 && numbers(call.args[0], &pair) && pair.size() == 2) {
                lim[0] = pair[0], lim[1] = pair[1];
            } else {
                for (std::size_t i = 0; i < call.args.size(); ++i)
                    if (!readFinite(call.args[i], &lim[i])) return *error = where + ": the limits must be numbers", false;
            }
            for (int i = 0; i < 2; ++i)
                if (const Json* d = keyword(call, i == 0 ? "thetamin" : "thetamax")) {
                    if (call.args.size() > std::size_t(i)) return *error = where + ": the limit is given twice", false;
                    if (!readFinite(*d, &lim[i])) return *error = where + ": the limits must be numbers", false;
                    lim[i] *= M_PI / 180.0;
                }
            ax.xaxis->setLim(lim[0], lim[1], false);
            const auto now = ax.xaxis->viewInterval();
            if (std::fabs(now[1] - now[0]) > 2 * M_PI) {
                ax.xaxis->setLim(orig[0], orig[1], false);
                return *error = where + ": The angle range must be less than a full circle", false;
            }
            return true;
        }
        if (m == "set_rlim" || m == "set_rscale" || m == "set_rticks") {
            model::Call as = call;
            as.method = m == "set_rlim" ? "set_ylim" : m == "set_rscale" ? "set_yscale" : "set_yticks";
            if (m == "set_rlim") {
                // rmin and rmax: bottom and top, given once
                for (const char* alias : {"rmin", "rmax"}) {
                    const auto it = std::find_if(as.kwargs.begin(), as.kwargs.end(), [&](const auto& kw) { return kw.first == alias; });
                    if (it == as.kwargs.end()) continue;
                    const bool bottom = alias[1] == 'm' && alias[2] == 'i';
                    const bool positional = as.args.size() > (bottom ? 0u : 1u) && !as.args[bottom ? 0 : 1].isNull();
                    const bool named = keyword(call, bottom ? "bottom" : "top") && !keyword(call, bottom ? "bottom" : "top")->isNull();
                    if (positional || named)
                        return *error = where + (bottom ? ": Cannot supply both positional \"bottom\"argument and kwarg \"rmin\""
                                                        : ": Cannot supply both positional \"top\"argument and kwarg \"rmax\""),
                               false;
                    it->first = bottom ? "bottom" : "top";
                }
            }
            return axesCallOn(ax, as, error);
        }
        // set_rgrids(radii, labels=None, angle=None, fmt=None) and set_thetagrids(angles, labels=None, fmt=None)
        const bool radial = m == "set_rgrids";
        if (!(radial ? only({"radii", "labels", "angle", "fmt"}) : only({"angles", "labels", "fmt"}))) return false;
        std::vector<double> at;
        const Json* given = argument(call, 0, radial ? "radii" : "angles");
        if (!given || !numbers(*given, &at)) return *error = where + ": " + (radial ? "radii" : "angles") + " must be a list of numbers", false;
        if (!radial)
            for (double& a : at) a *= M_PI / 180.0;  // np.deg2rad
        std::optional<std::vector<std::string>> labels;
        if (const Json* l = argument(call, 1, "labels"); l && !l->isNull()) {
            const Json* seq = sequence(*l);
            if (!seq) return *error = where + ": labels must be a list of strings", false;
            labels = std::vector<std::string>{};
            for (const Json& t : seq->items()) {
                if (!t.isString()) return *error = where + ": labels must be strings (others are not supported yet)", false;
                labels->push_back(t.asString());
            }
        }
        Axis& axis = radial ? *ax.yaxis : *ax.xaxis;
        if (!axis.setTicks(at, std::nullopt, error) || (labels && !axis.setTicks(at, labels, error)))
            return *error = where + ": " + *error, false;
        if (const Json* f = argument(call, radial ? 3 : 2, "fmt"); !labels && f && !f->isNull()) {
            if (!f->isString()) return *error = where + ": fmt must be a string", false;
            Axis& owner = axis.tickerAxis();  // FormatStrFormatter(fmt)
            owner.strFormat = f->asString();
            owner.strFormatMethod = false;
            owner.nullMajorFormatter = false;
            owner.fixedLabels.reset();
            owner.dictLabels.reset();
        }
        if (radial) {
            double angle = p.rlabelPosition();  // (None: get_rlabel_position())
            if (const Json* a = argument(call, 2, "angle"); a && !a->isNull() && !readNumber(*a, &angle))
                return *error = where + ": angle must be a number", false;
            p.rLabelPosition->clear().translate(angle * (M_PI / 180.0), 0.0);
        }
        return true;
    }

    bool axesCallOn(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.method == "loglog" || call.method == "semilogx" || call.method == "semilogy") {
            // set_xscale('log') and / or set_yscale('log') with the scale's keywords, then plot with the rest
            // (dx: base, subs, nonpositive and their x forms, which LogScale refuses; dy likewise)
            static const char* const scaleKeywords[] = {"base", "subs", "nonpositive", "basex", "subsx", "nonposx",
                                                        "basey", "subsy", "nonposy"};
            const auto isScaleKeyword = [&](const std::string& k) {
                return std::find(std::begin(scaleKeywords), std::end(scaleKeywords), k) != std::end(scaleKeywords);
            };
            for (const char* scale : {"set_xscale", "set_yscale"}) {
                const bool x = scale[4] == 'x';
                if ((call.method == "semilogx" && !x) || (call.method == "semilogy" && x)) continue;
                model::Call c;
                c.on = call.on;
                c.method = scale;
                c.args = {Json::string("log")};
                for (const auto& kw : call.kwargs) {
                    const std::string& k = kw.first;
                    if (k == "base" || k == "subs" || k == "nonpositive") c.kwargs.push_back(kw);
                    else if (k == (x ? "basex" : "basey") || k == (x ? "subsx" : "subsy") || k == (x ? "nonposx" : "nonposy"))
                        return *error = where + ": LogScale.__init__() got an unexpected keyword argument '" + k + "'", false;
                }
                if (!axesCall(ax, c, error)) return false;
            }
            model::Call c = call;
            c.method = "plot";
            c.kwargs.clear();
            for (const auto& kw : call.kwargs)
                if (!isScaleKeyword(kw.first)) c.kwargs.push_back(kw);
            return axesCall(ax, c, error);
        }
        if (call.method == "set_xticks" || call.method == "set_yticks") {
            // set_ticks(ticks, labels=None, *, minor=False, **kwargs)
            const Json* ticksJson = argument(call, 0, "ticks");
            std::vector<double> ticks;
            if (!ticksJson || !numbers(*ticksJson, &ticks)) return *error = where + ": ticks must be a list of numbers", false;
            std::optional<std::vector<std::string>> labels;
            if (const Json* l = argument(call, 1, "labels"); l && !l->isNull()) {
                const Json* seq = sequence(*l);
                if (!seq) return *error = where + ": labels must be a list of strings", false;
                labels = std::vector<std::string>{};
                for (const Json& s : seq->items()) {
                    if (!s.isString()) return *error = where + ": labels must be strings (others are not supported yet)", false;
                    labels->push_back(s.asString());
                }
            }
            for (const auto& [name, value] : call.kwargs) {
                if (name == "ticks" || name == "labels") continue;
                if (name == "minor") {
                    if (!value.isBool() || value.asBool()) return *error = where + ": minor ticks are not supported yet", false;
                    continue;
                }
                if (!labels)
                    return *error = where + ": Incorrect use of keyword argument '" + name +
                                    "'. Keyword arguments other than 'minor' modify the text labels and can only be used if 'labels' are passed as well.",
                           false;
                return *error = where + ": keywords for the tick labels are not supported yet", false;
            }
            Axis& axis = call.method == "set_xticks" ? *ax.xaxis : *ax.yaxis;
            if (!axis.setTicks(ticks, labels, error)) return *error = where + ": " + *error, false;
            return true;
        }
        if (call.method == "set_frame_on") {
            const Json* b = argument(call, 0, "b");
            double n = 0;
            if (!b) return *error = where + ": b is required", false;
            if (b->isBool()) ax.frameOn = b->asBool();
            else if (b->isNull()) ax.frameOn = false;
            else if (readNumber(*b, &n)) ax.frameOn = n != 0;
            else return *error = where + ": b must be True or False", false;
            ax.updateTwinnedPatchVisibility();
            return true;
        }
        if (call.method == "indicate_inset" || call.method == "indicate_inset_zoom") {
            // indicate_inset(bounds=None, inset_ax=None, *, transform=None, facecolor='none', edgecolor='0.5',
            // alpha=0.5, zorder=None, **kwargs); indicate_inset_zoom(inset_ax, **kwargs)
            const bool zoom = call.method == "indicate_inset_zoom";
            const Json* boundsJson = zoom ? nullptr : argument(call, 0, "bounds");
            const Json* insetJson = zoom ? argument(call, 0, "inset_ax") : argument(call, 1, "inset_ax");
            if (call.args.size() > (zoom ? 1u : 2u)) return *error = where + ": too many positional arguments", false;
            std::optional<std::array<double, 4>> bounds;
            if (boundsJson && !boundsJson->isNull()) {
                const Json* seq = sequence(*boundsJson);
                std::array<double, 4> b{};
                if (!seq || seq->size() != 4) return *error = where + ": bounds must be [x0, y0, width, height]", false;
                for (std::size_t i = 0; i < 4; ++i)
                    if (!readNumber((*seq)[i], &b[i])) return *error = where + ": bounds must be [x0, y0, width, height]", false;
                bounds = b;
            }
            Axes* inset = nullptr;
            if (insetJson && !insetJson->isNull()) {
                const bool ref = insetJson->isObject() && insetJson->members()[0].first == "$ref";
                const auto it = ref ? axes_.find(insetJson->members()[0].second.asString()) : axes_.end();
                if (it == axes_.end()) return *error = where + ": inset_ax must name an Axes ({\"$ref\": name})", false;
                inset = it->second;
            }
            if (!bounds && !inset) return *error = where + ": At least one of bounds or inset_ax must be supplied", false;
            ax.applyAspect();
            if (!bounds) {
                const auto xl = inset->xaxis->viewInterval(), yl = inset->yaxis->viewInterval();
                bounds = std::array<double, 4>{xl[0], yl[0], xl[1] - xl[0], yl[1] - yl[0]};
            }
            auto indicator = std::make_unique<InsetIndicator>(rc_, ax, inset, inset ? std::nullopt : bounds);
            Rectangle& r = indicator->rectangle;
            r.x0 = (*bounds)[0], r.y0 = (*bounds)[1], r.width = (*bounds)[2], r.height = (*bounds)[3];
            r.transform = ax.transData;
            r.transformSet = true;
            static const Json none = *support::parseJson("\"none\"", nullptr), grey = *support::parseJson("\"0.5\"", nullptr),
                              half = *support::parseJson("0.5", nullptr), label = *support::parseJson("\"_indicate_inset\"", nullptr);
            std::vector<std::pair<std::string, const Json*>> kwargs{{"facecolor", &none}, {"edgecolor", &grey}, {"alpha", &half}};
            std::optional<double> z;
            for (const auto& [name, value] : call.kwargs) {
                if (name == "bounds" || name == "inset_ax") continue;
                if (name == "transform") {
                    if (!value.isNull()) return *error = where + ": a transform other than None is not supported yet", false;
                } else if (name == "zorder") {
                    double n = 0;
                    if (!value.isNull() && !readFinite(value, &n)) return *error = where + ": zorder must be a number or None", false;
                    if (!value.isNull()) z = n;
                } else {
                    bool replaced = false;
                    for (auto& k : kwargs)
                        if (k.first == name) k.second = &value, replaced = true;
                    if (!replaced) kwargs.emplace_back(name, &value);
                }
            }
            if (!patchConstructor(r, kwargs, where, error)) return false;
            if (r.label.empty()) r.label = label.asString();
            if (z) indicator->zorder = *z;
            ax.children.push_back(std::move(indicator));
            return true;
        }
        if (call.method == "inset_axes") {
            // inset_axes(bounds, *, transform=None, zorder=5, **kwargs)
            const Json* b = argument(call, 0, "bounds");
            const Json* seq = b ? sequence(*b) : nullptr;
            std::array<double, 4> bounds{};
            if (!seq || seq->size() != 4) return *error = where + ": bounds must be [x0, y0, width, height]", false;
            for (std::size_t i = 0; i < 4; ++i)
                if (!readNumber((*seq)[i], &bounds[i])) return *error = where + ": bounds must be [x0, y0, width, height]", false;
            if (call.args.size() > 1) return *error = where + ": takes 1 positional argument", false;
            double z = 5;
            for (const auto& [name, value] : call.kwargs) {
                if (name == "bounds") continue;
                if (name == "zorder") {
                    if (!readFinite(value, &z)) return *error = where + ": zorder must be a finite number", false;
                } else if (name == "transform") {
                    if (!value.isNull()) return *error = where + ": a transform other than None is not supported yet", false;
                } else {
                    return *error = where + ": " + name + " is not supported yet", false;
                }
            }
            return nameResult(call, ax.insetAxes(bounds, z), error);
        }
        if (call.method == "secondary_xaxis" || call.method == "secondary_yaxis") {
            // secondary_xaxis(location, functions=None, *, transform=None, **kwargs)
            const bool x = call.method == "secondary_xaxis";
            if (call.args.size() > 2) return *error = where + ": takes 2 positional arguments", false;
            for (const auto& [name, value] : call.kwargs) {
                if (name == "location" || name == "functions") continue;
                if (name == "transform") {
                    if (!value.isNull()) return *error = where + ": a transform other than None is not supported yet", false;
                } else {
                    return *error = where + ": " + name + " is not supported yet", false;
                }
            }
            const Json* loc = argument(call, 0, "location");
            if (!loc) return *error = where + ": missing 1 required positional argument: 'location'", false;
            const std::string one = x ? "top" : "right", two = x ? "bottom" : "left";
            double pos = 0;
            if (loc->isString() && (loc->asString() == one || loc->asString() == two)) pos = loc->asString() == one ? 1. : 0.;
            else if (loc->isBool()) return *error = where + ": a location True or False is not supported yet", false;
            else if (!readNumber(*loc, &pos))
                return *error = where + ": " + call.method + " location must be either a float or \"" + (x ? "top\"/\"bottom\"" : "left\"/\"right\""),
                       false;
            // set_functions: a tuple of two functions, or None (the identity)
            std::shared_ptr<const support::PyLambda> forward, inverse;
            const Json* fns = argument(call, 1, "functions");
            if (!fns || fns->isNull()) {
                std::string unused;
                forward = inverse = std::make_shared<const support::PyLambda>(*support::PyLambda::parse("x", &unused));
            } else {
                const bool tuple = fns->isObject() && fns->members().size() == 1 && fns->members()[0].first == "$tuple";
                const Json* pair = tuple ? sequence(*fns) : nullptr;
                const auto isFunction = [](const Json& v) { return v.isObject() && v.members().size() == 1 && v.members()[0].first == "$lambda"; };
                if (!pair || pair->size() != 2 || !isFunction((*pair)[0]) || !isFunction((*pair)[1]))
                    return *error = where + ": functions argument of secondary Axes must be a two-tuple of callable functions with the first "
                                            "function being the transform and the second being the inverse",
                           false;
                if (!functionPair(*fns, &forward, &inverse, where, error)) return false;
            }
            return nameResult(call, ax.secondaryAxis(x, loc->isString() ? loc->asString() : std::string(), pos, forward, inverse), error);
        }
        if (call.method == "twinx" || call.method == "twiny") {
            for (const auto& [name, value] : call.kwargs)
                if (!(name == "delta_zorder" && value.isNumber() && value.asNumber() == 0) && !(name == "axes_class" && value.isNull()))
                    return *error = where + ": " + name + " is not supported yet", false;
            if (!call.args.empty()) return *error = where + ": axes_class is not supported yet", false;
            Axes* t = ax.twin(call.method == "twinx", error);
            if (!t) return *error = where + ": " + *error, false;
            current_ = t;
            return nameResult(call, t, error);
        }
        if (call.method == "set_aspect") {
            // set_aspect(aspect, adjustable=None, anchor=None, share=False)
            const Json* a = argument(call, 0, "aspect");
            double value = 0;
            if (!a) return *error = where + ": aspect is required", false;
            if (ax.twinned && !(a->isString() && a->asString() == "auto"))
                return *error = where + ": aspects of twinned Axes (adjustable 'datalim') are not supported yet", false;
            if (a->isString() && a->asString() == "auto") {
                ax.aspect.reset();
            } else if (a->isString() && a->asString() == "equal") {
                ax.aspect = 1.0;
            } else if (readNumber(*a, &value)) {
                if (!(value > 0) || !std::isfinite(value)) return *error = where + ": aspect must be finite and positive ", false;
                ax.aspect = value;
            } else {
                return *error = where + ": could not convert that to float", false;
            }
            if (const Json* adj = argument(call, 1, "adjustable"); adj && !adj->isNull()) {
                if (adj->isString() && adj->asString() == "datalim") return *error = where + ": adjustable 'datalim' is not supported yet", false;
                if (!adj->isString() || adj->asString() != "box")
                    return *error = where + ": that is not a valid value for adjustable; supported values are 'box', 'datalim'", false;
            }
            if (const Json* anc = argument(call, 2, "anchor"); anc && !anc->isNull()) {
                // Bbox.coefs, or a pair
                static const std::pair<const char*, std::array<double, 2>> coefs[] = {
                    {"C", {0.5, 0.5}}, {"SW", {0, 0}}, {"S", {0.5, 0}}, {"SE", {1.0, 0}}, {"E", {1.0, 0.5}},
                    {"NE", {1.0, 1.0}}, {"N", {0.5, 1.0}}, {"NW", {0, 1.0}}, {"W", {0, 0.5}}};
                const Json* pair = sequence(*anc);
                bool found = false;
                for (const auto& [name, at] : coefs)
                    if (anc->isString() && anc->asString() == name) ax.anchor = at, found = true;
                if (!found) {
                    double px = 0, py = 0;
                    if (!pair || pair->size() != 2 || !readNumber((*pair)[0], &px) || !readNumber((*pair)[1], &py))
                        return *error = where + ": argument must be among C, SW, S, SE, E, NE, N, NW, W", false;
                    ax.anchor = {px, py};
                }
            }
            if (const Json* share = argument(call, 3, "share"); share && share->isBool() && share->asBool())
                return *error = where + ": share=True is not supported yet", false;
            return true;
        }
        if (call.method == "set_axis_off" || call.method == "set_axis_on") {
            if (!noArguments(call, error)) return false;
            ax.axisOn = call.method == "set_axis_on";
            return true;
        }
        if (call.method == "set") {
            // Artist.set: set_<name>(value) for each keyword, in the order given
            if (!call.args.empty())
                return *error = where + ": set() takes 1 positional argument but " + std::to_string(call.args.size() + 1) + " were given", false;
            static const char* const setters[] = {"title", "xlabel", "ylabel", "xlim", "ylim", "xscale", "yscale", "aspect",
                                                  "xticks", "yticks", "frame_on"};
            for (const auto& [name, value] : call.kwargs) {
                if (std::find(std::begin(setters), std::end(setters), name) == std::end(setters))
                    return *error = where + ": set(" + name + "=...) is not supported yet", false;
                model::Call c;
                c.on = call.on;
                c.method = "set_" + name;
                c.args = {value};
                if (!positionalArguments(c, error) || !axesCall(ax, c, error)) return false;
            }
            return true;
        }
        if (call.method == "set_title") {
            std::vector<std::pair<std::string, const Json*>> given;
            if (!otherKeywords(call, {"label", "loc", "pad", "y"}, &given, error)) return false;
            std::string label, loc = rc_.text("axes.titlelocation");
            if (!(call.args.size() == 1 ? text(&call.args[0], &label) : text(keyword(call, "label"), &label))) {
                *error = where + ": the title must be a string";
                return false;
            }
            if (const Json* l = keyword(call, "loc"); l && !text(l, &loc)) {
                *error = where + ": loc must be 'left', 'center' or 'right'";
                return false;
            }
            for (char& c : loc) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (loc != "left" && loc != "center" && loc != "right") {
                *error = where + ": loc must be 'left', 'center' or 'right'";
                return false;
            }
            std::optional<double> pad, y;
            double n = 0;
            if (const Json* v = keyword(call, "pad")) {
                if (!readNumber(*v, &n)) return *error = where + ": pad must be a number", false;
                pad = n;
            }
            if (const Json* v = keyword(call, "y")) {
                if (!readNumber(*v, &n)) return *error = where + ": y must be a number", false;
                y = n;
            }
            // title.update(default), then title._internal_update(kwargs)
            return textProperties(ax.setTitle(label, loc, pad, y), given, TextKeywords::Internal, where, error);
        }
        if (call.method == "set_xlabel" || call.method == "set_ylabel") {
            const bool isX = call.method == "set_xlabel";
            std::vector<std::pair<std::string, const Json*>> given;
            if (!otherKeywords(call, {isX ? "xlabel" : "ylabel", "labelpad", "loc"}, &given, error)) return false;
            std::string label;
            if (!(call.args.size() == 1 ? text(&call.args[0], &label) : text(keyword(call, isX ? "xlabel" : "ylabel"), &label))) {
                *error = where + ": the label must be a string";
                return false;
            }
            Axis& axis = isX ? *ax.xaxis : *ax.yaxis;
            if (const Json* v = keyword(call, "labelpad"); v && !v->isNull()) {
                double n = 0;
                if (!readNumber(*v, &n)) return *error = where + ": labelpad must be a number", false;
                axis.labelpad = n;
            }
            // The place along the axis and the alignment are the loc's, unless the keywords say them
            // (x or y, horizontalalignment, ha): then there must be no loc, and what the label has stays
            const Json* l = keyword(call, "loc");
            const bool placed = std::find_if(given.begin(), given.end(), [&](const auto& g) {
                                    return g.first == (isX ? "x" : "y") || g.first == "horizontalalignment" || g.first == "ha";
                                }) != given.end();
            if (placed) {
                if (l && !l->isNull())
                    return *error = where + ": loc cannot be given together with " + (isX ? "x" : "y") + ", horizontalalignment or ha", false;
                axis.label.text = label;
            } else {
                std::string loc = rc_.text(isX ? "xaxis.labellocation" : "yaxis.labellocation");
                if (l && !l->isNull() && !text(l, &loc)) return *error = where + ": bad loc", false;
                const bool okLoc = isX ? (loc == "left" || loc == "center" || loc == "right")
                                       : (loc == "bottom" || loc == "center" || loc == "top");
                if (!okLoc) return *error = where + ": bad loc " + loc, false;
                axis.setLabelText(label, loc);
            }
            // Axis.set_label_text: label.update(kwargs)
            return textProperties(axis.label, given, TextKeywords::Update, where, error);
        }
        if (call.method == "set_xlim" || call.method == "set_ylim") {
            const bool isX = call.method == "set_xlim";
            const char* lo = isX ? "left" : "bottom";
            const char* hi = isX ? "right" : "top";
            if (!keywords(call, {lo, hi}, error)) return false;
            markDateUnits(ax, call);  // (_set_lim: _process_unit_info first)
            Axis& axis = isX ? *ax.xaxis : *ax.yaxis;
            const Json* given[2] = {call.args.size() > 0 ? &call.args[0] : keyword(call, lo),
                                    call.args.size() > 1 ? &call.args[1] : keyword(call, hi)};
            if (call.args.size() == 1 && sequence(call.args[0]) && sequence(call.args[0])->size() == 2) {
                given[0] = &(*sequence(call.args[0]))[0];
                given[1] = &(*sequence(call.args[0]))[1];
            }
            double v[2] = {0, 0};
            bool missing = false;
            for (int i = 0; i < 2; ++i) {
                if (!given[i] || given[i]->isNull()) {
                    missing = true;
                    continue;
                }
                if (!readNumber(*given[i], &v[i])) return *error = where + ": limits must be numbers", false;
                if (!std::isfinite(v[i])) return *error = where + ": limits cannot be NaN or Inf", false;
                // (numpy holds a larger integer as an object, which isfinite does not take)
                if (given[i]->isNumber() && given[i]->isInteger() && !fitsIn64Bits(given[i]->numberText()))
                    return *error = where + ": an integer limit must fit in 64 bits", false;
            }
            if (missing) {  // the old limits fill in, read through viewLim (a pending autoscale runs first)
                const auto view = axis.viewInterval();
                for (int i = 0; i < 2; ++i)
                    if (!given[i] || given[i]->isNull()) v[i] = view[i];
            }
            axis.setLim(v[0], v[1], false);
            return true;
        }
        if (call.method == "text") return axesText(ax, call, error);
        if (call.method == "annotate") return annotate(ax, call, error);
        if (call.method == "bar_label") return barLabel(ax, call, error);
        if (call.method == "plot") return plot(ax, call, error);
        if (call.method == "scatter") return scatter(ax, call, error) && noteMappable(ax, call);
        if (call.method == "fill_between" || call.method == "fill_betweenx") return fillBetween(ax, call, error);
        if (call.method == "errorbar") return errorbar(ax, call, error);
        if (call.method == "legend") return legend(ax, call, error);
        if (call.method == "grid") return grid(ax, call, error);
        if (call.method == "tick_params") return tickParams(&ax, nullptr, call, error);
        if (call.method == "axhline" || call.method == "axvline") return axline(ax, call, error);
        if (call.method == "axline") return infiniteLine(ax, call, error);
        if (call.method == "axhspan" || call.method == "axvspan") return axspan(ax, call, error);
        if (call.method == "bar" || call.method == "barh") return bar(ax, call, error);
        if (call.method == "fill") return fill(ax, call, error);
        if (call.method == "step") return step(ax, call, error);
        if (call.method == "stem") return stem(ax, call, error);
        if (call.method == "boxplot") return boxplot(ax, call, error);
        if (call.method == "violinplot") return violinplot(ax, call, error);
        if (call.method == "pie") return pie(ax, call, error);
        if (call.method == "stairs") return stairs(ax, call, error);
        if (call.method == "add_patch") return addPatch(ax, call, error);
        if (call.method == "hist") return hist(ax, call, error);
        if (call.method == "imshow") return imshow(ax, call, error) && noteMappable(ax, call);
        if (call.method == "matshow") return matshow(ax, call, error) && noteMappable(ax, call);
        if (call.method == "spy") return spy(ax, call, error) && noteMappable(ax, call);
        if (call.method == "pcolormesh") return pcolormesh(ax, call, error) && noteMappable(ax, call);
        if (call.method == "pcolor") return pcolor(ax, call, error) && noteMappable(ax, call);
        if (call.method == "pcolorfast") return pcolorfast(ax, call, error) && noteMappable(ax, call);
        if (call.method == "contour" || call.method == "contourf" || call.method == "tricontour" || call.method == "tricontourf")
            return contour(ax, call, error) && noteMappable(ax, call);
        if (call.method == "hexbin") return hexbin(ax, call, error) && noteMappable(ax, call);
        if (call.method == "quiver") return quiver(ax, call, error) && noteMappable(ax, call);
        if (call.method == "quiverkey") return quiverkey(ax, call, error);
        if (call.method == "barbs") return barbs(ax, call, error) && noteMappable(ax, call);
        if (call.method == "streamplot") return streamplot(ax, call, error);
        if (call.method == "triplot") return triplot(ax, call, error);
        if (call.method == "stackplot") return stackplot(ax, call, error);
        if (call.method == "broken_barh") return brokenBarh(ax, call, error);
        if (call.method == "eventplot") return eventplot(ax, call, error);
        if (call.method == "arrow") return arrow(ax, call, error);
        if (call.method == "table") return table(ax, call, error);
        if (call.method == "ecdf") return ecdf(ax, call, error);
        if (call.method == "xcorr" || call.method == "acorr") return xcorr(ax, call, error);
        if (call.method == "tripcolor") return tripcolor(ax, call, error) && noteMappable(ax, call);
        if (call.method == "clabel") {
            const Json* cs = argument(call, 0, "CS");
            if (call.args.size() > 2) return *error = where + ": clabel() takes from 2 to 3 positional arguments but " + std::to_string(call.args.size() + 1) + " were given", false;
            if (!cs) return *error = where + ": clabel() missing 1 required positional argument: 'CS'", false;
            if (!(cs->isObject() && cs->members().size() == 1 && cs->members()[0].first == "$ref" && cs->members()[0].second.isString()))
                return *error = where + ": the contour set must be one the figure file named ({\"$ref\": name})", false;
            return clabel(cs->members()[0].second.asString(), argument(call, 1, "levels"), call, where, error);
        }
        if (call.method == "vlines" || call.method == "hlines") return lines(ax, call, error);
        if (call.method == "set_xscale" || call.method == "set_yscale") {
            const Json* nameJson = call.args.empty() ? keyword(call, "value") : &call.args[0];
            if (call.args.size() > 1 || !nameJson || !nameJson->isString()) return *error = where + ": give the scale's name", false;
            const std::string name = nameJson->asString();
            if (name != "linear" && name != "log" && name != "symlog" && name != "logit" && name != "function" && name != "functionlog")
                return *error = where + ": the " + name + " scale is not supported yet", false;
            if (ax.polar && call.method == "set_xscale" && name != "linear")  // ThetaAxis._set_scale
                return *error = where + ": The xscale cannot be set on a polar plot", false;
            AxisScaleOptions options;
            const bool function = name == "function" || name == "functionlog";
            bool functionsGiven = false;
            for (const auto& [key, value] : call.kwargs) {
                if (key == "value") continue;
                double n = 0;
                if (function && key == "functions") {
                    if (!functionPair(value, &options.forward, &options.inverse, where, error)) return false;
                    functionsGiven = true;
                } else if (name == "functionlog" && key == "base") {
                    if (!readNumber(value, &n)) return *error = where + ": base must be a number", false;
                    if (n <= 0 || n == 1) return *error = where + ": The log base cannot be <= 0 or == 1", false;
                    options.base = n;
                } else if (name == "symlog" && (key == "base" || key == "linthresh" || key == "linscale")) {
                    if (!readNumber(value, &n)) return *error = where + ": " + key + " must be a number", false;
                    (key == "base" ? options.base : key == "linthresh" ? options.linthresh : options.linscale) = n;
                } else if (name == "log" && key == "base") {
                    if (!readNumber(value, &n)) return *error = where + ": base must be a number", false;
                    if (n <= 0 || n == 1) return *error = where + ": The log base cannot be <= 0 or == 1", false;
                    options.base = n;
                } else if (name == "log" && key == "subs") {
                    std::vector<double> subs;
                    if (value.isNull()) continue;
                    if (!numbers(value, &subs) || subs.empty()) return *error = where + ": subs must be a list of numbers or None", false;
                    options.subs = subs;
                } else if (name == "log" && key == "nonpositive") {
                    if (!value.isString() || (value.asString() != "mask" && value.asString() != "clip"))
                        return *error = where + ": " + repr(value) + " is not a valid value for nonpositive; supported values are 'clip', 'mask'", false;
                    options.logClip = value.asString() == "clip";
                } else if (name == "logit" && key == "nonpositive") {
                    if (!value.isString() || (value.asString() != "mask" && value.asString() != "clip"))
                        return *error = where + ": " + repr(value) + " is not a valid value for nonpositive; supported values are 'mask', 'clip'", false;
                    options.clip = value.asString() == "clip";
                } else if (name == "logit" && key == "one_half") {
                    if (!value.isString()) return *error = where + ": one_half must be a string", false;
                    options.oneHalf = value.asString();
                } else if (name == "logit" && key == "use_overline") {
                    if (!value.isBool()) return *error = where + ": use_overline must be True or False", false;
                    options.useOverline = value.asBool();
                } else if (name == "symlog" && key == "subs") {
                    std::vector<double> subs;
                    if (value.isNull()) continue;
                    if (!numbers(value, &subs) || subs.empty()) return *error = where + ": subs must be a list of numbers or None", false;
                    options.subs = subs;
                } else {
                    return *error = where + ": the " + name + " scale's option " + key + " is not supported yet", false;
                }
            }
            if (function && !functionsGiven)
                return *error = where + ": the " + name + " scale needs functions=(forward, inverse)", false;
            // SymmetricalLogTransform's checks
            if (name == "symlog") {
                if (options.base <= 1.0) return *error = where + ": 'base' must be larger than 1", false;
                if (options.linthresh <= 0.0) return *error = where + ": 'linthresh' must be positive", false;
                if (options.linscale <= 0.0) return *error = where + ": 'linscale' must be positive", false;
            }
            ax.setScale(call.method == "set_xscale", name, options);
            return true;
        }
        *error = "unsupported call: " + where;
        return false;
    }

    // A function of a figure file ({"$lambda": ...}); its errors are Python's, raised by the parts without x,
    // so a call on one value finds them (matplotlib meets them when it first calls the function)
    bool function(const Json& v, std::shared_ptr<const support::PyLambda>* out, const std::string& where, std::string* error) {
        if (!v.isObject() || v.members().size() != 1 || v.members()[0].first != "$lambda" || !v.members()[0].second.isString())
            return *error = where + ": functions other than {\"$lambda\": ...} are not supported yet", false;
        std::string why;
        auto f = support::PyLambda::parse(v.members()[0].second.asString(), &why);
        if (!f) return *error = where + ": " + why, false;
        std::vector<double> unused;
        if (!f->apply({1.0}, &unused, &why)) return *error = where + ": " + why, false;
        *out = std::make_shared<const support::PyLambda>(std::move(*f));
        return true;
    }

    // functions=(forward, inverse): two functions (a tuple or a list), as FuncScale unpacks them
    bool functionPair(const Json& v, std::shared_ptr<const support::PyLambda>* forward, std::shared_ptr<const support::PyLambda>* inverse,
                      const std::string& where, std::string* error) {
        const Json* pair = sequence(v);
        if (!pair || pair->size() != 2)
            return *error = where + ": functions must be (forward, inverse), two functions", false;
        return function((*pair)[0], forward, where, error) && function((*pair)[1], inverse, where, error);
    }

    // A one-dimensional list of numbers (ints and floats alike).
    bool numbers(const Json& v, std::vector<double>* out) {
        const Json* s = sequence(v);
        if (!s) return false;
        out->clear();
        for (const Json& x : s->items()) {
            double d = 0;
            if (!readNumber(x, &d)) return false;
            out->push_back(d);
        }
        return true;
    }

    // A colour of a figure file: a string, or a list or tuple of three or four numbers, that
    // matplotlib's to_rgba takes; or one of `words` where the property has names of its own ('auto',
    // 'inherit'). (to_rgba also takes a nan among the numbers, and the drawing breaks on it later.)
    bool color(const Json& v, colors::Spec* out, std::initializer_list<const char*> words = {}, std::string* why = nullptr) {
        if (v.isString()) {
            *out = v.asString();
            for (const char* word : words)
                if (v.asString() == word) return true;
        } else {
            std::vector<double> t;
            if (!numbers(v, &t) || (t.size() != 3 && t.size() != 4)) return false;
            for (double x : t)
                if (!std::isfinite(x)) return false;
            *out = t;
        }
        return colors::toRgba(*out, std::nullopt, rc_, why).has_value();
    }

    // axes._base._process_plot_format: (linestyle, marker, color) of a format string; false if invalid.
    bool plotFormat(const std::string& fmt, std::optional<std::string>* linestyle, std::optional<std::string>* marker,
                    std::optional<colors::Spec>* colorOut, std::string* error) {
        if (fmt != "0" && fmt != "1" && colors::toRgba(fmt, std::nullopt, rc_, nullptr)) {
            // the colour is to_rgba(fmt), a tuple of floats: for 'none' (0, 0, 0, 0), which takes the line's
            // alpha as any tuple does (the name 'none' would ignore it); a polygon's face holds its parts as
            // floats (an SVG hatch id hashes them). A colour letter among other characters stays a name.
            const auto rgba = colors::toRgba(fmt, std::nullopt, rc_, nullptr);
            *colorOut = colors::Spec(std::vector<double>(rgba->begin(), rgba->end()));
            return true;
        }
        static const char* const kMarkers = ".,ov^<>12348spP*hH+xXDd|_";
        static const char* const kColors = "bgrcmykw";
        for (std::size_t i = 0; i < fmt.size();) {
            const char c = fmt[i];
            const std::string two = fmt.substr(i, 2);
            if (two == "--" || two == "-.") {
                if (*linestyle) return *error = "'" + fmt + "' is not a valid format string (two linestyle symbols)", false;
                *linestyle = two;
                i += 2;
            } else if (c == '-' || c == ':' || c == ' ') {
                if (*linestyle) return *error = "'" + fmt + "' is not a valid format string (two linestyle symbols)", false;
                *linestyle = std::string(1, c);
                i += 1;
            } else if (std::strchr(kMarkers, c) && c) {
                if (*marker) return *error = "'" + fmt + "' is not a valid format string (two marker symbols)", false;
                *marker = std::string(1, c);
                i += 1;
            } else if (std::strchr(kColors, c) && c) {
                if (*colorOut) return *error = "'" + fmt + "' is not a valid format string (two color symbols)", false;
                *colorOut = colors::Spec(std::string(1, c));
                i += 1;
            } else if (c == 'C') {
                std::size_t j = i + 1;
                while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) ++j;
                if (j == i + 1) return *error = "'" + fmt + "' is not a valid format string ('C' must be followed by a number)", false;
                *colorOut = colors::Spec(fmt.substr(i, j - i));
                i = j;
            } else {
                // (the whole character, of several bytes in UTF-8)
                std::size_t end = i + 1;
                while (end < fmt.size() && (static_cast<unsigned char>(fmt[end]) & 0xC0) == 0x80) ++end;
                return *error = "'" + fmt + "' is not a valid format string (unrecognized character '" + fmt.substr(i, end - i) + "')", false;
            }
        }
        if (!*linestyle && !*marker) *linestyle = rc_.text("lines.linestyle");
        if (!*linestyle) *linestyle = std::string("None");
        if (!*marker) *marker = std::string("None");
        return true;
    }

    // Axes.plot(*args, **kwargs): [x], y, [fmt] groups; one line each (1-D data).
    bool plot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".plot";
        // Line2D properties by their full names (aliases normalised)
        if (!givenOnce(call, lineAliases(), where, error)) return false;
        markDateUnits(ax, call);  // (_plot_args: update_units of each group's x and y, before the lines are added)
        std::vector<std::pair<std::string, const Json*>> props;
        for (const auto& [name, value] : call.kwargs) props.emplace_back(fullName(name, lineAliases()), &value);
        std::vector<const Json*> args;
        for (const Json& a : call.args) args.push_back(&a);
        std::size_t i = 0;
        while (i < args.size()) {
            // this = args[:2], plus a following format string
            std::vector<const Json*> group(args.begin() + long(i), args.begin() + long(std::min(i + 2, args.size())));
            i += group.size();
            if (i < args.size() && args[i]->isString()) group.push_back(args[i++]);
            std::optional<std::string> linestyle, marker;
            std::optional<colors::Spec> fmtColor;
            if (group.size() > 1 && group.back()->isString()) {
                if (!plotFormat(group.back()->asString(), &linestyle, &marker, &fmtColor, error)) return false;
                group.pop_back();
            } else if (group.size() == 3) {
                return *error = where + ": third arg must be a format string", false;
            }
            std::vector<double> x, y;
            if (group.size() == 2) {
                if (!numbers(*group[0], &x) || !numbers(*group[1], &y))
                    return *error = where + ": x and y must be lists of numbers", false;
            } else {
                if (!numbers(*group[0], &y)) return *error = where + ": y must be a list of numbers", false;
                for (std::size_t k = 0; k < y.size(); ++k) x.push_back(double(k));  // index_of
            }
            if (x.size() != y.size())
                return *error = where + ": x and y must have same first dimension", false;

            auto line = std::make_unique<Line2D>(rc_, x, y);
            // kw from the format, the keyword arguments over it, and for what neither says (None says
            // nothing) the next entry of the property cycle
            std::vector<std::string> given;
            if (linestyle) given.push_back("linestyle");
            if (marker) given.push_back("marker");
            if (fmtColor) given.push_back("color");
            for (const auto& [name, value] : props)
                if (!value->isNull()) given.push_back(name);
            if (linestyle && !line->setLinestyle(*linestyle, error)) return false;
            if (marker && !line->setMarker(*marker, error)) return false;
            if (fmtColor) line->color = *fmtColor;
            for (const auto& [name, value] : ax.lineDefaults(given))
                if (!lineProperty(*line, name, value, where, error)) return false;
            for (const auto& [name, value] : props) {
                if (value->isNull() && isLineParameter(name)) continue;  // Line2D(..., name=None): its default
                if (!lineProperty(*line, name, *value, where, error)) return false;
            }
            ax.addLine(std::move(line));
        }
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // A colour or a list of colours (to_rgba_array's inputs that are colour specifications).
    bool colorList(const Json& v, std::vector<colors::Spec>* out) {
        out->clear();
        colors::Spec one;
        if (color(v, &one)) {
            out->push_back(one);
            return true;
        }
        const Json* seq = sequence(v);
        if (!seq) return false;
        for (const Json& item : seq->items()) {
            if (!color(item, &one)) return false;
            out->push_back(one);
        }
        return true;
    }

    // Axes.scatter(x, y, s=None, c=None, *, marker=None, alpha=None, linewidths=None, edgecolors=None, ...)
    bool scatter(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".scatter";
        markDateUnits(ax, call);
        const Json* x = call.args.size() > 0 ? &call.args[0] : keyword(call, "x");
        const Json* y = call.args.size() > 1 ? &call.args[1] : keyword(call, "y");
        const Json* s = call.args.size() > 2 ? &call.args[2] : keyword(call, "s");
        const Json* c = call.args.size() > 3 ? &call.args[3] : keyword(call, "c");
        if (call.args.size() > 4) return *error = where + ": marker and the rest are keyword arguments", false;
        std::vector<double> xs, ys;
        if (!x || !y || !numbers(*x, &xs) || !numbers(*y, &ys)) return *error = where + ": x and y must be lists of numbers", false;
        if (xs.size() != ys.size()) return *error = where + ": x and y must be the same size", false;
        // s: lines.markersize ** 2 by default; one size or one per point
        std::vector<double> sizes;
        const bool classic = rc_.boolean("_internal.classic_mode");
        if (!s || s->isNull()) sizes = {classic ? 20.0 : std::pow(rc_.number("lines.markersize"), 2.0)};
        else {
            double one = 0;
            if (readNumber(*s, &one)) sizes = {one};
            else if (!numbers(*s, &sizes)) return *error = where + ": s must be a number or a list of numbers", false;
        }
        if (sizes.size() != 1 && sizes.size() != xs.size())
            return *error = where + ": s cannot be broadcast to match x and y", false;
        // _parse_scatter_color_args
        std::optional<std::vector<colors::Spec>> facecolors, edgecolors, kwcolor;
        std::optional<colors::Spec> edgeSingle;
        bool edgeFace = false, edgeNone = false;
        std::string marker;
        if (!markerFromJson(rc_.value("scatter.marker"), &marker, error)) return *error = where + ": " + *error, false;
        std::optional<double> alpha;
        std::vector<double> linewidths;
        bool linewidthsGiven = false;
        std::optional<double> zorder, vmin, vmax;
        std::string label, cmapName = rc_.text("image.cmap");
        // (edgecolors and linewidths, scatter's own, join the keywords to normalise unless they are None)
        const auto ownNone = [](const std::string& name, const Json& value) {
            return (name == "edgecolors" || name == "linewidths") && value.isNull();
        };
        if (!givenOnce(call, collectionAliases(), where, error, ownNone)) return false;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "x" || name == "y" || name == "s" || name == "c" || ownNone(name, value)) continue;
            if (name == "cmap") {
                if (!value.isNull() && !value.isString()) return *error = where + ": cmap must be a colormap name", false;
                if (value.isString()) cmapName = value.asString();
                continue;
            }
            if (name == "vmin" || name == "vmax") {
                double v = 0;
                if (value.isNull()) continue;
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "vmin" ? vmin : vmax) = v;
                continue;
            }
            if (name == "facecolors" || name == "facecolor" || name == "fc") {
                std::vector<colors::Spec> l;
                if (!colorList(value, &l)) return *error = where + ": bad facecolors", false;
                facecolors = l;
            } else if (name == "edgecolors" || name == "edgecolor" || name == "ec") {
                if (value.isString() && (value.asString() == "face")) edgeFace = true;
                else if (value.isString() && (value.asString() == "none" || value.asString() == "None")) edgeNone = true;
                else {
                    std::vector<colors::Spec> l;
                    if (!colorList(value, &l)) return *error = where + ": bad edgecolors", false;
                    edgecolors = l;
                }
            } else if (name == "color") {
                std::vector<colors::Spec> l;
                if (!colorList(value, &l)) return *error = where + ": bad color", false;
                kwcolor = l;
            } else if (name == "marker") {
                if (!markerFromJson(value, &marker, error)) return *error = where + ": " + *error, false;
            } else if (name == "alpha") {
                double a = 0;
                if (!value.isNull()) {
                    if (!readAlpha(value, &a)) return *error = where + ": alpha must be a number from 0 to 1", false;
                    alpha = a;
                }
            } else if (name == "linewidths" || name == "linewidth" || name == "lw") {
                double one = 0;
                if (readNumber(value, &one)) linewidths = {one};
                else if (!numbers(value, &linewidths)) return *error = where + ": bad linewidths", false;
                if (!finiteWidths(linewidths)) return *error = where + ": bad linewidths", false;
                linewidthsGiven = true;
            } else if (name == "zorder") {
                double z = 0;
                if (!readFinite(value, &z)) return *error = where + ": zorder must be a finite number", false;
                zorder = z;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                label = value.asString();
            } else {
                return *error = where + ": unsupported keyword " + name, false;
            }
        }
        if (kwcolor && c && !c->isNull()) return *error = where + ": supply a 'c' argument or a 'color' kwarg but not both", false;
        if (kwcolor) {
            if (!edgecolors && !edgeFace && !edgeNone) edgecolors = kwcolor;
            if (!facecolors) facecolors = kwcolor;
        }
        // (in the classic style the edges stay unset: the collection's own default, black)
        if (!edgecolors && !edgeFace && !edgeNone && !classic) {
            const std::string e = rc_.text("scatter.edgecolors");
            if (e == "face") edgeFace = true;
            else if (e == "none") edgeNone = true;
            else if (!colors::toRgba(e, std::nullopt, rc_, nullptr)) return *error = where + ": rcParams[\"scatter.edgecolors\"] is not a colour", false;
            else edgecolors = std::vector<colors::Spec>{e};
        }
        std::vector<colors::Spec> faces;
        std::optional<std::vector<double>> mappedValues;
        if (c && !c->isNull()) {
            // c looks suitable for value-mapping when it is numbers, one per point (c.size == x.size)
            std::vector<double> values;
            if (!c->isString() && numbers(*c, &values) && values.size() == xs.size()) mappedValues = values;
            else if (!colorList(*c, &faces)) return *error = where + ": 'c' must be a color or a sequence of colors", false;
        } else if (facecolors) {
            faces = *facecolors;
        } else if (classic) {
            faces = {colors::Spec(std::string("b"))};
        } else {
            faces = {ax.nextPatchColor()};
        }
        if (!mappedValues && faces.size() != 1 && faces.size() != xs.size() && !faces.empty())
            return *error = where + ": 'c' has a size inconsistent with x and y", false;
        // cbook._combine_masks(x, y, s, c, ...): a point is bad where any of them, one value per point,
        // is not finite; its offset is then NaN (the masked offsets are filled with NaN when drawn), it
        // bounds no data, and its value to map is masked (the colormap's 'bad' colour)
        const std::size_t n = xs.size();
        std::vector<bool> bad(n, false);
        for (std::size_t i = 0; i < n; ++i) {
            bad[i] = !std::isfinite(xs[i]) || !std::isfinite(ys[i]);
            if (sizes.size() == n && !std::isfinite(sizes[i])) bad[i] = true;
            if (mappedValues && !std::isfinite((*mappedValues)[i])) bad[i] = true;
        }

        MarkerStyle style;
        if (!style.set(marker, std::nullopt, rc_, error)) return false;
        // marker.get_path().transformed(marker.get_transform())
        std::vector<double> v = style.path().vertices ? *style.path().vertices : std::vector<double>{};
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            const auto p = transforms::apply(style.transform(), v[i], v[i + 1]);
            v[i] = p[0], v[i + 1] = p[1];
        }
        Path path = Path::make(std::move(v), style.path().codes ? *style.path().codes : std::vector<unsigned char>{}, rc_);
        // one colour may be 'none' (to_rgba_array gives no colours) or 'face'
        const auto argOf = [](const std::vector<colors::Spec>& l) {
            if (l.size() == 1) return ColorArg::of(l[0]);
            ColorArg a;
            a.kind = ColorArg::Colors;
            a.colors = l;
            return a;
        };
        ColorArg faceArg = mappedValues ? ColorArg{} : argOf(faces), edgeArg;
        if (edgeFace) edgeArg.kind = ColorArg::Face;
        else if (edgeNone) edgeArg.kind = ColorArg::None;
        else if (edgecolors) edgeArg = argOf(*edgecolors);
        bool mapped = mappedValues.has_value();
        if (!style.filled()) {
            if (style.fillstyle() == "none") {
                // promote the facecolor to be the edgecolor, and set the facecolor to 'none'. (The
                // values to map are then lost: matplotlib maps them only while there are no colours.)
                edgeArg = faceArg;
                faceArg = ColorArg{};
                faceArg.kind = ColorArg::None;
                mapped = false;
            } else {
                edgeArg = ColorArg{};
                edgeArg.kind = ColorArg::Face;
            }
            if (!linewidthsGiven) linewidths = {rc_.number("lines.linewidth")}, linewidthsGiven = true;
        }
        auto collection = std::make_unique<PathCollection>(rc_, std::vector<Path>{path}, sizes);
        collection->setFacecolor(faceArg);
        collection->setEdgecolor(edgeArg);
        collection->setLinewidths(linewidthsGiven ? linewidths : std::vector<double>{});
        const double nan = std::numeric_limits<double>::quiet_NaN();
        for (std::size_t i = 0; i < n; ++i) collection->offsets.push_back(bad[i] ? std::array<double, 2>{nan, nan} : std::array<double, 2>{xs[i], ys[i]});
        collection->offsetTransform = ax.transData;
        if (alpha) collection->setAlpha(alpha);
        if (mapped) {  // set_cmap, set_norm(Normalize()), set_array(c), _scale_norm
            collection->cmap = colors::Colormap::named(cmapName);
            if (!collection->cmap) return *error = where + ": '" + cmapName + "' is not a valid value for cmap", false;
            // set_array of the masked values (masked: they map to 'bad'), set_clim, then autoscale_None
            colors::MaskedArray values;
            values.data = *mappedValues;
            values.mask = bad;
            colors::NormSpec spec;
            spec.vmin = vmin;
            spec.vmax = vmax;
            if (!colors::Norm::make(spec, collection->norm.get(), error)) return *error = where + ": " + *error, false;
            collection->norm->autoscaleNone(colors::processValue(values));
            collection->array = values;
        } else if (vmin || vmax || cmapName != rc_.text("image.cmap")) {
            // matplotlib warns that these are ignored without values to map
        }
        collection->transform = std::make_shared<transforms::IdentityTransform>();
        collection->transformSet = true;
        if (zorder) collection->zorder = *zorder;
        collection->label = label;
        // Classic mode only: margins to allow for the finite size of the symbols
        if (classic && !xs.empty()) {
            if (ax.xmargin < 0.05) ax.xmargin = 0.05, ax.requestAutoscaleView(true, false);
            if (ax.ymargin < 0.05) ax.ymargin = 0.05, ax.requestAutoscaleView(false, true);
        }
        ax.addCollection(std::move(collection));
        return true;
    }

    // Axes.fill_between(x, y1, y2=0, *, where=None, interpolate=False, step=None, **kwargs)
    // (fill_betweenx: the same with y, x1, x2 and the regions along y)
    bool fillBetween(Axes& ax, const model::Call& call, std::string* error) {
        const bool alongY = call.method == "fill_betweenx";
        const std::string where = call.on + "." + call.method;
        const char* const names[3] = {alongY ? "y" : "x", alongY ? "x1" : "y1", alongY ? "x2" : "y2"};
        markDateUnits(ax, call);  // (_fill_between_process_units)
        const Json* xa = call.args.size() > 0 ? &call.args[0] : keyword(call, names[0]);
        const Json* y1a = call.args.size() > 1 ? &call.args[1] : keyword(call, names[1]);
        const Json* y2a = call.args.size() > 2 ? &call.args[2] : keyword(call, names[2]);
        if (call.args.size() > 3) return *error = where + ": too many positional arguments", false;
        std::vector<double> t, f1, f2;
        if (!xa || !numbers(*xa, &t)) return *error = where + ": " + names[0] + " must be a list of numbers", false;
        // y1 and y2: lists, or numbers broadcast over x
        const auto values = [&](const Json* j, std::vector<double>* out, double fallback) {
            double one = fallback;
            if (!j || j->isNull() || readNumber(*j, &one)) {
                out->assign(t.size(), one);
                return true;
            }
            return numbers(*j, out) && out->size() == t.size();
        };
        if (!y1a || !values(y1a, &f1, 0.0))
            return *error = where + ": " + names[1] + " must be a number or a list as long as " + names[0], false;
        if (!values(y2a, &f2, 0.0)) return *error = where + ": " + names[2] + " must be a number or a list as long as " + names[0], false;
        std::vector<bool> mask(t.size(), true);
        std::optional<std::string> hatchPattern;
        std::string stepKind;
        bool interpolate = false;
        ColorArg face, edge, hatch;
        bool faceGiven = false;
        std::optional<double> alpha, zorder;
        std::vector<double> linewidths;
        std::string label;
        // (the classic style neither normalises the keywords nor takes a colour from the property
        // cycle: the collection gets them as they come, and its own default colour)
        const bool classic = rc_.boolean("_internal.classic_mode");
        if (!classic && !givenOnce(call, collectionAliases(), where, error)) return false;
        for (const auto& [name, value] : call.kwargs) {
            if (name == names[0] || name == names[1] || name == names[2]) continue;
            if (name == "where") {
                const Json* seq = sequence(value);
                if (value.isNull()) continue;
                if (!seq || seq->size() != t.size()) return *error = where + ": where must be booleans, one per x", false;
                for (std::size_t i = 0; i < t.size(); ++i) {
                    if (!(*seq)[i].isBool()) return *error = where + ": where must be booleans", false;
                    mask[i] = (*seq)[i].asBool();
                }
            } else if (name == "hatch") {
                if (!value.isNull() && !value.isString()) return *error = where + ": hatch must be a string", false;
                if (value.isString()) hatchPattern = value.asString();
            } else if (name == "interpolate") {
                // (any truth value)
                if (!value.isNull() && !value.isBool() && !value.isNumber()) return *error = where + ": interpolate must be true or false", false;
                interpolate = value.isBool() ? value.asBool() : value.isNumber() && value.asNumber() != 0;
            } else if (name == "step") {
                // STEP_LOOKUP_MAP["steps-" + step]: 'pre', 'post' or 'mid'; None is no steps (False is
                // not: matplotlib breaks on it)
                if (!value.isNull()) {
                    if (!value.isString() || (value.asString() != "pre" && value.asString() != "post" && value.asString() != "mid"))
                        return *error = where + ": step must be 'pre', 'post' or 'mid'", false;
                    stepKind = value.asString();
                }
            } else if ((name == "color" || name == "facecolor" || name == "facecolors" || name == "fc") && value.isNull()) {
                // (given, even as None: no colour from the cycle; None is the default)
                faceGiven = true;
                face = ColorArg{};
                if (name == "color") edge = hatch = ColorArg{};
            } else if ((name == "edgecolor" || name == "edgecolors" || name == "ec") && value.isNull()) {
                edge = ColorArg{};
            } else if (name == "color" || name == "facecolor" || name == "facecolors" || name == "fc") {
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad " + name, false;
                face = ColorArg::of(c);
                faceGiven = true;
                if (name == "color") edge = hatch = ColorArg::of(c);  // set_color: face, edge and hatch
            } else if (name == "edgecolor" || name == "edgecolors" || name == "ec") {
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad " + name, false;
                edge = ColorArg::of(c);
            } else if (name == "alpha") {
                double a = 0;
                if (!value.isNull()) {
                    if (!readAlpha(value, &a)) return *error = where + ": alpha must be a number from 0 to 1", false;
                    alpha = a;
                }
            } else if (name == "linewidth" || name == "linewidths" || name == "lw") {
                double w = 0;
                if (!readFinite(value, &w)) return *error = where + ": linewidth must be a finite number", false;
                linewidths = {w};
            } else if (name == "zorder") {
                double z = 0;
                if (!readFinite(value, &z)) return *error = where + ": zorder must be a finite number", false;
                zorder = z;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                label = value.asString();
            } else {
                return *error = where + ": unsupported keyword " + name, false;
            }
        }
        if (!faceGiven && !classic) face = ColorArg::of(ax.nextPatchColor());
        // x, y1 and y2 are masked where they are not finite (np.ma.masked_invalid): the regions break there
        for (std::size_t i = 0; i < t.size(); ++i)
            mask[i] = mask[i] && std::isfinite(t[i]) && std::isfinite(f1[i]) && std::isfinite(f2[i]);
        auto collection = std::make_unique<FillBetweenPolyCollection>(rc_, t, f1, f2, mask, stepKind, alongY, interpolate);
        // the keyword arguments are set after the defaults (_internal_update)
        collection->setFacecolor(face);
        if (edge.kind != ColorArg::Default) collection->setEdgecolor(edge);
        collection->setHatchcolor(hatch);
        collection->hatch = hatchPattern;
        if (!linewidths.empty()) collection->setLinewidths(linewidths);
        if (alpha) collection->setAlpha(alpha);
        if (zorder) collection->zorder = *zorder;
        collection->label = label;
        ax.addCollection(std::move(collection));
        return true;
    }

    // An error argument: a number, a list (one per point), or two lists (below, above).
    bool errorValues(const Json& v, std::size_t n, std::vector<double>* below, std::vector<double>* above) {
        double one = 0;
        if (readNumber(v, &one)) {
            below->assign(n, one), above->assign(n, one);
            return true;
        }
        std::vector<double> list;
        if (numbers(v, &list) && list.size() == n) {
            *below = *above = list;
            return true;
        }
        const Json* seq = sequence(v);
        if (seq && seq->size() == 2 && numbers((*seq)[0], below) && numbers((*seq)[1], above) && below->size() == n &&
            above->size() == n)
            return true;
        return false;
    }

    // Axes.errorbar(x, y, yerr=None, xerr=None, fmt='', ecolor=None, elinewidth=None, capsize=None,
    // barsabove=False, capthick=None, **Line2D kwargs) without limit markers or errorevery
    bool errorbar(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".errorbar";
        const Json* xa = call.args.size() > 0 ? &call.args[0] : keyword(call, "x");
        const Json* ya = call.args.size() > 1 ? &call.args[1] : keyword(call, "y");
        const Json* yerr = call.args.size() > 2 ? &call.args[2] : keyword(call, "yerr");
        const Json* xerr = call.args.size() > 3 ? &call.args[3] : keyword(call, "xerr");
        const Json* fmtJson = call.args.size() > 4 ? &call.args[4] : keyword(call, "fmt");
        if (call.args.size() > 5) return *error = where + ": ecolor and the rest are keyword arguments", false;
        std::vector<double> x, y;
        if (!xa || !ya || !numbers(*xa, &x) || !numbers(*ya, &y)) return *error = where + ": x and y must be lists of numbers", false;
        if (x.size() != y.size()) return *error = where + ": 'x' and 'y' must have the same size", false;
        if (yerr && yerr->isNull()) yerr = nullptr;
        if (xerr && xerr->isNull()) xerr = nullptr;
        std::string fmt;
        // (fmt=None is not the default '': "third arg must be a format string")
        if (fmtJson) {
            if (!fmtJson->isString()) return *error = where + ": fmt must be a string", false;
            fmt = fmtJson->asString();
        }
        std::optional<colors::Spec> ecolor;
        std::optional<double> elinewidth, capsize, capthick;
        bool barsabove = false;
        // the Line2D keyword arguments (aliases normalised, None dropped)
        if (!givenOnce(call, lineAliases(), where, error)) return false;
        std::vector<std::pair<std::string, const Json*>> props;
        std::optional<double> zorderArg;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "x" || name == "y" || name == "yerr" || name == "xerr" || name == "fmt") continue;
            if (name == "ecolor") {
                if (value.isNull()) continue;
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad ecolor", false;
                ecolor = c;
            } else if (name == "elinewidth" || name == "capsize" || name == "capthick") {
                if (value.isNull()) continue;
                double n = 0;
                if (!readFinite(value, &n)) return *error = where + ": " + name + " must be a finite number", false;
                (name == "elinewidth" ? elinewidth : name == "capsize" ? capsize : capthick) = n;
            } else if (name == "barsabove") {
                if (!value.isBool()) return *error = where + ": barsabove must be True or False", false;
                barsabove = value.asBool();
            } else if (name == "lolims" || name == "uplims" || name == "xlolims" || name == "xuplims" ||
                       name == "errorevery" || name == "elinestyle") {
                return *error = where + ": " + name + " is not supported yet", false;
            } else {
                if (value.isNull()) continue;
                const std::string full = fullName(name, lineAliases());
                if (full == "zorder") {
                    double z = 0;
                    if (!readFinite(value, &z)) return *error = where + ": zorder must be a finite number", false;
                    zorderArg = z;
                    continue;
                }
                props.emplace_back(full, &value);
            }
        }
        const double zorder = zorderArg ? *zorderArg : 2;  // kwargs.setdefault('zorder', 2)
        const auto prop = [&](const char* name) -> const Json* {
            for (const auto& [n, v] : props)
                if (n == name) return v;
            return nullptr;
        };

        // the data line, as _plot_args((x, y[, fmt]), kwargs, return_kwargs=True)
        std::optional<std::string> linestyle, marker;
        std::optional<colors::Spec> fmtColor;
        std::string lowerFmt = fmt;
        for (char& ch : lowerFmt) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (!fmt.empty() && !plotFormat(fmt, &linestyle, &marker, &fmtColor, error)) return false;
        colors::Spec baseColor;
        bool baseColorSet = false;
        if (fmtColor) baseColor = *fmtColor, baseColorSet = true;
        if (const Json* c = prop("color")) {
            if (!color(*c, &baseColor)) return *error = where + ": bad color", false;
            baseColorSet = true;
        }
        // what the property cycle gives for what the format and the keywords do not say
        std::vector<std::string> given;
        if (linestyle) given.push_back("linestyle");
        if (marker) given.push_back("marker");
        if (fmtColor) given.push_back("color");
        for (const auto& [name, value] : props) given.push_back(name);
        const auto defaults = ax.lineDefaults(given);
        const auto fromCycle = [&](const char* name) -> const Json* {
            for (const auto& [n, v] : defaults)
                if (n == name) return &v;
            return nullptr;
        };
        // ErrorbarContainer((data_line, caplines, barcols), has_xerr=, has_yerr=, label=label)
        ErrorbarContainer container;
        container.hasXerr = xerr != nullptr;
        container.hasYerr = yerr != nullptr;
        if (const Json* l = keyword(call, "label"); l && l->isString()) container.label = l->asString();
        if (lowerFmt != "none") {
            auto line = std::make_unique<Line2D>(rc_, x, y);
            if (linestyle && !line->setLinestyle(*linestyle, error)) return false;
            if (marker && !line->setMarker(*marker, error)) return false;
            if (fmtColor) line->color = *fmtColor;
            for (const auto& [name, value] : defaults)
                if (!lineProperty(*line, name, value, where, error)) return false;
            for (const auto& [name, value] : props)
                if (!lineProperty(*line, name, *value, where, error)) return false;
            if (fromCycle("color")) baseColor = line->color, baseColorSet = true;
            line->label = "_nolegend_";
            line->zorder = barsabove ? zorder - .1 : zorder + .1;
            container.dataLine = &ax.addLine(std::move(line));
        } else {
            // no data line; base_style keeps the kwargs' colour only. (The keywords still go to the
            // lines of the bars and caps, which refuse what a line refuses.)
            Line2D probe(rc_, {}, {});
            for (const auto& [name, value] : defaults)
                if (!lineProperty(probe, name, value, where, error)) return false;
            for (const auto& [name, value] : props)
                if (!lineProperty(probe, name, *value, where, error)) return false;
            if (!prop("color")) baseColorSet = false;
        }
        if (!baseColorSet) baseColor = std::string("C0");
        const colors::Spec eColor = ecolor ? *ecolor : baseColor;
        // base_style keeps what is not about markers or line styles
        std::optional<double> linewidth, alpha;
        double n = 0;
        for (const Json* v : {fromCycle("linewidth"), prop("linewidth")})
            if (v && readNumber(*v, &n)) linewidth = n;
        for (const Json* v : {fromCycle("alpha"), prop("alpha")})
            if (v && readNumber(*v, &n)) alpha = n;
        if (!elinewidth && !rc_.value("errorbar.elinewidth").isNull()) elinewidth = rc_.number("errorbar.elinewidth");
        const double caps = capsize ? *capsize : rc_.number("errorbar.capsize");
        if (!capthick && !rc_.value("errorbar.capthick").isNull()) capthick = rc_.number("errorbar.capthick");

        const auto styleBars = [&](Collection& bars) {
            bars.setEdgecolor(ColorArg::of(eColor));  // set_color: the edge
            if (elinewidth) bars.setLinewidths({*elinewidth});
            else if (linewidth) bars.setLinewidths({*linewidth});
            if (alpha) bars.setAlpha(alpha);
            bars.zorder = zorder;
            bars.label = "_nolegend_";
        };
        const auto capLine = [&](const std::vector<double>& cx, const std::vector<double>& cy, const char* m) {
            auto line = std::make_unique<Line2D>(rc_, cx, cy);
            line->color = baseColor;
            std::string e;
            line->setLinestyle("none", &e);
            line->setMarker(m, &e);
            if (linewidth) line->setLinewidth(*linewidth);
            if (alpha) line->alpha = alpha;
            if (caps > 0) line->markersize = 2. * caps;
            if (capthick) line->markeredgewidth = *capthick;
            if (const Json* v = prop("markeredgewidth"); v && readNumber(*v, &n)) line->markeredgewidth = n;
            line->markeredgecolor = eColor;
            line->zorder = zorder;
            line->label = "_nolegend_";
            return line;
        };
        std::vector<std::unique_ptr<Line2D>> capLines;
        struct CapData {
            std::vector<double> x, y;
            bool radial;
        };
        std::vector<CapData> capData;
        // x errors (hlines, '|' caps), then y errors (vlines, '_' caps)
        for (int axisIndex = 0; axisIndex < 2; ++axisIndex) {
            const Json* err = axisIndex == 0 ? xerr : yerr;
            if (!err) continue;
            const std::vector<double>& dep = axisIndex == 0 ? x : y;
            const std::vector<double>& indep = axisIndex == 0 ? y : x;
            std::vector<double> below, above;
            if (!errorValues(*err, dep.size(), &below, &above))
                return *error = where + ": the errors must be a number, one list or two lists as long as the data", false;
            std::vector<double> low, high;
            for (std::size_t i = 0; i < dep.size(); ++i) {
                if (below[i] < -below[i] || above[i] < -above[i]) return *error = where + ": errors must not be negative", false;
                low.push_back(dep[i] + -1 * below[i]);
                high.push_back(dep[i] + 1 * above[i]);
            }
            std::vector<std::vector<std::array<double, 2>>> segments;
            for (std::size_t i = 0; i < dep.size(); ++i) {
                if (axisIndex == 0) segments.push_back({{low[i], indep[i]}, {high[i], indep[i]}});
                else segments.push_back({{indep[i], low[i]}, {indep[i], high[i]}});
            }
            auto bars = std::make_unique<LineCollection>(rc_, segments);
            styleBars(*bars);
            if (ax.polar && axisIndex == 0)  // (the theta errors of a polar Axes follow the circles)
                for (Path& path : bars->paths) path.interpolationSteps = 2;
            container.barlinecols.push_back(&ax.addLinesCollection(std::move(bars)));
            if (caps > 0) {
                for (const std::vector<double>* lh : {&low, &high}) {
                    if (axisIndex == 0) capLines.push_back(capLine(*lh, indep, "|"));
                    else capLines.push_back(capLine(indep, *lh, "_"));
                    capData.push_back({axisIndex == 0 ? *lh : indep, axisIndex == 0 ? indep : *lh, axisIndex == 1});
                }
            }
        }
        if (ax.polar) {
            // the caps turned to stand across the bars: of each point of each cap line a line of its own, its '_'
            // marker (errorbar's last `marker`) rotated by _ScaledRotation(theta, transShift), a quarter turn more
            // for the radial bars; the cap lines themselves are the container's only
            for (std::size_t i = 0; i < capLines.size(); ++i) {
                const CapData& d = capData[i];
                for (std::size_t k = 0; k < d.x.size(); ++k) {
                    transforms::Affine2D rotation;
                    rotation.rotate(ax.polar->transShift->transform(d.x[k], 0)[0]);
                    transforms::Matrix user = rotation.matrix();
                    if (d.radial) {
                        transforms::Affine2D quarter;
                        quarter.rotate(M_PI / 2);
                        user = transforms::dot(quarter.matrix(), user);
                    }
                    auto cap = capLine({d.x[k]}, {d.y[k]}, "_");
                    cap->markerStyle.setUserTransform(user);
                    ax.addLine(std::move(cap));
                }
                container.caplines.push_back(capLines[i].get());
                ax.detached.push_back(std::move(capLines[i]));
            }
            capLines.clear();
        }
        for (auto& l : capLines) container.caplines.push_back(&ax.addLine(std::move(l)));
        ax.containers.push_back(container);
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // How the keywords of a text reach it: as Text(x, y, text, **kwargs) takes them (its own
    // parameters first, where None is the default, then the others through Text.update); as
    // Text.update takes them (every one through its setter; an alias together with its property is
    // an error); or as Artist._internal_update does (every one through its setter, in order, an
    // alias and its property both: the later wins).
    enum class TextKeywords { Construct, Update, Internal };

    // The properties of a Text given as keywords, each checked as its setter checks it.
    bool textProperties(Text& t, const std::vector<std::pair<std::string, const Json*>>& given, TextKeywords mode,
                        const std::string& where, std::string* error) {
        const bool constructing = mode == TextKeywords::Construct;
        static const std::pair<const char*, const char*> aliases[] = {
            {"c", "color"},           {"family", "fontfamily"},   {"name", "fontname"},
            {"size", "fontsize"},     {"stretch", "fontstretch"}, {"style", "fontstyle"},
            {"variant", "fontvariant"}, {"weight", "fontweight"}, {"ha", "horizontalalignment"},
            {"va", "verticalalignment"}, {"ma", "multialignment"}};
        static const char* const own[] = {"color", "verticalalignment", "horizontalalignment", "multialignment",
                                          "rotation", "linespacing", "rotation_mode", "usetex", "wrap",
                                          "transform_rotates_text", "parse_math", "antialiased"};
        const auto isOwn = [&](const std::string& name) {
            return constructing && std::find_if(std::begin(own), std::end(own), [&](const char* o) { return name == o; }) != std::end(own);
        };
        const auto canonical = [&](const std::string& name) {
            for (const auto& [alias, property] : aliases)
                if (name == alias) return std::string(property);
            return name;
        };
        const auto bad = [&](const std::string& name, const std::string& what) {
            *error = where + ": " + name + " " + what;
            return false;
        };
        // One property through its setter; `own` for a parameter of Text itself that is None
        const auto set = [&](const std::string& name, const Json& v, bool isParameter) {
            const std::string property = canonical(name);
            double n = 0;
            if (property == "color") {
                if (v.isNull() && isParameter) return t.setColor(rc_.text("text.color")), true;
                colors::Spec spec;
                std::string why;
                if (!color(v, &spec, {}, &why)) return bad(name, "is not a colour" + (why.empty() ? "" : ": " + why));
                const auto rgba = colors::toRgba(spec, std::nullopt, rc_, nullptr);
                t.color = *rgba;
            } else if (property == "horizontalalignment" || property == "verticalalignment" || property == "multialignment") {
                // (Text.__init__ stores a multialignment unchecked, and the layout of several lines
                // then breaks on an unknown one: it is checked here as its setter checks it)
                if (property == "multialignment" && isParameter && v.isNull()) return t.multialignment.reset(), true;
                if (!v.isString()) return bad(name, "must be a string");
                const std::string& a = v.asString();
                const bool vertical = property == "verticalalignment";
                const bool ok = vertical ? (a == "top" || a == "bottom" || a == "center" || a == "baseline" || a == "center_baseline")
                                         : (a == "center" || a == "right" || a == "left");
                if (!ok) return bad(name, "is not an alignment: " + a);
                if (vertical) t.verticalAlignment = a;
                else if (property == "multialignment") t.multialignment = a;
                else t.horizontalAlignment = a;
            } else if (property == "rotation") {
                if (v.isNull()) t.setRotation(0.0);
                else if (readNumber(v, &n)) t.setRotation(n);
                else if (v.isString() && (v.asString() == "horizontal" || v.asString() == "vertical")) t.setRotation(v.asString());
                else return bad(name, "must be 'vertical', 'horizontal' or a number");
            } else if (property == "linespacing") {
                if (v.isNull() && isParameter) t.linespacing.reset();
                else if (v.isString() && v.asString() == "normal") t.linespacing.reset();
                else if (readNumber(v, &n)) t.linespacing = n;
                else return bad(name, "must be 'normal' or a number");
            } else if (property == "rotation_mode") {
                if (v.isNull()) return t.rotationMode = "default", true;
                if (!v.isString()) return bad(name, "must be a string");
                const std::string& m = v.asString();
                if (m != "anchor" && m != "default" && m != "xtick" && m != "ytick") return bad(name, "is not a rotation mode: " + m);
                t.rotationMode = m;
            } else if (property == "usetex" || property == "wrap" || property == "transform_rotates_text") {
                if (!v.isNull() && !(v.isBool() && !v.asBool())) return bad(name, "is not supported yet");
            } else if (property == "parse_math" || property == "antialiased") {
                const char* key = property == "parse_math" ? "text.parse_math" : "text.antialiased";
                if (!v.isBool() && !v.isNull()) return bad(name, "must be True or False");
                // None is the rcParams' value as a parameter of Text, and False to the setters
                (property == "parse_math" ? t.parseMath : t.antialiased) = v.isNull() ? isParameter && rc_.boolean(key) : v.asBool();
            } else if (property == "alpha") {
                if (v.isNull()) return t.alpha.reset(), true;
                if (!readNumber(v, &n)) return bad(name, "must be a number or None");
                if (!(0 <= n && n <= 1)) return bad(name, "is outside the 0-1 range");
                t.alpha = n;
            } else if (property == "fontsize") {
                // FontProperties.set_size
                double size = 0;
                if (v.isNull()) {
                    size = rc_.number("font.size");
                } else if (readNumber(v, &size)) {
                } else if (v.isString()) {
                    const std::string& text = v.asString();
                    const auto parsed = support::pythonFloat(text);
                    const auto named = parsed ? std::optional<double>() : text::fontSize(text, rc_);
                    if (!parsed && !named) return bad(name, "is not a font size: " + text);
                    size = parsed ? *parsed : *named;
                } else {
                    return bad(name, "must be a number or the name of a size");
                }
                if (!std::isfinite(size)) return bad(name, "is not a font size");
                // Fontsize < 1.0 pt not allowed by FreeType. Setting fontsize = 1 pt
                t.font.size = size < 1.0 ? 1.0 : size;
            } else if (property == "fontweight" || property == "fontstretch") {
                // FontProperties.set_weight / set_stretch: a name of the table, or an integer from 0 to 1000
                const bool weight = property == "fontweight";
                text::FontValue& target = weight ? t.font.weight : t.font.stretch;
                if (v.isNull()) {
                    const auto& d = rc_.value(weight ? "font.weight" : "font.stretch");
                    target = d.isNumber() ? text::FontValue(int(d.asNumber())) : text::FontValue(d.asString());
                    return true;
                }
                std::optional<double> number;
                if (v.isString()) {
                    const std::string& text = v.asString();
                    if (weight ? text::isFontWeightName(text) : text::isFontStretchName(text)) return target = text, true;
                    const auto integer = support::pythonInt(text);
                    if (!integer) return bad(name, "is invalid: " + text);
                    number = double(*integer);
                } else if (readNumber(v, &n)) {
                    if (!std::isfinite(n)) return bad(name, "is invalid");
                    number = std::trunc(n);
                } else {
                    return bad(name, "must be a name or a number");
                }
                if (!(0 <= *number && *number <= 1000)) return bad(name, "is invalid");
                target = int(*number);
            } else if (property == "fontstyle" || property == "fontvariant") {
                const bool style = property == "fontstyle";
                std::string value;
                if (v.isNull()) value = rc_.text(style ? "font.style" : "font.variant");
                else if (v.isString()) value = v.asString();
                else return bad(name, "must be a string");
                const bool ok = style ? (value == "normal" || value == "italic" || value == "oblique")
                                      : (value == "normal" || value == "small-caps");
                if (!ok) return bad(name, "is not one of the choices: " + value);
                (style ? t.font.style : t.font.variant) = value;
            } else if (property == "fontfamily" || property == "fontname") {
                if (v.isNull()) return t.font.family = rc_.strings("font.family"), true;
                if (v.isString()) return t.font.family = {v.asString()}, true;
                const Json* list = sequence(v);
                if (!list) return bad(name, "must be a name or a list of names");
                std::vector<std::string> names;
                for (const Json& item : list->items()) {
                    if (!item.isString()) return bad(name, "must be a name or a list of names");
                    names.push_back(item.asString());
                }
                t.font.family = names;
            } else if (property == "math_fontfamily") {
                // FontProperties.set_math_fontfamily
                static const char* const fontsets[] = {"dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"};
                if (v.isNull()) return t.font.mathFontfamily = rc_.text("mathtext.fontset"), true;
                if (!v.isString() || std::find_if(std::begin(fontsets), std::end(fontsets), [&](const char* f) { return v.asString() == f; }) == std::end(fontsets))
                    return bad(name, "is not a math font family");
                t.font.mathFontfamily = v.asString();
            } else if (property == "language") {
                // Text.set_language: None is the rcParams' language
                const auto& d = rc_.value("text.language");
                if (v.isNull()) return t.language = d.isString() ? std::optional<std::string>(d.asString()) : std::nullopt, true;
                if (sequence(v)) return bad(name, "as a list of ranges is not supported yet");
                if (!v.isString()) return bad(name, "must be a string or None");
                t.language = v.asString();
            } else if ((property == "x" || property == "y") && !constructing) {
                if (!readNumber(v, &n)) return bad(name, "must be a number");
                (property == "x" ? t.x : t.y) = n;
            } else if (property == "zorder") {
                if (v.isNull()) return t.zorder = 3, true;
                if (!readNumber(v, &n)) return bad(name, "must be a number");
                t.zorder = n;
            } else if (property == "clip_on" || property == "visible") {
                if (!v.isBool()) return bad(name, "must be True or False");
                (property == "clip_on" ? t.clipOn : t.visible) = v.asBool();
            } else if (property == "snap") {
                if (v.isNull()) t.snap.reset();
                else if (v.isBool()) t.snap = v.asBool();
                else return bad(name, "must be True, False or None");
            } else if (property == "bbox") {
                return textBbox(t, v, where, error);
            } else if (property == "backgroundcolor") {
                // set_backgroundcolor: a box of that colour (face and edge), or that face for the box there is
                if (!t.bboxPatch)
                    return textBbox(t, Json::object({{"$dict", Json::object({{"facecolor", v}, {"edgecolor", v}})}}), where, error);
                return patchProperty(*t.bboxPatch, "facecolor", v, where, error);
            } else {
                *error = where + ": unsupported keyword " + name;
                return false;
            }
            return true;
        };
        if (constructing)
            for (const char* parameter : own)
                for (const auto& [name, value] : given)
                    if (name == parameter && !set(name, *value, true)) return false;
        std::vector<std::string> seen;
        const Json* bbox = nullptr;
        for (const auto& [name, value] : given) {
            if (isOwn(name)) continue;
            const std::string property = canonical(name);
            if (mode != TextKeywords::Internal && std::find(seen.begin(), seen.end(), property) != seen.end()) {
                *error = where + ": " + name + " is given twice (it is an alias of " + property + ")";
                return false;
            }
            seen.push_back(property);
            if (property == "bbox") {  // (Text.update sets the box last: it depends on the font)
                bbox = value;
                continue;
            }
            if (!set(name, *value, false)) return false;
        }
        if (bbox && !set("bbox", *bbox, false)) return false;
        return true;
    }

    // Text.set_bbox(rectprops): None for no box; else a FancyBboxPatch of the box style (square, padded by 4
    // points, when none is given; a pad of 0.3 else), "%0.2f" of the pad added to a style without one
    bool textBbox(Text& t, const Json& v, const std::string& where, std::string* error) {
        if (v.isNull()) return t.bboxPatch.reset(), true;
        if (!(v.isObject() && v.members()[0].first == "$dict")) return *error = where + ": bbox must be a dict ({\"$dict\": {...}}) or None", false;
        const Json& props = v.members()[0].second;
        const Json* boxstyle = props.find("boxstyle");
        const Json* padValue = props.find("pad");
        std::optional<double> pad;
        if (padValue && !padValue->isNull()) {
            double n = 0;
            if (!readNumber(*padValue, &n)) return *error = where + ": bbox's pad must be a number", false;
            pad = n;
        }
        std::string style;
        if (!boxstyle || boxstyle->isNull()) {
            style = "square";
            pad = (pad ? *pad : 4) / t.font.size;  // to a fraction of the font size
        } else {
            if (!boxstyle->isString()) return *error = where + ": bbox's boxstyle must be a style name", false;
            style = boxstyle->asString();
            if (!pad) pad = 0.3;
        }
        if (style.find("pad") == std::string::npos) {
            char text[64];
            std::snprintf(text, sizeof text, ",pad=%0.2f", *pad);
            style += text;
        }
        auto patch = std::make_unique<FancyBboxPatch>(rc_);
        if (!BoxStyle::make(style, {}, &patch->boxStyle, error)) return *error = where + ": " + *error, false;
        patch->transform = std::make_shared<transforms::IdentityTransform>();
        patch->transformSet = true;
        std::vector<std::pair<std::string, const Json*>> kwargs;
        for (const auto& [key, value] : props.members()) {
            if (key == "boxstyle" || key == "pad") continue;
            double n = 0;
            if (key == "mutation_scale" || key == "mutation_aspect") {
                if (key == "mutation_aspect" && value.isNull()) continue;
                if (!readFinite(value, &n)) return *error = where + ": bbox's " + key + " must be a finite number", false;
                (key == "mutation_scale" ? patch->mutationScale : patch->mutationAspect) = n;
                continue;
            }
            kwargs.emplace_back(key, &value);
        }
        if (!patchConstructor(*patch, kwargs, where, error)) return false;
        t.bboxPatch = std::move(patch);
        return true;
    }

    // Axes.text(x, y, s, fontdict=None, **kwargs)
    bool axesText(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".text";
        const char* names[4] = {"x", "y", "s", "fontdict"};
        const Json* values[4] = {nullptr, nullptr, nullptr, nullptr};
        for (std::size_t i = 0; i < 4; ++i) values[i] = argument(call, i, names[i]);
        double x = 0, y = 0;
        if (!values[0] || !readNumber(*values[0], &x) || !values[1] || !readNumber(*values[1], &y))
            return *error = where + ": x and y must be numbers", false;
        if (!values[2] || !values[2]->isString()) return *error = where + ": the text must be a string", false;
        // {'verticalalignment': 'baseline', 'horizontalalignment': 'left', 'transform': transData,
        //  'clip_on': False, **kwargs}: a later one replaces an earlier one where it stands
        const Json baseline = *support::parseJson("\"baseline\"", nullptr), left = *support::parseJson("\"left\"", nullptr),
                   no = *support::parseJson("false", nullptr);
        std::vector<std::pair<std::string, const Json*>> given = {
            {"verticalalignment", &baseline}, {"horizontalalignment", &left}, {"clip_on", &no}};
        const auto put = [&](const std::string& name, const Json& value) {
            for (auto& g : given)
                if (g.first == name) return void(g.second = &value);
            given.emplace_back(name, &value);
        };
        // (a figure file has no dictionaries yet: the keywords say the same)
        if (values[3] && !values[3]->isNull()) return *error = where + ": fontdict is not supported yet", false;
        for (const auto& [name, value] : call.kwargs)
            if (name != "x" && name != "y" && name != "s" && name != "fontdict") put(name, value);
        auto t = std::make_unique<Text>(rc_, x, y, values[2]->asString());
        if (!textProperties(*t, given, TextKeywords::Construct, where, error)) return false;
        ax.addText(std::move(t));
        return true;
    }

    // A coordinate system of annotate: a name, or a pair of names
    bool annotationCoords(const Json& v, bool offset, AnnotationCoords* out, const std::string& where, std::string* error) {
        std::vector<std::string> names;
        if (v.isString()) {
            names = {v.asString()};
        } else if (const Json* pair = v.isObject() ? sequence(v) : nullptr; pair && pair->size() == 2 && (*pair)[0].isString() &&
                                                                            (*pair)[1].isString()) {
            names = {(*pair)[0].asString(), (*pair)[1].asString()};
        } else {
            return *error = where + ": coordinates must be a name or a tuple of two names (not supported yet otherwise)", false;
        }
        // (xycoords: no 'offset ...' even inside a tuple)
        for (const auto& n : names)
            if (std::string why = annotationCoordsError(n, offset); !why.empty()) return *error = where + ": " + why, false;
        out->names = names;
        return true;
    }

    // Annotation.__init__'s arrow: FancyArrowPatch((0, 0), (1, 1), **arrowprops) without relpos (with an arrow
    // style) or without width, headwidth, headlength and shrink (without one, which update_positions reads)
    bool annotationArrow(Annotation& a, const Json& props, const std::string& where, std::string* error) {
        Annotation::ArrowProps ap;
        const Json* style = props.find("arrowstyle");
        ap.hasArrowstyle = style != nullptr;
        std::array<double, 2> relpos{0.5, 0.5};
        const auto number = [&](const char* key, double* out) {
            const Json* j = props.find(key);
            if (!j) return true;
            if (!readFinite(*j, out)) return *error = where + ": arrowprops' " + key + " must be a finite number", false;
            return true;
        };
        std::vector<std::pair<std::string, const Json*>> rest;
        for (const auto& [key, value] : props.members()) {
            if (ap.hasArrowstyle && key == "relpos") {
                const Json* s = sequence(value);
                if (!s || s->size() != 2 || !readFinite((*s)[0], &relpos[0]) || !readFinite((*s)[1], &relpos[1]))
                    return *error = where + ": arrowprops' relpos must be a pair of numbers", false;
                continue;
            }
            if (!ap.hasArrowstyle && (key == "width" || key == "headwidth" || key == "headlength" || key == "shrink")) continue;
            rest.emplace_back(key, &value);
        }
        if (!ap.hasArrowstyle && (!number("width", &ap.width) || !number("headwidth", &ap.headwidth) ||
                                  !number("headlength", &ap.headlength) || !number("shrink", &ap.shrink)))
            return false;
        if (const Json* ms = props.find("mutation_scale")) {
            double m = 0;
            if (!readFinite(*ms, &m)) return *error = where + ": arrowprops' mutation_scale must be a finite number", false;
            ap.mutationScale = m;
        }
        auto arrow = std::make_unique<FancyArrowPatch>(rc_, bezier::Point{0, 0}, bezier::Point{1, 1});
        std::vector<std::pair<std::string, const Json*>> patchKwargs;
        for (const auto& [key, value] : rest) {
            const Json& j = *value;
            double n = 0;
            if (key == "arrowstyle" || key == "connectionstyle") {
                if (key == "connectionstyle" && j.isNull()) continue;  // (None: 'arc3')
                if (!j.isString()) return *error = where + ": arrowprops' " + key + " must be a style name", false;
                if (key == "arrowstyle" ? !ArrowStyle::make(j.asString(), {}, &arrow->arrowStyle, error)
                                        : !ConnectionStyle::make(j.asString(), {}, &arrow->connectionStyle, error))
                    return *error = where + ": " + *error, false;
            } else if (key == "patchA" || key == "patchB") {
                if (!j.isNull()) return *error = where + ": arrowprops' " + key + " other than None is not supported yet", false;
                if (key == "patchA") ap.patchAGiven = true;
            } else if (key == "shrinkA" || key == "shrinkB" || key == "mutation_scale" || key == "mutation_aspect") {
                if (key == "mutation_aspect" && j.isNull()) continue;  // (get_mutation_aspect: 1)
                if (!readFinite(j, &n)) return *error = where + ": arrowprops' " + key + " must be a finite number", false;
                (key == "shrinkA" ? arrow->shrinkA : key == "shrinkB" ? arrow->shrinkB : key == "mutation_scale" ? arrow->mutationScale : arrow->mutationAspect) = n;
            } else if (key == "path" || key == "posA" || key == "posB") {
                return *error = where + ": arrowprops' " + key + " is not supported", false;
            } else {
                patchKwargs.emplace_back(key, value);
            }
        }
        if (!patchConstructor(*arrow, patchKwargs, where, error)) return false;
        a.setArrow(std::move(arrow), ap, relpos);
        return true;
    }

    // Axes.annotate(text, xy, xytext=None, xycoords='data', textcoords=None, arrowprops=None,
    // annotation_clip=None, **kwargs): an Annotation (Text properties as keywords) placed at draw time
    // in its coordinates; its transform is the identity until then. No arrows yet.
    bool annotate(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".annotate";
        static const char* const names[] = {"text", "xy", "xytext", "xycoords", "textcoords", "arrowprops", "annotation_clip"};
        const Json* v[7];
        for (std::size_t i = 0; i < 7; ++i) v[i] = argument(call, i, names[i]);
        if (!v[0] || !v[0]->isString()) return *error = where + ": the text must be a string", false;
        const auto pair = [&](const Json* j, std::array<double, 2>* out) {
            const Json* s = j ? sequence(*j) : nullptr;
            return s && s->size() == 2 && readNumber((*s)[0], &(*out)[0]) && readNumber((*s)[1], &(*out)[1]);
        };
        std::array<double, 2> xy{}, xytext{};
        if (!v[1]) return *error = where + ": xy is required", false;
        if (!pair(v[1], &xy)) return *error = where + ": xy must be a pair of numbers", false;
        AnnotationCoords xycoords, textcoords;
        xycoords.names = {"data"};
        if (v[3] && !annotationCoords(*v[3], false, &xycoords, where, error)) return false;
        // textcoords None: the xycoords; xytext None: xy
        textcoords = xycoords;
        if (v[4] && !v[4]->isNull() && !annotationCoords(*v[4], true, &textcoords, where, error)) return false;
        xytext = xy;
        if (v[2] && !v[2]->isNull() && !pair(v[2], &xytext)) return *error = where + ": xytext must be a pair of numbers", false;
        const Json* arrowprops = v[5] && !v[5]->isNull() ? v[5] : nullptr;
        if (arrowprops && !(arrowprops->isObject() && arrowprops->members()[0].first == "$dict"))
            return *error = where + ": arrowprops must be a dict ({\"$dict\": {...}}) or None", false;
        std::optional<bool> clip;
        if (v[6] && !v[6]->isNull()) {
            if (!v[6]->isBool()) return *error = where + ": annotation_clip must be True, False or None", false;
            clip = v[6]->asBool();
        }
        auto a = std::make_unique<Annotation>(rc_, ax, v[0]->asString(), xy, xytext, xycoords, textcoords, clip);
        std::vector<std::pair<std::string, const Json*>> given;
        for (const auto& [name, value] : call.kwargs)
            if (std::find(std::begin(names), std::end(names), name) == std::end(names)) given.emplace_back(name, &value);
        if (arrowprops && !annotationArrow(*a, arrowprops->members()[0].second, where, error)) return false;
        if (!textProperties(*a, given, TextKeywords::Construct, where, error)) return false;
        // clip_on=True: clipped by the Axes' patch (a box); _add_text: the figure
        const Json* clipOn = keyword(call, "clip_on");
        if (clipOn && clipOn->isBool() && clipOn->asBool())
            ax.clipToPatch(*a);
        a->figure = &ax.figure;
        ax.children.push_back(std::move(a));
        return true;
    }

    // The keywords of a call but those the call takes itself; fontdict only as None
    bool otherKeywords(const model::Call& call, std::initializer_list<const char*> own,
                       std::vector<std::pair<std::string, const Json*>>* given, std::string* error) {
        for (const auto& keyword : call.kwargs) {
            const std::string& name = keyword.first;
            if (name == "fontdict") {
                // (a figure file has no dictionaries yet: the keywords say the same)
                if (!keyword.second.isNull()) return *error = call.on + "." + call.method + ": fontdict is not supported yet", false;
                continue;
            }
            if (std::find_if(own.begin(), own.end(), [&](const char* o) { return name == o; }) == own.end())
                given->emplace_back(name, &keyword.second);
        }
        return true;
    }

    // Figure.text(x, y, s, fontdict=None, **kwargs): a text in figure coordinates
    bool figureText(const model::Call& call, std::string* error) {
        const std::string where = call.on + ".text";
        const char* names[4] = {"x", "y", "s", "fontdict"};
        const Json* values[4] = {nullptr, nullptr, nullptr, nullptr};
        for (std::size_t i = 0; i < 4; ++i) values[i] = argument(call, i, names[i]);
        double x = 0, y = 0;
        if (!values[0] || !readNumber(*values[0], &x) || !values[1] || !readNumber(*values[1], &y))
            return *error = where + ": x and y must be numbers", false;
        if (!values[2] || !values[2]->isString()) return *error = where + ": the text must be a string", false;
        if (values[3] && !values[3]->isNull()) return *error = where + ": fontdict is not supported yet", false;
        std::vector<std::pair<std::string, const Json*>> given;
        if (!otherKeywords(call, {"x", "y", "s"}, &given, error)) return false;
        auto t = std::make_unique<Text>(rc_, x, y, values[2]->asString());
        if (!textProperties(*t, given, TextKeywords::Construct, where, error)) return false;
        t->transform = figure_->transSubfigure;
        t->figure = figure_.get();
        figure_->texts.push_back(std::move(t));
        return true;
    }

    // Axes.legend(**kwargs): the labelled lines, patches and collections, in the order they were added
    bool legend(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".legend";
        if (!call.args.empty()) return *error = where + ": handles and labels as arguments are not supported yet", false;
        LegendOptions o;
        const Json* ncols = nullptr;
        const Json* ncol = nullptr;
        // Legend.__init__'s parameters whose None is the default (alignment's is not one; the column
        // counts are read when the entries are known)
        static const char* const noneIsDefault[] = {
            "handles", "labels", "loc", "numpoints", "markerscale", "markerfirst", "reverse", "scatterpoints", "scatteryoffsets",
            "prop", "fontsize", "labelcolor", "borderpad", "labelspacing", "handlelength", "handleheight", "handletextpad",
            "borderaxespad", "columnspacing", "mode", "fancybox", "shadow", "title", "title_fontsize", "framealpha", "edgecolor",
            "facecolor", "linewidth", "bbox_to_anchor", "bbox_transform", "frameon", "handler_map", "title_fontproperties",
            "draggable"};
        for (const auto& [name, value] : call.kwargs) {
            double n = 0;
            if (name == "ncols" || name == "ncol") {
                (name == "ncols" ? ncols : ncol) = &value;
                continue;
            }
            // (markerfirst is used as it comes: None puts the markers last)
            if (name == "markerfirst" && value.isNull()) o.markerfirst = false;
            bool known = false;
            for (const char* k : noneIsDefault) known = known || name == k;
            if (value.isNull() && known) continue;
            if (name == "loc") {
                // set_loc: a name, an integer code 0-10 (a bool is one), or a pair of coordinates in the
                // box the legend is anchored to
                const char* const why = ": loc must be string, coordinate tuple, or an integer 0-10";
                const Json* pair = sequence(value);
                const auto real = [](const Json& j, double* out) {
                    if (j.isBool()) return *out = j.asBool() ? 1 : 0, true;
                    return j.isNumber() && (*out = j.asNumber(), true);
                };
                if (value.isString()) {
                    o.loc = value.asString();
                } else if (value.isBool()) {
                    o.locCode = value.asBool() ? 1 : 0;
                } else if (value.isNumber() && value.isInteger()) {
                    if (value.asNumber() < 0 || value.asNumber() > 10) return *error = where + why, false;
                    o.locCode = int(value.asNumber());
                } else if (pair && pair->size() == 2) {
                    std::array<double, 2> xy{};
                    if (!real((*pair)[0], &xy[0]) || !real((*pair)[1], &xy[1])) return *error = where + why, false;
                    o.locXY = xy;
                } else {
                    return *error = where + why, false;
                }
            } else if (name == "title") {
                if (!value.isString()) return *error = where + ": title must be a string", false;
                o.title = value.asString();
            } else if (name == "fontsize" || name == "title_fontsize") {
                // FontProperties.set_size: a number, a number as text, or the name of a size
                std::optional<double>& target = name == "fontsize" ? o.fontsize : o.titleFontsize;
                if (value.isString()) {
                    const auto parsed = support::pythonFloat(value.asString());
                    const auto size = parsed ? parsed : text::fontSize(value.asString(), rc_);
                    if (!size) return *error = where + ": unknown font size " + value.asString(), false;
                    target = *size;
                } else if (readNumber(value, &n)) {
                    target = n;
                } else {
                    return *error = where + ": bad " + name, false;
                }
                if (!std::isfinite(*target)) return *error = where + ": bad " + name, false;
            } else if (name == "frameon" || name == "fancybox") {
                if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name == "frameon" ? o.frameon : o.fancybox) = value.asBool();
            } else if (name == "labelcolor") {
                o.labelcolor = value;
            } else if (name == "markerfirst") {
                if (!value.isBool()) return *error = where + ": markerfirst must be True or False", false;
                o.markerfirst = value.asBool();
            } else if (name == "facecolor" || name == "edgecolor") {
                // a colour that to_rgba takes, or 'inherit' (the Axes' own)
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad " + name, false;
                const std::string* word = std::get_if<std::string>(&c);
                if (!(word && *word == "inherit") && !colors::toRgba(c, std::nullopt, rc_, nullptr))
                    return *error = where + ": bad " + name, false;
                (name == "facecolor" ? o.facecolor : o.edgecolor) = c;
            } else if (name == "shadow") {
                // a bool, or 0 or 1 (a dict of the shadow's properties cannot be written in a figure file yet)
                if (value.isBool()) o.shadow = value.asBool();
                else if (value.isNumber() && (value.asNumber() == 0 || value.asNumber() == 1)) o.shadow = value.asNumber() == 1;
                else return *error = where + ": Legend shadow must be a dict or bool", false;
            } else if (name == "numpoints" || name == "scatterpoints") {
                // counts of points (np.linspace takes an integer only)
                if (!value.isNumber() || !value.isInteger() || std::abs(value.asNumber()) > 1e6)
                    return *error = where + ": " + name + " must be an integer", false;
                (name == "numpoints" ? o.numpoints : o.scatterpoints) = int(value.asNumber());
            } else {
                static const char* const numeric[] = {"framealpha", "borderpad", "labelspacing", "handlelength",
                                                      "handletextpad", "borderaxespad", "columnspacing", "markerscale"};
                std::optional<double>* targets[] = {&o.framealpha, &o.borderpad, &o.labelspacing, &o.handlelength,
                                                    &o.handletextpad, &o.borderaxespad, &o.columnspacing, &o.markerscale};
                bool found = false;
                for (int i = 0; i < 8; ++i)
                    if (name == numeric[i]) {
                        if (!readNumber(value, &n)) return *error = where + ": " + name + " must be a number", false;
                        *targets[i] = n;
                        found = true;
                    }
                if (!found) return *error = where + ": unsupported keyword " + name, false;
            }
        }
        std::vector<Artist*> handles;
        std::vector<std::string> labels;
        for (const auto& child : ax.children) {
            const std::string& l = child->label;
            if (l.empty() || l[0] == '_') continue;
            handles.push_back(child.get());
            labels.push_back(l);
        }
        // _get_legend_handles: the containers come after the children
        std::vector<const ErrorbarContainer*> containers(handles.size(), nullptr);
        for (const auto& c : ax.containers) {
            if (c.label.empty() || c.label[0] == '_') continue;
            handles.push_back(nullptr);
            containers.push_back(&c);
            labels.push_back(c.label);
        }
        // ncols is 1 with fewer than two entries, whatever was given; and ncol counts when ncols is 1.
        // np.array_split reads the count: int() of it, 1 or more; the columns past the entries are empty.
        double columns = 1;
        const bool one = handles.size() < 2 || !ncols || (readNumber(*ncols, &columns) && columns == 1) ||
                         (ncols->isBool() && ncols->asBool());
        const Json* counted = one ? ncol : ncols;
        const std::string name = one ? "ncol" : "ncols";
        if (counted) {
            if (counted->isBool()) columns = counted->asBool() ? 1 : 0;
            else if (!readNumber(*counted, &columns)) return *error = where + ": " + name + " must be a number", false;
            if (!std::isfinite(columns)) return *error = where + ": " + name + " must be finite", false;
            columns = std::trunc(columns);
            if (columns < 1) return *error = where + ": " + name + " must be 1 or more", false;
            if (columns > std::numeric_limits<int>::max()) return *error = where + ": more than 2147483647 columns are not supported yet", false;
        }
        o.ncols = int(std::min(columns, double(handles.size())));
        std::string problem;
        auto made = std::make_unique<Legend>(rc_, ax, handles, containers, labels, o, &problem);
        if (!problem.empty()) return *error = where + ": " + problem, false;
        ax.legend = std::move(made);
        return true;
    }

    // A positional argument, else the keyword, else nullptr
    const Json* argument(const model::Call& call, std::size_t index, const char* name) {
        if (call.args.size() > index) return &call.args[index];
        return keyword(call, name);
    }

    // Axes.grid(visible=None, which='major', axis='both', **line properties)
    bool grid(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".grid";
        std::optional<bool> on;
        std::string which = "major", axisName = "both";
        TickSwitches style;
        bool styled = false, colorByItsName = false;
        if (const Json* v = argument(call, 0, "visible"); v && !v->isNull()) {
            if (!v->isBool()) return *error = where + ": visible must be True, False or None", false;
            on = v->asBool();
        }
        if (call.args.size() > 1) return *error = where + ": which and axis are keyword arguments here", false;
        // (the grid lines of the ticks take the properties with Artist.set, which normalises them)
        if (!givenOnce(call, lineAliases(), where, error)) return false;
        for (const auto& [name, value] : call.kwargs) {
            double n = 0;
            if (name == "visible") continue;
            if (name == "which" || name == "axis") {
                if (!value.isString()) return *error = where + ": " + name + " must be a string", false;
                (name == "which" ? which : axisName) = value.asString();
            } else if (name == "color" || name == "c") {
                colors::Spec checked;
                if (!color(value, &checked)) return *error = where + ": bad " + name, false;
                style.gridColor = checked, styled = true;
                colorByItsName = name == "color";
            } else if (name == "linestyle" || name == "ls") {
                // Line2D.set_linestyle, when the grid lines take it
                Line2D probe(rc_, {}, {});
                if (!value.isString() || !probe.setLinestyle(value.asString(), error)) return *error = where + ": bad linestyle", false;
                style.gridLinestyle = value.asString(), styled = true;
            } else if (name == "linewidth" || name == "lw") {
                if (!readFinite(value, &n)) return *error = where + ": linewidth must be a finite number", false;
                style.gridLinewidth = n, styled = true;
            } else if (name == "alpha") {
                if (!readAlpha(value, &n)) return *error = where + ": alpha must be a number from 0 to 1", false;
                style.gridAlpha = n, styled = true;
            } else {
                return *error = where + ": unsupported keyword " + name, false;
            }
        }
        // Tick._apply_params: a color (by that name) with an alpha channel, when no alpha comes with
        // it, is given to the grid lines as its RGB and its alpha
        if (colorByItsName && !style.gridAlpha && colors::hasAlphaChannel(*style.gridColor)) {
            const auto rgba = *colors::toRgba(*style.gridColor, std::nullopt, rc_, nullptr);
            style.gridColor = std::vector<double>{rgba[0], rgba[1], rgba[2]};
            style.gridAlpha = rgba[3];
        }
        for (char& ch : which) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (which != "major" && which != "minor" && which != "both") return *error = where + ": bad which", false;
        if (axisName != "x" && axisName != "y" && axisName != "both") return *error = where + ": bad axis", false;
        if (axisName != "y") ax.xaxis->grid(on, which, style, styled);
        if (axisName != "x") ax.yaxis->grid(on, which, style, styled);
        return true;
    }

    // Axes.axhline(y=0, xmin=0, xmax=1, **kwargs) and axvline(x=0, ymin=0, ymax=1, **kwargs)
    bool axline(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const bool horizontal = call.method == "axhline";
        double at = 0, lo = 0, hi = 1;
        const char* names[3] = {horizontal ? "y" : "x", horizontal ? "xmin" : "ymin", horizontal ? "xmax" : "ymax"};
        double* targets[3] = {&at, &lo, &hi};
        for (int i = 0; i < 3; ++i)
            if (const Json* v = argument(call, std::size_t(i), names[i]); v && !readNumber(*v, targets[i]))
                return *error = where + ": " + names[i] + " must be a number", false;
        // get_ybound / get_xbound read the view (a pending autoscale runs first)
        Axis& axis = horizontal ? *ax.yaxis : *ax.xaxis;
        const auto view = axis.viewInterval();
        const double bmin = std::min(view[0], view[1]), bmax = std::max(view[0], view[1]);
        const bool rescale = at < bmin || at > bmax;
        markDateUnits(ax, call);  // (after get_ybound / get_xbound, as matplotlib does)
        auto line = horizontal ? std::make_unique<Line2D>(rc_, std::vector<double>{lo, hi}, std::vector<double>{at, at})
                               : std::make_unique<Line2D>(rc_, std::vector<double>{at, at}, std::vector<double>{lo, hi});
        line->transform = horizontal ? ax.yaxisTransform : ax.xaxisTransform;
        // Line2D(..., **kwargs): its own parameters (those with an alias matter here), then the rest
        // through their setters, the aliases among them: an alias wins over its property, and None is
        // no default for it
        for (const auto& [name, value] :
             ownFirst(call, {"linewidth", "linestyle", "color", "markersize", "markeredgewidth", "markeredgecolor", "markerfacecolor",
                             "antialiased"})) {
            if (name == names[0] || name == names[1] || name == names[2]) continue;
            if (name == "c" && value->isNull()) return *error = where + ": bad color", false;
            if (!lineProperty(*line, fullName(name, lineAliases()), *value, where, error)) return false;
        }
        ax.addLine(std::move(line), true).setInterpolationSteps(180);  // GRIDLINE_INTERPOLATION_STEPS
        if (rescale) ax.requestAutoscaleView(!horizontal, horizontal);
        return true;
    }

    // A Patch property set as Artist.set does it: through its setter, None included (the setters take
    // None as their default). `closed` is a Polygon's.
    bool patchProperty(Patch& p, const std::string& name, const Json& v, const std::string& where, std::string* error) {
        double n = 0;
        const auto text = [&](std::string* out) {
            if (!v.isString()) return false;
            *out = v.asString();
            return true;
        };
        // bool(v) of a value a figure file holds for a flag
        const auto truth = [&](bool* out) {
            if (v.isNull()) return *out = false, true;
            if (v.isBool()) return *out = v.asBool(), true;
            if (readNumber(v, &n)) return *out = n != 0, true;
            return false;
        };
        std::string str;
        bool b = false;
        if (name == "color" || name == "facecolor" || name == "fc" || name == "edgecolor" || name == "ec") {
            // set_color / set_facecolor / set_edgecolor: a colour that to_rgba takes, or None for the default
            std::optional<colors::Spec> spec;
            if (!v.isNull()) {
                colors::Spec c;
                std::string why;
                if (!color(v, &c, {}, &why))
                    return *error = where + ": " + name + " is not a colour" + (why.empty() ? "" : ": " + why), false;
                spec = c;
            }
            if (name == "color") {
                p.setEdgecolor(spec);
                p.setHatchcolor(spec);
                p.setFacecolor(spec);
            } else if (name[0] == 'f') {
                p.setFacecolor(spec);
            } else {
                p.setEdgecolor(spec);
            }
        } else if (name == "alpha") {
            if (v.isNull()) p.setAlpha(std::nullopt);  // set_alpha(None)
            else if (!readAlpha(v, &n)) return *error = where + ": alpha must be a number from 0 to 1", false;
            else p.setAlpha(n, v.isNumber() && v.isInteger());
        } else if (name == "linewidth" || name == "lw") {
            if (v.isNull()) p.setLinewidth(std::nullopt);  // rcParams["patch.linewidth"]
            else if (!readFinite(v, &n)) return *error = where + ": linewidth must be a finite number", false;
            else p.setLinewidth(n);
        } else if (name == "linestyle" || name == "ls") {
            // Patch.set_linestyle: a name of mlines._get_dash_pattern ('None', ' ' and '' draw no edge); None: solid
            static const char* const styles[] = {"-", "--", "-.", ":", "solid", "dashed", "dashdot", "dotted", "None", "none", " ", ""};
            if (v.isNull()) str = "solid";
            else if (!text(&str) || std::find_if(std::begin(styles), std::end(styles), [&](const char* s) { return str == s; }) == std::end(styles))
                return *error = where + ": bad linestyle", false;
            p.setLinestyle(str == "-" ? "solid" : str == "--" ? "dashed" : str == "-." ? "dashdot" : str == ":" ? "dotted" : str);
        } else if (name == "fill") {
            if (!truth(&b)) return *error = where + ": fill must be True or False", false;
            p.setFill(b);
        } else if (name == "closed" && dynamic_cast<Polygon*>(&p)) {
            if (!truth(&b)) return *error = where + ": closed must be True, False or None", false;
            static_cast<Polygon&>(p).setClosed(b);
        } else if (name == "hatch") {
            // set_hatch: a pattern, or None
            if (!v.isNull() && !v.isString()) return *error = where + ": hatch must be a string", false;
            p.setHatch(v.isNull() ? std::nullopt : std::optional<std::string>(v.asString()));
        } else if (name == "hatchcolor") {
            if (v.isNull()) {
                p.setHatchcolor(std::nullopt);
            } else {
                colors::Spec spec;
                if (v.isString() && v.asString() == "edge") spec = std::string("edge");
                else if (!color(v, &spec)) return *error = where + ": hatchcolor is not a colour", false;
                p.setHatchcolor(spec);
            }
        } else if (name == "zorder") {
            if (v.isNull()) p.zorder = 1;  // set_zorder(None): the class's, Patch.zorder
            else if (!readFinite(v, &n)) return *error = where + ": zorder must be a finite number", false;
            else p.zorder = n;
        } else if (name == "label") {
            if (v.isNull()) p.label.clear();  // (None: no label)
            else if (!text(&str)) return *error = where + ": label must be a string", false;
            else p.label = str;
        } else {
            return *error = where + ": unsupported keyword " + name, false;
        }
        return true;
    }

    // A list of (x, y) points: the vertices x0, y0, x1, y1, ...
    bool points(const Json& v, std::vector<double>* out) {
        const Json* seq = sequence(v);
        if (!seq || seq->items().empty()) return false;
        for (const Json& item : seq->items()) {
            const Json* xy = sequence(item);
            double x = 0, y = 0;
            if (!xy || xy->size() != 2 || !readNumber((*xy)[0], &x) || !readNumber((*xy)[1], &y)) return false;
            out->push_back(x), out->push_back(y);
        }
        return true;
    }

    // Patch.__init__(*, edgecolor=None, facecolor=None, color=None, linewidth=None, linestyle=None,
    // antialiased=None, hatch=None, fill=True, capstyle=None, joinstyle=None, hatchcolor=None,
    // edgegapcolor=None, **kwargs): its own parameters in its order (fill first; a colour replaces
    // edgecolor, hatchcolor and facecolor, which are then not read), then the other keywords through
    // their setters as given (aliases included: no normalize_kwargs there).
    bool patchConstructor(Patch& p, const std::vector<std::pair<std::string, const Json*>>& kwargs, const std::string& where,
                          std::string* error) {
        static const char* const own[] = {"edgecolor", "facecolor", "color", "linewidth", "linestyle", "antialiased", "hatch",
                                          "fill", "capstyle", "joinstyle", "hatchcolor", "edgegapcolor"};
        const auto find = [&](const char* name) -> const Json* {
            for (const auto& [n, v] : kwargs)
                if (n == name) return v;
            return nullptr;
        };
        const auto value = [&](const char* name) {
            const Json* v = find(name);
            return v && !v->isNull() ? v : nullptr;
        };
        if (const Json* v = find("fill"); v && !patchProperty(p, "fill", *v, where, error)) return false;
        if (const Json* c = value("color")) {
            if (!patchProperty(p, "color", *c, where, error)) return false;
        } else {
            for (const char* name : {"edgecolor", "hatchcolor", "facecolor"})
                if (const Json* v = value(name); v && !patchProperty(p, name, *v, where, error)) return false;
        }
        for (const char* name : {"linestyle", "linewidth", "hatch"})
            if (const Json* v = value(name); v && !patchProperty(p, name, *v, where, error)) return false;
        for (const char* name : {"antialiased", "capstyle", "joinstyle", "edgegapcolor"})
            if (value(name)) return *error = where + ": unsupported keyword " + name, false;
        for (const auto& [name, v] : kwargs) {
            if (std::find(std::begin(own), std::end(own), name) != std::end(own)) continue;
            if (!patchProperty(p, name, *v, where, error)) return false;
        }
        return true;
    }

    // Axes.stem(*args, linefmt=None, markerfmt=None, basefmt=None, bottom=0, label=None,
    // orientation='vertical'): stems by vlines (hlines), markers and a baseline by plot, kept together
    // in a StemContainer with the label
    bool stem(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".stem";
        if (call.args.empty() || call.args.size() > 3)
            return *error = where + ": stem() takes from 1 to 3 positional arguments but " + std::to_string(call.args.size()) +
                            " were given",
                   false;
        const Json* linefmt = nullptr;
        const Json* markerfmt = nullptr;
        const Json* basefmt = nullptr;
        const Json* bottomJson = nullptr;
        const Json* label = nullptr;
        bool vertical = true;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "linefmt") linefmt = &value;
            else if (name == "markerfmt") markerfmt = &value;
            else if (name == "basefmt") basefmt = &value;
            else if (name == "bottom") bottomJson = &value;
            else if (name == "label") label = &value;
            else if (name == "orientation") {
                if (!value.isString() || (value.asString() != "horizontal" && value.asString() != "vertical"))
                    return *error = where + ": " + (value.isString() ? "'" + value.asString() + "'" : std::string("that")) +
                                    " is not a valid value for orientation; supported values are 'horizontal', 'vertical'",
                           false;
                vertical = value.asString() == "vertical";
            } else {
                return *error = where + ": got an unexpected keyword argument '" + name + "'", false;
            }
        }
        // heads alone, heads and a format, or locs and heads (then a format)
        std::vector<double> locs, heads;
        std::size_t rest = 0;
        if (call.args.size() == 1 || call.args[1].isString()) {
            if (!numbers(call.args[0], &heads)) return *error = where + ": the heads must be numbers", false;
            for (std::size_t i = 0; i < heads.size(); ++i) locs.push_back(double(i));
            rest = 1;
        } else {
            if (!numbers(call.args[0], &locs) || !numbers(call.args[1], &heads)) return *error = where + ": locs and heads must be numbers", false;
            rest = 2;
        }
        const auto fmtOf = [&](const Json* given, const char* fallback, std::string* out) {
            if (given && !given->isNull()) {
                if (!given->isString()) return *error = where + ": a format must be a string", false;
                *out = given->asString();
            } else {
                *out = fallback;
            }
            return true;
        };
        std::string lineFmt, markerFmt, baseFmt;
        if (!linefmt && call.args.size() > rest) linefmt = &call.args[rest];
        if (!fmtOf(linefmt, "C0-", &lineFmt)) return false;
        std::optional<std::string> linestyle, linemarker, markerstyle, markermarker, basestyle, basemarker;
        std::optional<colors::Spec> linecolor, markercolor, basecolor;
        if (!plotFormat(lineFmt, &linestyle, &linemarker, &linecolor, error)) return *error = where + ": " + *error, false;
        if (!fmtOf(markerfmt, "o", &markerFmt)) return false;
        if (markerFmt.empty()) markerFmt = " ";  // (an empty line style; '' would take the rcParams)
        if (!plotFormat(markerFmt, &markerstyle, &markermarker, &markercolor, error)) return *error = where + ": " + *error, false;
        if (!markermarker) markermarker = "o";
        if (!markerstyle) markerstyle = "None";
        if (!markercolor) markercolor = linecolor;
        if (!fmtOf(basefmt, rc_.boolean("_internal.classic_mode") ? "C2-" : "C3-", &baseFmt)) return false;
        if (!plotFormat(baseFmt, &basestyle, &basemarker, &basecolor, error)) return *error = where + ": " + *error, false;
        if (!linestyle) linestyle = rc_.text("lines.linestyle");
        double bottom = 0;
        if (bottomJson && !readNumber(*bottomJson, &bottom)) return *error = where + ": bottom must be a number (others are not supported yet)", false;
        if (locs.size() != heads.size()) return *error = where + ": locs and heads must have the same length", false;
        if (locs.empty()) return *error = where + ": zero-size array to reduction operation minimum which has no identity", false;
        const Json bottomValue = bottomJson ? *bottomJson : Json::integer(0);
        const auto optColor = [&](const std::optional<colors::Spec>& c) { return c ? colorJson(*c) : Json(); };
        const auto optText = [](const std::optional<std::string>& s) { return s ? Json::string(*s) : Json(); };
        // the stems
        model::Call lines;
        lines.on = call.on;
        lines.method = vertical ? "vlines" : "hlines";
        lines.args = {numbersJson(locs), bottomValue, numbersJson(heads)};
        lines.kwargs = {{"colors", optColor(linecolor)}, {"linestyles", Json::string(*linestyle)}, {"label", Json::string("_nolegend_")}};
        if (!this->lines(ax, lines, error)) return false;
        Collection* stemLines = dynamic_cast<Collection*>(ax.children.back().get());
        // np.min(locs), np.max(locs): NaN wins
        double lo = locs[0], hi = locs[0];
        for (double v : locs) {
            lo = std::isnan(lo) || std::isnan(v) ? std::nan("") : std::min(lo, v);
            hi = std::isnan(hi) || std::isnan(v) ? std::nan("") : std::max(hi, v);
        }
        const auto plotLine = [&](const std::vector<double>& x, const std::vector<double>& y, const Json& color, const Json& style,
                                  const Json& marker) -> Line2D* {
            model::Call p;
            p.on = call.on;
            p.method = "plot";
            p.args = {numbersJson(x), numbersJson(y)};
            p.kwargs = {{"color", color}, {"linestyle", style}, {"marker", marker}, {"label", Json::string("_nolegend_")}};
            if (!plot(ax, p, error)) return nullptr;
            return dynamic_cast<Line2D*>(ax.children.back().get());
        };
        Line2D* marker = vertical ? plotLine(locs, heads, optColor(markercolor), optText(markerstyle), optText(markermarker))
                                  : plotLine(heads, locs, optColor(markercolor), optText(markerstyle), optText(markermarker));
        if (!marker) return false;
        Line2D* base = vertical ? plotLine({lo, hi}, {bottom, bottom}, optColor(basecolor), optText(basestyle), optText(basemarker))
                                : plotLine({bottom, bottom}, {lo, hi}, optColor(basecolor), optText(basestyle), optText(basemarker));
        if (!base) return false;
        base->setInterpolationSteps(180);  // baseline.get_path()._interpolation_steps = GRIDLINE_INTERPOLATION_STEPS
        ErrorbarContainer container;
        container.isStem = true;
        container.stemMarker = marker;
        container.stemLines = stemLines;
        container.stemBaseline = base;
        if (label && !label->isNull()) {
            if (!label->isString()) return *error = where + ": label must be a string", false;
            container.label = label->asString();
        }
        ax.containers.push_back(container);
        return true;
    }

    // cbook.boxplot_stats of one data set: (`whis`: a factor of the IQR, or two percentiles)
    struct BoxStats {
        double mean, med, q1, q3, iqr, cilo, cihi, whislo, whishi;
        std::vector<double> fliers;
        std::optional<std::string> label;
    };
    static BoxStats boxStats(const std::vector<double>& x, bool integer, double whisFactor,
                             std::optional<std::pair<double, double>> whisPercentiles, bool autorange) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        BoxStats s{nan, nan, nan, nan, nan, nan, nan, nan, nan, {}, std::nullopt};
        if (x.empty()) return s;
        s.mean = support::npMean(x, integer);
        s.q1 = support::npPercentile(x, 25), s.med = support::npPercentile(x, 50), s.q3 = support::npPercentile(x, 75);
        s.iqr = s.q3 - s.q1;
        if (s.iqr == 0 && autorange) whisPercentiles = std::make_pair(0.0, 100.0);
        // _compute_conf_interval without bootstrap
        const double n = double(x.size());
        s.cilo = s.med - 1.57 * s.iqr / std::sqrt(n);
        s.cihi = s.med + 1.57 * s.iqr / std::sqrt(n);
        double loval, hival;
        if (whisPercentiles) {
            loval = support::npPercentile(x, whisPercentiles->first);
            hival = support::npPercentile(x, whisPercentiles->second);
        } else {
            loval = s.q1 - whisFactor * s.iqr;
            hival = s.q3 + whisFactor * s.iqr;
        }
        // the whiskers: the farthest values within reach (np.max(x[x <= hival]), np.min(x[x >= loval])),
        // but never inside the box
        std::vector<double> wiskhi, wisklo;
        for (double v : x) {
            if (v <= hival) wiskhi.push_back(v);
            if (v >= loval) wisklo.push_back(v);
        }
        s.whishi = wiskhi.empty() || support::npMax(wiskhi) < s.q3 ? s.q3 : support::npMax(wiskhi);
        s.whislo = wisklo.empty() || support::npMin(wisklo) > s.q1 ? s.q1 : support::npMin(wisklo);
        for (double v : x)
            if (v < s.whislo) s.fliers.push_back(v);
        for (double v : x)
            if (v > s.whishi) s.fliers.push_back(v);
        return s;
    }

    // Axes.boxplot(x, ...) with cbook.boxplot_stats, then Axes.bxp: lines by plot (the boxes by
    // add_artist of PathPatches with patch_artist), the properties from rcParams["boxplot.*props.*"];
    // manage_ticks: the data limits reach half a unit past the positions, which become the axis' fixed
    // ticks, labelled with the tick_labels or the positions
    bool boxplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".boxplot";
        static const char* const names[] = {"x", "notch", "sym", "vert", "orientation", "whis", "positions", "widths",
                                            "patch_artist", "bootstrap", "usermedians", "conf_intervals", "meanline",
                                            "showmeans", "showcaps", "showbox", "showfliers", "boxprops", "tick_labels",
                                            "flierprops", "medianprops", "meanprops", "capprops", "whiskerprops",
                                            "manage_ticks", "autorange", "zorder", "capwidths", "label"};
        constexpr std::size_t count = sizeof names / sizeof names[0];
        if (call.args.size() > count) return *error = where + ": too many positional arguments", false;
        for (const auto& kw : call.kwargs)
            if (kw.first != "data" && std::find(std::begin(names), std::end(names), kw.first) == std::end(names))
                return *error = where + ": got an unexpected keyword argument '" + kw.first + "'", false;
        if (const Json* d = keyword(call, "data"); d && !d->isNull()) return *error = where + ": data is not supported yet", false;
        const auto arg = [&](const char* name) -> const Json* {
            for (std::size_t i = 0; i < count; ++i)
                if (std::string(names[i]) == name) {
                    const Json* v = argument(call, i, name);
                    return v && !v->isNull() ? v : nullptr;
                }
            return nullptr;
        };
        double one = 0;
        // bool(v) of a value, or of the rcParam when it is None
        const auto flag = [&](const char* name, const char* rcKey, bool fallback, bool* out) {
            const Json* v = arg(name);
            const Json& w = v ? *v : (rcKey ? rc_.value(rcKey) : Json());
            if (w.isNull()) return *out = fallback, true;
            if (w.isBool()) return *out = w.asBool(), true;
            if (readNumber(w, &one)) return *out = one != 0, true;
            return *error = where + ": " + name + " must be True or False", false;
        };
        for (const char* props : {"boxprops", "flierprops", "medianprops", "meanprops", "capprops", "whiskerprops"})
            if (arg(props)) return *error = where + ": " + props + " is not supported yet", false;
        if (arg("bootstrap") || !rc_.value("boxplot.bootstrap").isNull())
            return *error = where + ": bootstrap (a random estimate) is not supported", false;
        // the data sets
        const Json* xJson = arg("x");
        if (!xJson) return *error = where + ": x is required", false;
        std::vector<std::vector<double>> sets;
        std::vector<bool> setInts;
        if (!reshape2D(*xJson, &sets, &setInts)) return *error = where + ": x must be lists of numbers", false;
        const std::size_t n = sets.size();
        // whis: a number, or two percentiles
        const Json& whisJson = arg("whis") ? *arg("whis") : rc_.value("boxplot.whiskers");
        double whisFactor = 1.5;
        std::optional<std::pair<double, double>> whisPercentiles;
        std::vector<double> pair;
        if (readNumber(whisJson, &one)) whisFactor = one;
        else if (numbers(whisJson, &pair) && pair.size() == 2) whisPercentiles = std::make_pair(pair[0], pair[1]);
        else return *error = where + ": whis must be a float or list of percentiles", false;
        bool autorange = false;
        if (!flag("autorange", nullptr, false, &autorange)) return false;
        std::vector<std::string> tickLabels;
        if (const Json* l = arg("tick_labels")) {
            const Json* seq = sequence(*l);
            if (!seq) return *error = where + ": tick_labels must be a list of strings", false;
            for (const Json& s : seq->items()) {
                if (!s.isString()) return *error = where + ": tick_labels must be strings (others are not supported yet)", false;
                tickLabels.push_back(s.asString());
            }
            if (tickLabels.size() != n)
                return *error = where + ": The number of labels (" + std::to_string(tickLabels.size()) + ") must match the number of columns (" +
                                std::to_string(n) + ").",
                       false;
        }
        std::vector<BoxStats> stats;
        for (std::size_t i = 0; i < n; ++i) {
            stats.push_back(boxStats(sets[i], setInts[i], whisFactor, whisPercentiles, autorange));
            if (!tickLabels.empty()) stats.back().label = tickLabels[i];
        }
        bool notch = false, patchArtist = false, meanline = false, showmeans = false, showcaps = true, showbox = true, showfliers = true;
        if (!flag("notch", "boxplot.notch", false, &notch) || !flag("patch_artist", "boxplot.patchartist", false, &patchArtist) ||
            !flag("meanline", "boxplot.meanline", false, &meanline) || !flag("showmeans", "boxplot.showmeans", false, &showmeans) ||
            !flag("showcaps", "boxplot.showcaps", true, &showcaps) || !flag("showbox", "boxplot.showbox", true, &showbox) ||
            !flag("showfliers", "boxplot.showfliers", true, &showfliers))
            return false;
        // sym: '' hides the fliers; otherwise its marker and colour
        std::vector<std::pair<std::string, Json>> flierprops;
        if (const Json* sym = arg("sym")) {
            if (!sym->isString()) return *error = where + ": sym must be a string", false;
            if (sym->asString().empty()) {
                flierprops = {{"linestyle", Json::string("none")}, {"marker", Json::string("")}, {"color", Json::string("none")}};
                showfliers = false;
            } else {
                std::optional<std::string> style, marker;
                std::optional<colors::Spec> color;
                if (!plotFormat(sym->asString(), &style, &marker, &color, error)) return *error = where + ": " + *error, false;
                if (marker) flierprops.emplace_back("marker", Json::string(*marker));
                if (color) {
                    for (const char* k : {"color", "markerfacecolor", "markeredgecolor"}) flierprops.emplace_back(k, colorJson(*color));
                }
            }
        }
        // usermedians and conf_intervals replace what the statistics say
        if (const Json* um = arg("usermedians")) {
            const Json* seq = sequence(*um);
            if (!seq || seq->size() != n) return *error = where + ": 'usermedians' and 'x' have different lengths", false;
            for (std::size_t i = 0; i < n; ++i) {
                if ((*seq)[i].isNull()) continue;
                if (!readNumber((*seq)[i], &one)) return *error = where + ": usermedians must be numbers or None", false;
                stats[i].med = one;
            }
        }
        if (const Json* ci = arg("conf_intervals")) {
            const Json* seq = sequence(*ci);
            if (!seq || seq->size() != n) return *error = where + ": 'conf_intervals' and 'x' have different lengths", false;
            for (std::size_t i = 0; i < n; ++i) {
                if ((*seq)[i].isNull()) continue;
                const Json* two = sequence((*seq)[i]);
                if (!two || two->size() != 2) return *error = where + ": each confidence interval must have two values", false;
                for (int k = 0; k < 2; ++k) {
                    if ((*two)[std::size_t(k)].isNull()) continue;
                    if (!readNumber((*two)[std::size_t(k)], &one)) return *error = where + ": confidence intervals must be numbers", false;
                    (k == 0 ? stats[i].cilo : stats[i].cihi) = one;
                }
            }
        }
        // --- bxp
        bool vertical = true;
        // vert None: rcParams["boxplot.vertical"]; vert is False: horizontal, whatever the orientation
        const Json& vert = arg("vert") ? *arg("vert") : rc_.value("boxplot.vertical");
        if (vert.isBool() && !vert.asBool()) vertical = false;
        else if (const Json* o = arg("orientation")) {
            if (!o->isString() || (o->asString() != "horizontal" && o->asString() != "vertical"))
                return *error = where + ": " + (o->isString() ? "'" + o->asString() + "'" : std::string("that")) +
                                " is not a valid value for orientation; supported values are 'horizontal', 'vertical'",
                       false;
            vertical = o->asString() == "vertical";
        }
        double zorder = 2;  // Line2D.zorder
        if (const Json* z = arg("zorder"); z && !readFinite(*z, &zorder)) return *error = where + ": zorder must be a finite number", false;
        using Kw = std::vector<std::pair<std::string, Json>>;
        const auto update = [](Kw& d, const Kw& with) {
            for (const auto& entry : with) {
                auto slot = std::find_if(d.begin(), d.end(), [&](const auto& e) { return e.first == entry.first; });
                if (slot != d.end()) slot->second = entry.second;
                else d.push_back(entry);
            }
        };
        const auto setdefault = [](Kw& d, const std::string& k, const Json& v) {
            if (std::none_of(d.begin(), d.end(), [&](const auto& e) { return e.first == k; })) d.emplace_back(k, v);
        };
        // merge_kw_rc: rcParams["boxplot.<sub>props.*"] (alphabetical), the zorder, no marker, then the props given
        const auto mergeKwRc = [&](const std::string& sub, const Kw& explicitProps, double zdelta, bool usemarker) {
            Kw d;
            const std::string prefix = "boxplot." + sub + "props";
            for (const std::string& key : rc_.keys())
                if (key.rfind(prefix, 0) == 0) d.emplace_back(key.substr(key.rfind('.') + 1), rc_.value(key));
            update(d, {{"zorder", Json::real(zorder + zdelta)}});
            if (!usemarker) update(d, {{"marker", Json::string("")}});
            update(d, explicitProps);
            return d;
        };
        const Kw butt = {{"solid_capstyle", Json::string("butt")}, {"dash_capstyle", Json::string("butt")}};
        const double zdelta = 0.1;
        Kw boxKw;
        if (patchArtist) {
            boxKw = {{"linestyle", rc_.value("boxplot.boxprops.linestyle")},
                     {"linewidth", rc_.value("boxplot.boxprops.linewidth")},
                     {"edgecolor", rc_.value("boxplot.boxprops.color")},
                     {"facecolor", rc_.boolean("_internal.classic_mode") ? Json::string("white") : rc_.value("patch.facecolor")},
                     {"zorder", Json::real(zorder)}};
            update(boxKw, {{"linestyle", Json::string("solid")}});  // (boxplot sets it for patch_artist)
        } else {
            boxKw = mergeKwRc("box", {}, 0, false);
        }
        Kw whiskerKw = mergeKwRc("whisker", {}, 0, false), capKw = mergeKwRc("cap", {}, 0, false);
        Kw flierKw = mergeKwRc("flier", flierprops, 0, true), medianKw = mergeKwRc("median", butt, zdelta, false);
        Kw meanKw = mergeKwRc("mean", butt, zdelta, true);
        update(meanKw, {{meanline ? "marker" : "linestyle", Json::string("")}});
        // positions, widths, capwidths
        std::vector<double> positions;
        bool positionsInt = true;
        if (const Json* p = arg("positions")) {
            if (!numbers(*p, &positions)) return *error = where + ": positions should be an iterable of numbers", false;
            if (positions.size() != n) return *error = where + ": List of boxplot statistics and `positions` values must have same the length", false;
            positionsInt = integers(*p);
        } else {
            for (std::size_t i = 0; i < n; ++i) positions.push_back(double(i + 1));
        }
        const auto sizesOf = [&](const char* name, std::vector<double>* out) {
            const Json* w = arg(name);
            if (!w) return false;
            if (readNumber(*w, &one)) out->assign(n, one);
            else if (!numbers(*w, out)) return *error = where + ": " + name + " must be numbers", true;
            else if (out->size() != n) return *error = where + ": List of boxplot statistics and `" + name + "` values must have same the length", true;
            return true;
        };
        std::vector<double> widths, capwidths;
        if (sizesOf("widths", &widths)) {
            if (!error->empty()) return false;
        } else {
            // [np.clip(0.15 * np.ptp(positions), 0.15, 0.5)] * N
            const auto [lo, hi] = std::minmax_element(positions.begin(), positions.end());
            double w = n ? 0.15 * (*hi - *lo) : 0;
            if (!std::isnan(w)) w = std::min(std::max(w, 0.15), 0.5);
            widths.assign(n, w);
        }
        if (sizesOf("capwidths", &capwidths)) {
            if (!error->empty()) return false;
        } else {
            for (double w : widths) capwidths.push_back(0.5 * w);
        }
        const auto doPlot = [&](std::vector<double> xs, std::vector<double> ys, const Kw& kw) -> Line2D* {
            if (!vertical) std::swap(xs, ys);
            model::Call p;
            p.on = call.on;
            p.method = "plot";
            p.args = {numbersJson(xs), numbersJson(ys)};
            p.kwargs = kw;
            if (!plot(ax, p, error)) return nullptr;
            return dynamic_cast<Line2D*>(ax.children.back().get());
        };
        const auto doPatch = [&](std::vector<double> xs, std::vector<double> ys, const Kw& kw) -> Patch* {
            if (!vertical) std::swap(xs, ys);
            std::vector<double> v;
            for (std::size_t k = 0; k < xs.size(); ++k) v.push_back(xs[k]), v.push_back(ys[k]);
            v.push_back(xs[0]), v.push_back(ys[0]);  // Path._create_closed
            auto patch = std::make_unique<PathPatch>(rc_, Path::make(std::move(v), {}, rc_, true));
            std::vector<std::pair<std::string, const Json*>> given;
            for (const auto& [k, value] : kw) given.emplace_back(k, &value);
            if (!patchConstructor(*patch, given, where, error)) return nullptr;
            return &ax.addArtist(std::move(patch));
        };
        std::vector<Artist*> boxes, medians;
        std::vector<std::string> datalabels;
        const Json nolegend = Json::string("_nolegend_");
        for (std::size_t i = 0; i < n; ++i) {
            const BoxStats& s = stats[i];
            const double pos = positions[i], width = widths[i], capwidth = capwidths[i];
            datalabels.push_back(s.label ? *s.label : positionsInt ? std::to_string(static_cast<long long>(pos)) : support::pythonRepr(pos));
            const double capLeft = pos - capwidth * 0.5, capRight = pos + capwidth * 0.5;
            const double boxLeft = pos - width * 0.5, boxRight = pos + width * 0.5;
            std::vector<double> boxX, boxY, medX;
            if (notch) {
                const double notchLeft = pos - width * 0.25, notchRight = pos + width * 0.25;
                boxX = {boxLeft, boxRight, boxRight, notchRight, boxRight, boxRight, boxLeft, boxLeft, notchLeft, boxLeft, boxLeft};
                boxY = {s.q1, s.q1, s.cilo, s.med, s.cihi, s.q3, s.q3, s.cihi, s.med, s.cilo, s.q1};
                medX = {notchLeft, notchRight};
            } else {
                boxX = {boxLeft, boxRight, boxRight, boxLeft, boxLeft};
                boxY = {s.q1, s.q1, s.q3, s.q3, s.q1};
                medX = {boxLeft, boxRight};
            }
            if (showbox) {
                Artist* box = patchArtist ? static_cast<Artist*>(doPatch(boxX, boxY, boxKw)) : doPlot(boxX, boxY, boxKw);
                if (!box) return false;
                boxes.push_back(box);
                setdefault(medianKw, "label", nolegend);
            }
            setdefault(whiskerKw, "label", nolegend);
            if (!doPlot({pos, pos}, {s.q1, s.whislo}, whiskerKw) || !doPlot({pos, pos}, {s.q3, s.whishi}, whiskerKw)) return false;
            if (showcaps) {
                setdefault(capKw, "label", nolegend);
                if (!doPlot({capLeft, capRight}, {s.whislo, s.whislo}, capKw) || !doPlot({capLeft, capRight}, {s.whishi, s.whishi}, capKw))
                    return false;
            }
            Line2D* median = doPlot(medX, {s.med, s.med}, medianKw);
            if (!median) return false;
            medians.push_back(median);
            if (showmeans) {
                if (meanline ? !doPlot({boxLeft, boxRight}, {s.mean, s.mean}, meanKw) : !doPlot({pos}, {s.mean}, meanKw)) return false;
            }
            if (showfliers) {
                setdefault(flierKw, "label", nolegend);
                if (!doPlot(std::vector<double>(s.fliers.size(), pos), s.fliers, flierKw)) return false;
            }
        }
        // label: on the boxes (patch_artist) or the medians
        if (const Json* label = arg("label")) {
            std::vector<Artist*>& target = showbox && patchArtist ? boxes : medians;
            if (label->isString()) {
                if (!label->asString().empty()) {
                    if (target.empty()) return *error = where + ": list index out of range", false;
                    target[0]->label = label->asString();
                }
            } else if (const Json* seq = sequence(*label)) {
                if (!seq->items().empty()) {
                    if (seq->size() != target.size())
                        return *error = where + ": List of boxplot statistics and `label` values must have same the length", false;
                    for (std::size_t i = 0; i < target.size(); ++i) {
                        if (!(*seq)[i].isString()) return *error = where + ": labels must be strings", false;
                        target[i]->label = (*seq)[i].asString();
                    }
                }
            } else {
                return *error = where + ": label must be a string or a list of strings", false;
            }
        }
        bool manageTicks = true;
        if (!flag("manage_ticks", nullptr, true, &manageTicks)) return false;
        if (manageTicks && n) {
            Axis& axis = vertical ? *ax.xaxis : *ax.yaxis;
            // dataLim.intervalx (y) reaches half a unit past the positions
            const auto [lo, hi] = std::minmax_element(positions.begin(), positions.end());
            auto p = ax.dataLim->points();
            const int k = vertical ? 0 : 1;
            const double a = *lo - .5, b = *hi + .5;
            if (a < p[std::size_t(k)]) p[std::size_t(k)] = a;          // min(interval[0], min(positions) - .5)
            if (b > p[std::size_t(k + 2)]) p[std::size_t(k + 2)] = b;  // max(interval[1], max(positions) + .5)
            ax.dataLim->setPoints(p);
            for (std::size_t i = 0; i < medians.size(); ++i) {
                auto& sticky = vertical ? medians[i]->stickyX : medians[i]->stickyY;
                sticky.push_back(positions[i] - .5);
                sticky.push_back(positions[i] + .5);
            }
            Axis& owner = axis.tickerAxis();
            if (!owner.fixedLocs) owner.fixedLocs = std::vector<double>{};
            owner.fixedLocs->insert(owner.fixedLocs->end(), positions.begin(), positions.end());
            if (!owner.fixedLabels) owner.fixedLabels = std::vector<std::string>{};
            owner.fixedLabels->insert(owner.fixedLabels->end(), datalabels.begin(), datalabels.end());
            owner.nullMajorFormatter = false;
            ax.requestAutoscaleView(true, true);
        }
        return true;
    }

    // Axes.violinplot(dataset, positions=None, vert=None, orientation='vertical', widths=0.5,
    // showmeans=False, showextrema=True, showmedians=False, quantiles=None, points=100,
    // bw_method=None, side='both', facecolor=None, linecolor=None): cbook.violin_stats, then
    // Axes.violin: the bodies by fill_betweenx (fill_between), the lines by hlines and vlines
    bool violinplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".violinplot";
        static const char* const names[] = {"dataset", "positions", "vert", "orientation", "widths", "showmeans", "showextrema",
                                            "showmedians", "quantiles", "points", "bw_method", "side", "facecolor", "linecolor"};
        constexpr std::size_t count = sizeof names / sizeof names[0];
        if (call.args.size() > count) return *error = where + ": too many positional arguments", false;
        for (const auto& kw : call.kwargs)
            if (kw.first != "data" && std::find(std::begin(names), std::end(names), kw.first) == std::end(names))
                return *error = where + ": got an unexpected keyword argument '" + kw.first + "'", false;
        if (const Json* d = keyword(call, "data"); d && !d->isNull()) return *error = where + ": data is not supported yet", false;
        const auto arg = [&](const char* name) -> const Json* {
            for (std::size_t i = 0; i < count; ++i)
                if (std::string(names[i]) == name) {
                    const Json* v = argument(call, i, name);
                    return v && !v->isNull() ? v : nullptr;
                }
            return nullptr;
        };
        double one = 0;
        const auto flag = [&](const char* name, bool fallback, bool* out) {
            const Json* v = arg(name);
            if (!v) return *out = fallback, true;
            if (v->isBool()) return *out = v->asBool(), true;
            if (readNumber(*v, &one)) return *out = one != 0, true;
            return *error = where + ": " + name + " must be True or False", false;
        };
        const Json* dataset = arg("dataset");
        if (!dataset) return *error = where + ": dataset is required", false;
        std::vector<std::vector<double>> sets;
        std::vector<bool> setInts;
        if (!reshape2D(*dataset, &sets, &setInts)) return *error = where + ": dataset must be lists of numbers", false;
        const std::size_t n = sets.size();
        // quantiles: one list per data set (_reshape_2D), or none
        std::vector<std::vector<double>> quantiles(n);
        if (const Json* q = arg("quantiles"); q && !(sequence(*q) && sequence(*q)->items().empty())) {
            std::vector<bool> unused;
            if (!reshape2D(*q, &quantiles, &unused)) return *error = where + ": quantiles must be lists of numbers", false;
            if (quantiles.size() != n)
                return *error = where + ": List of violinplot statistics and quantiles values must have the same length", false;
        }
        Bandwidth bw;
        if (const Json* b = arg("bw_method")) {
            if (b->isString() && (b->asString() == "scott" || b->asString() == "silverman")) bw.rule = b->asString();
            else if (b->isNumber() && readNumber(*b, &one)) bw.factor = one;
            else return *error = where + ": `bw_method` should be 'scott', 'silverman', a scalar or a callable", false;
        }
        long long points = 100;
        if (const Json* p = arg("points")) {
            if (!p->isNumber() || !p->isInteger()) return *error = where + ": points must be an integer", false;
            points = static_cast<long long>(p->asNumber());
        }
        std::vector<ViolinStats> stats(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!violinStats(sets[i], setInts[i], bw, points, quantiles[i], &stats[i], error)) return *error = where + ": " + *error, false;
        // --- violin
        bool vertical = true;
        if (arg("vert")) {
            bool b = true;
            if (!flag("vert", true, &b)) return false;
            vertical = b;  // ('vertical' if vert else 'horizontal')
        } else if (const Json* o = arg("orientation")) {
            if (!o->isString() || (o->asString() != "horizontal" && o->asString() != "vertical"))
                return *error = where + ": " + (o->isString() ? "'" + o->asString() + "'" : std::string("that")) +
                                " is not a valid value for orientation; supported values are 'horizontal', 'vertical'",
                       false;
            vertical = o->asString() == "vertical";
        }
        std::vector<double> positions, widths;
        if (const Json* p = arg("positions")) {
            if (!numbers(*p, &positions)) return *error = where + ": positions must be numbers", false;
            if (positions.size() != n) return *error = where + ": List of violinplot statistics and `positions` values must have the same length", false;
        } else {
            for (std::size_t i = 0; i < n; ++i) positions.push_back(double(i + 1));
        }
        if (const Json* w = arg("widths")) {
            if (readNumber(*w, &one)) widths.assign(n, one);
            else if (!numbers(*w, &widths)) return *error = where + ": widths must be numbers", false;
            else if (widths.size() != n) return *error = where + ": List of violinplot statistics and `widths` values must have the same length", false;
        } else {
            widths.assign(n, 0.5);
        }
        std::string side = "both";
        if (const Json* s = arg("side")) {
            if (!s->isString() || (s->asString() != "both" && s->asString() != "low" && s->asString() != "high"))
                return *error = where + ": " + (s->isString() ? "'" + s->asString() + "'" : std::string("that")) +
                                " is not a valid value for side; supported values are 'both', 'low', 'high'",
                       false;
            side = s->asString();
        }
        const bool lowSide = side == "both" || side == "low", highSide = side == "both" || side == "high";
        std::vector<double> lineLow(n), lineHigh(n);
        for (std::size_t i = 0; i < n; ++i) {
            lineLow[i] = (lowSide ? -0.25 : 0.0) * widths[i] + positions[i];
            lineHigh[i] = (highSide ? 0.25 : 0.0) * widths[i] + positions[i];
        }
        // cycle_color: the rows of to_rgba_array(color) over and over (or 'none' without any), one per violin
        const auto cycleColor = [&](const Json& c, std::vector<Json>* out) {
            std::vector<colors::Spec> list;
            colors::Spec single;
            if (color(c, &single)) list = {single};
            else if (!colorList(c, &list)) return *error = where + ": that is not a colour or a list of colours", false;
            std::vector<Json> rows;
            for (const auto& item : list) {
                if (colors::isNone(item) && list.size() == 1) break;  // ('none': no rows)
                const auto rgba = colors::toRgba(item, std::nullopt, rc_, nullptr);
                if (!rgba) return *error = where + ": that is not a colour or a list of colours", false;
                rows.push_back(colorJson(std::vector<double>(rgba->begin(), rgba->end())));
            }
            out->clear();
            for (std::size_t i = 0; i < n; ++i) out->push_back(rows.empty() ? Json::string("none") : rows[i % rows.size()]);
            return true;
        };
        const bool classic = rc_.boolean("_internal.classic_mode");
        const Json* facecolor = arg("facecolor");
        const Json* linecolor = arg("linecolor");
        Json nextColor;
        if ((!facecolor || !linecolor) && !classic) nextColor = colorJson(ax.nextLineColor());
        std::vector<Json> faces, lineColors;
        std::optional<double> bodyAlpha;
        if (facecolor) {
            if (!cycleColor(*facecolor, &faces)) return false;
        } else {
            bodyAlpha = 0.3;
            if (!cycleColor(classic ? Json::string("y") : nextColor, &faces)) return false;
        }
        // (the classic style edges the bodies with ("k", 0.3), a colour and an alpha: not supported yet)
        if (classic) return *error = where + ": violin plots in the classic style are not supported yet", false;
        const Json bodyEdge;  // None
        if (!cycleColor(linecolor ? *linecolor : classic ? Json::string("r") : nextColor, &lineColors)) return false;
        std::vector<Json> colorsList(lineColors.begin(), lineColors.end());
        const Json lineColorsJson = Json::array(colorsList);
        // the bodies
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<double> vals = stats[i].vals;
            if (!vals.empty()) {
                double top = vals[0];  // vals.max(): NaN wins
                for (double v : vals) top = std::isnan(top) || std::isnan(v) ? std::nan("") : std::max(top, v);
                for (double& v : vals) v = 0.5 * widths[i] * v / top;
            }
            std::vector<double> lo, hi;
            for (double v : vals) lo.push_back(-v + positions[i]), hi.push_back(v + positions[i]);
            model::Call f;
            f.on = call.on;
            f.method = vertical ? "fill_betweenx" : "fill_between";
            f.args = {numbersJson(stats[i].coords), lowSide ? numbersJson(lo) : Json::real(positions[i]),
                      highSide ? numbersJson(hi) : Json::real(positions[i])};
            f.kwargs = {{"facecolor", faces[i]}, {"edgecolor", bodyEdge}, {"alpha", bodyAlpha ? Json::real(*bodyAlpha) : Json()}};
            if (!fillBetween(ax, f, error)) return false;
        }
        // the lines across (perp_lines) and along (par_lines)
        const auto lines = [&](bool across, const std::vector<double>& at, const std::vector<double>& from, const std::vector<double>& to) {
            model::Call l;
            l.on = call.on;
            l.method = (across == vertical) ? "hlines" : "vlines";
            l.args = {numbersJson(at), numbersJson(from), numbersJson(to)};
            l.kwargs = {{"colors", lineColorsJson}};
            if (side != "both") l.kwargs.emplace_back("capstyle", Json::string("projecting"));
            return this->lines(ax, l, error);
        };
        std::vector<double> means, mins, maxes, medians, qs, qLow, qHigh;
        for (std::size_t i = 0; i < n; ++i) {
            means.push_back(stats[i].mean), mins.push_back(stats[i].min), maxes.push_back(stats[i].max), medians.push_back(stats[i].median);
            for (double q : stats[i].quantiles) qs.push_back(q), qLow.push_back(lineLow[i]), qHigh.push_back(lineHigh[i]);
        }
        bool showmeans = false, showextrema = true, showmedians = false;
        if (!flag("showmeans", false, &showmeans) || !flag("showextrema", true, &showextrema) || !flag("showmedians", false, &showmedians))
            return false;
        if (showmeans && !lines(true, means, lineLow, lineHigh)) return false;
        if (showextrema && (!lines(true, maxes, lineLow, lineHigh) || !lines(true, mins, lineLow, lineHigh) || !lines(false, positions, mins, maxes)))
            return false;
        if (showmedians && !lines(true, medians, lineLow, lineHigh)) return false;
        if (!qs.empty() && !lines(true, qs, qLow, qHigh)) return false;
        return true;
    }

    // Axes.pie(x, explode=None, labels=None, colors=None, autopct=None, pctdistance=0.6, shadow=False,
    // labeldistance=1.1, startangle=0, radius=1, counterclock=True, wedgeprops=None, textprops=None,
    // center=(0, 0), frame=False, rotatelabels=False, *, normalize=True, hatch=None): an equal aspect,
    // one Wedge per value (and its Shadow), the labels and percentages by pie_label, and without a frame
    // no ticks and limits of 1.25 around the centre
    bool pie(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".pie";
        // (pie sets the aspect 'equal', which twinned Axes would meet by their data limits: not ported)
        if (ax.twinned) return *error = where + ": a pie on twinned Axes (adjustable 'datalim') is not supported yet", false;
        static const char* const names[] = {"x", "explode", "labels", "colors", "autopct", "pctdistance", "shadow", "labeldistance",
                                            "startangle", "radius", "counterclock", "wedgeprops", "textprops", "center", "frame",
                                            "rotatelabels", "normalize", "hatch"};
        constexpr std::size_t count = sizeof names / sizeof names[0];
        if (call.args.size() > 16) return *error = where + ": too many positional arguments", false;
        for (const auto& kw : call.kwargs)
            if (kw.first != "data" && std::find(std::begin(names), std::end(names), kw.first) == std::end(names))
                return *error = where + ": got an unexpected keyword argument '" + kw.first + "'", false;
        if (const Json* d = keyword(call, "data"); d && !d->isNull()) return *error = where + ": data is not supported yet", false;
        const auto arg = [&](const char* name) -> const Json* {
            for (std::size_t i = 0; i < count; ++i)
                if (std::string(names[i]) == name) {
                    const Json* v = argument(call, i, name);
                    return v && !v->isNull() ? v : nullptr;
                }
            return nullptr;
        };
        double one = 0;
        const auto number = [&](const char* name, double fallback, double* out) {
            const Json* v = arg(name);
            if (!v) return *out = fallback, true;
            if (!v->isNumber() || !readNumber(*v, out)) return *error = where + ": " + name + " must be a number", false;
            return true;
        };
        const auto flag = [&](const char* name, bool fallback, bool* out) {
            const Json* v = arg(name);
            if (!v) return *out = fallback, true;
            if (v->isBool()) return *out = v->asBool(), true;
            if (readNumber(*v, &one)) return *out = one != 0, true;
            return *error = where + ": " + name + " must be True or False", false;
        };
        for (const char* props : {"wedgeprops", "textprops"})
            if (arg(props)) return *error = where + ": " + props + " is not supported yet", false;
        // set_aspect('equal')
        ax.aspect = 1.0;
        const Json* xJson = arg("x");
        std::vector<double> x;
        if (!xJson || !numbers(*xJson, &x)) return *error = where + ": x must be a list of numbers", false;
        for (double v : x)
            if (v < 0) return *error = where + ": Wedge sizes 'x' must be non negative values", false;
        for (double v : x)
            if (!std::isfinite(v)) return *error = where + ": Wedge sizes must be finite numbers", false;
        const double sx = support::npSum(x);  // (exact for integers too)
        if (sx == 0) return *error = where + ": All wedge sizes are zero", false;
        bool normalize = true;
        if (!flag("normalize", true, &normalize)) return false;
        std::vector<double> fracs;
        if (normalize) {
            for (double v : x) fracs.push_back(v / sx);
        } else if (sx > 1) {
            return *error = where + ": Cannot plot an unnormalized pie with sum(x) > 1", false;
        } else {
            fracs = x;
        }
        std::vector<std::string> labels(x.size());
        if (const Json* l = arg("labels")) {
            const Json* seq = sequence(*l);
            if (!seq) return *error = where + ": labels must be a list of strings", false;
            labels.clear();
            for (const Json& s : seq->items()) {
                if (!s.isString()) return *error = where + ": labels must be strings (others are not supported yet)", false;
                labels.push_back(s.asString());
            }
        }
        std::vector<double> explode(x.size(), 0.0);
        if (const Json* e = arg("explode"); e && !numbers(*e, &explode)) return *error = where + ": explode must be numbers", false;
        if (x.size() != labels.size())
            return *error = where + ": 'labels' must be of length 'x', not " + std::to_string(labels.size()), false;
        if (x.size() != explode.size())
            return *error = where + ": 'explode' must be of length 'x', not " + std::to_string(explode.size()), false;
        std::vector<Json> colorCycle;
        if (const Json* cs = arg("colors")) {
            const Json* seq = sequence(*cs);
            if (!seq || seq->items().empty()) return *error = where + ": colors must be a list of colours", false;
            colorCycle = seq->items();
        }
        // hatch: itertools.cycle(np.atleast_1d(hatch))
        std::vector<Json> hatches{Json()};
        if (const Json* h = arg("hatch")) {
            if (h->isString()) hatches = {*h};
            else if (const Json* seq = sequence(*h); seq && !seq->items().empty()) hatches = seq->items();
            else return *error = where + ": hatch must be a string or a list of them", false;
        }
        double radius = 1, startangle = 0, pctdistance = 0.6, labeldistance = 1.1;
        if (!number("radius", 1, &radius) || !number("startangle", 0, &startangle) || !number("pctdistance", 0.6, &pctdistance))
            return false;
        if (!(radius > 0)) return *error = where + ": 'radius' must be a positive number, not " + support::pythonRepr(radius), false;
        bool counterclock = true, frame = false, rotatelabels = false;
        if (!flag("counterclock", true, &counterclock) || !flag("frame", false, &frame) || !flag("rotatelabels", false, &rotatelabels))
            return false;
        std::array<double, 2> center{0, 0};
        if (const Json* c = arg("center")) {
            const Json* seq = sequence(*c);
            if (!seq || seq->size() != 2 || !readNumber((*seq)[0], &center[0]) || !readNumber((*seq)[1], &center[1]))
                return *error = where + ": center must be (x, y)", false;
        }
        bool shadow = false;
        if (!flag("shadow", false, &shadow)) return false;
        // the wedges
        double theta1 = startangle / 360;
        std::vector<Wedge*> wedges;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double frac = fracs[i];
            double xPos = center[0], yPos = center[1];
            const double theta2 = counterclock ? theta1 + frac : theta1 - frac;
            const double thetam = 2 * M_PI * 0.5 * (theta1 + theta2);
            xPos += explode[i] * std::cos(thetam);
            yPos += explode[i] * std::sin(thetam);
            auto w = std::make_unique<Wedge>(rc_, xPos, yPos, radius, 360. * std::min(theta1, theta2), 360. * std::max(theta1, theta2));
            // Wedge(..., facecolor=get_next_color(), hatch=next(hatch_cycle), clip_on=False, label=label)
            const Json face = colorCycle.empty() ? colorJson(ax.nextPatchColor()) : colorCycle[i % colorCycle.size()];
            const Json hatch = hatches[i % hatches.size()];
            const Json label = Json::string(labels[i]);
            if (!patchConstructor(*w, {{"facecolor", &face}}, where, error)) return false;
            if (hatch.isString()) w->setHatch(hatch.asString(), true);  // (np.atleast_1d of a str: np.str_)
            w->clipOn = false;
            w->label = labels[i];
            Wedge& added = static_cast<Wedge&>(ax.addPatch(std::move(w)));
            wedges.push_back(&added);
            if (shadow) {
                auto s = std::make_unique<Shadow>(rc_, added, -0.02, -0.02);
                s->label = "_nolegend_";
                ax.addPatch(std::move(s));
            }
            theta1 = theta2;
        }
        // pie_label: a text per wedge at `distance` radii along its middle
        const auto pieLabel = [&](const std::vector<std::string>& texts, double distance, bool outer, bool rotate, const Json* fontsize) {
            for (std::size_t i = 0; i < wedges.size(); ++i) {
                const Wedge& w = *wedges[i];
                const double thetam = 2 * M_PI * 0.5 * (w.theta1 + w.theta2) / 360;
                const double xt = w.cx + distance * w.r * std::cos(thetam);
                const double yt = w.cy + distance * w.r * std::sin(thetam);
                std::string ha = outer ? (xt > 0 ? "left" : "right") : "center", va = "center";
                Json rotation = Json::string("horizontal");
                if (rotate) {
                    if (outer) va = yt > 0 ? "bottom" : "top";
                    rotation = Json::real(thetam * (180.0 / M_PI) + (xt > 0 ? 0 : 180));  // np.rad2deg
                }
                model::Call tc;
                tc.on = call.on;
                tc.method = "text";
                tc.args = {Json::real(xt), Json::real(yt), Json::string(texts[i])};
                tc.kwargs = {{"clip_on", Json::boolean(false)}, {"rotation", rotation}, {"horizontalalignment", Json::string(ha)},
                             {"verticalalignment", Json::string(va)}};
                if (fontsize) tc.kwargs.emplace_back("fontsize", *fontsize);  // t.set(**textprops)
                if (!axesText(ax, tc, error)) return false;
            }
            return true;
        };
        // labeldistance None: no labels
        const Json* ld = argument(call, 7, "labeldistance");
        if (!(ld && ld->isNull())) {
            if (!number("labeldistance", 1.1, &labeldistance)) return false;
            if (!pieLabel(labels, labeldistance, true, rotatelabels, &rc_.value("xtick.labelsize"))) return false;
        }
        if (const Json* autopct = arg("autopct")) {
            if (!autopct->isString()) return *error = where + ": autopct must be a format string (a callable is not supported)", false;
            std::vector<std::string> texts;
            for (double frac : fracs) {
                const auto f = support::percentFormat(autopct->asString(), 100. * frac, false);
                if (f.kind != support::Formatted::Text)
                    return *error = where + ": " + (f.kind == support::Formatted::Raised ? f.text : "this autopct is not supported yet"), false;
                texts.push_back(f.text);
            }
            if (!pieLabel(texts, pctdistance, false, false, nullptr)) return false;
        }
        if (frame) {
            ax.requestAutoscaleView(true, true);
        } else {
            // set(frame_on=False, xticks=[], yticks=[], xlim=..., ylim=...)
            ax.frameOn = false;
            if (!ax.xaxis->setTicks({}, std::nullopt, error) || !ax.yaxis->setTicks({}, std::nullopt, error)) return false;
            for (int k = 0; k < 2; ++k) {
                model::Call lim;
                lim.on = call.on;
                lim.method = k == 0 ? "set_xlim" : "set_ylim";
                lim.args = {Json::array({Json::real(-1.25 + center[std::size_t(k)]), Json::real(1.25 + center[std::size_t(k)])})};
                if (!axesCall(ax, lim, error)) return false;
            }
        }
        return true;
    }

    // Axes.step(x, y, *args, where='pre', data=None, **kwargs): plot with drawstyle 'steps-<where>'
    bool step(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".step";
        if (call.args.size() < 2) return *error = where + ": x and y are required", false;
        std::string place = "pre";
        model::Call c = call;
        c.method = "plot";
        c.kwargs.clear();
        for (const auto& [name, value] : call.kwargs) {
            if (name == "where") {
                if (!value.isString() || (value.asString() != "pre" && value.asString() != "post" && value.asString() != "mid"))
                    return *error = where + ": " + (value.isString() ? "'" + value.asString() + "'" : std::string("that")) +
                                    " is not a valid value for where; supported values are 'pre', 'post', 'mid'",
                           false;
                place = value.asString();
            } else if (name == "data") {
                if (!value.isNull()) return *error = where + ": data is not supported yet", false;
            } else {
                c.kwargs.emplace_back(name, value);
            }
        }
        // kwargs['drawstyle'] = 'steps-' + where: in place of a drawstyle given, else last
        const Json drawstyle = Json::string("steps-" + place);
        auto slot = std::find_if(c.kwargs.begin(), c.kwargs.end(), [](const auto& kw) { return kw.first == "drawstyle"; });
        if (slot != c.kwargs.end()) slot->second = drawstyle;
        else c.kwargs.emplace_back("drawstyle", drawstyle);
        return plot(ax, c, error);
    }

    // Axes.stairs(values, edges=None, *, orientation='vertical', baseline=0, fill=False, **kwargs): a
    // StepPatch in the colour given or the lines' next one (as its edge, or its face when filled)
    bool stairs(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".stairs";
        const Json* valuesJson = argument(call, 0, "values");
        if (!valuesJson) return *error = where + ": values is required", false;
        std::vector<double> values, edges;
        double one = 0;
        if (readNumber(*valuesJson, &one)) values = {one};
        else if (!numbers(*valuesJson, &values)) return *error = where + ": values must be numbers", false;
        if (const Json* e = argument(call, 1, "edges"); e && !e->isNull()) {
            if (readNumber(*e, &one)) edges = {one};
            else if (!numbers(*e, &edges)) return *error = where + ": edges must be numbers", false;
        } else {
            for (std::size_t i = 0; i <= values.size(); ++i) edges.push_back(double(i));  // np.arange(len(values) + 1)
        }
        bool vertical = true;
        if (const Json* o = keyword(call, "orientation")) {
            if (!o->isString()) return *error = where + ": orientation must be a string", false;
            vertical = o->asString() == "vertical";  // (anything else is horizontal)
        }
        StepBaseline baseline;
        if (const Json* b = keyword(call, "baseline")) {
            if (b->isNull()) baseline.kind = StepBaseline::None;
            else if (readNumber(*b, &one)) baseline.values = {one};
            else if (numbers(*b, &baseline.values)) baseline.kind = StepBaseline::Array;
            else return *error = where + ": Invalid `baseline` specified", false;
        }
        bool fill = false;
        static const Json no = Json::boolean(false);
        const Json* fillJson = keyword(call, "fill");
        if (!fillJson) fillJson = &no;
        if (fillJson->isNull() || fillJson->isBool()) fill = !fillJson->isNull() && fillJson->asBool();
        else if (readNumber(*fillJson, &one)) fill = one != 0;
        else return *error = where + ": fill must be True or False", false;
        // the colour: color= (popped, even None), else the lines' next one
        std::vector<std::pair<std::string, const Json*>> kwargs;
        Json color;
        bool colorGiven = false;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "values" || name == "edges" || name == "orientation" || name == "baseline" || name == "fill") continue;
            if (name == "color") {
                color = value;
                colorGiven = true;
                continue;
            }
            kwargs.emplace_back(name, &value);
        }
        if (!colorGiven) color = colorJson(ax.nextLineColor());
        static const Json zero = Json::integer(0);
        const auto setdefault = [&](const std::string& name, const Json* value) {
            if (std::none_of(kwargs.begin(), kwargs.end(), [&](const auto& kw) { return kw.first == name; })) kwargs.emplace_back(name, value);
        };
        if (fill) {
            setdefault("linewidth", &zero);
            setdefault("facecolor", &color);
        } else {
            setdefault("edgecolor", &color);
        }
        Path path;
        if (!stepPatchPath(values, edges, baseline, vertical, rc_, &path, error)) return *error = where + ": " + *error, false;
        auto patch = std::make_unique<StepPatch>(rc_, path);
        kwargs.emplace(kwargs.begin(), "fill", fillJson);  // (StepPatch(..., fill=fill, **kwargs))
        if (!patchConstructor(*patch, kwargs, where, error)) return false;
        Patch& added = ax.addPatch(std::move(patch));
        if (baseline.kind != StepBaseline::None) {
            // np.min(baseline): NaN wins
            double low = baseline.values[0];
            for (double b : baseline.values) low = std::isnan(low) || std::isnan(b) ? std::nan("") : std::min(low, b);
            (vertical ? added.stickyY : added.stickyX).push_back(low);
            ax.updateDatalim({vertical ? std::array<double, 2>{edges[0], low} : std::array<double, 2>{low, edges[0]}});
        }
        ax.requestAutoscaleView(true, true);
        return true;
    }
    bool polygon(const model::Call& call, std::string* error) {
        const std::string where = "plt.Polygon";
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        const Json* xyValue = argument(call, 0, "xy");
        if (!xyValue) return *error = where + ": xy is required", false;
        std::vector<double> xy;
        if (!points(*xyValue, &xy)) return *error = where + ": xy must be a list of (x, y) points", false;
        std::optional<bool> closed = true;
        if (const Json* v = keyword(call, "closed")) {
            double n = 0;
            if (v->isNull()) closed.reset();
            else if (v->isBool()) closed = v->asBool();
            else if (readNumber(*v, &n)) closed = n != 0;
            else return *error = where + ": closed must be True, False or None", false;
        }
        auto poly = std::make_unique<Polygon>(rc_, std::move(xy), closed);
        std::vector<std::pair<std::string, const Json*>> kwargs;
        for (const auto& [name, v] : call.kwargs)
            if (name != "xy" && name != "closed") kwargs.emplace_back(name, &v);
        if (!patchConstructor(*poly, kwargs, where, error)) return false;
        if (!call.as.empty()) {
            patches_[call.as[0]] = std::move(poly);
            addedPatches_.erase(call.as[0]);
        }
        return true;
    }


    // The classes of matplotlib.colors a figure file makes on "colors": norms, each kept under its "as" name.
    // np.asarray(triangles, dtype=np.int32) of a figure file's value, of shape (N, 3): false when it is not one
    bool triangleRows(const Json& v, tri::Triangulation::Triangles* out) {
        const Json* rows = sequence(v);
        if (!rows || rows->size() == 0) return false;
        out->clear();
        for (const Json& r : rows->items()) {
            const Json* row = sequence(r);
            if (!row || row->size() != 3) return false;
            std::array<int, 3> t{};
            for (std::size_t k = 0; k < 3; ++k) {
                const Json& n = (*row)[k];
                if (!n.isNumber() || !n.isInteger()) return false;  // (other values are not taken yet)
                double d = 0;
                if (!readNumber(n, &d) || d < -2147483648.0 || d > 2147483647.0) return false;
                t[k] = int(d);
            }
            out->push_back(t);
        }
        return true;
    }

    // np.asarray(mask, dtype=bool)
    bool maskValues(const Json& v, std::vector<bool>* out, const std::string& where, std::string* error) {
        const Json* seq = sequence(v);
        if (!seq) return *error = where + ": mask must be a list of booleans", false;
        out->clear();
        for (const Json& m : seq->items()) {
            double d = 0;
            if (m.isBool()) out->push_back(m.asBool());
            else if (readNumber(m, &d)) out->push_back(d != 0);
            else return *error = where + ": mask must be a list of booleans", false;
        }
        return true;
    }

    // mtri.Triangulation(x, y, triangles=None, mask=None) (import matplotlib.tri as mtri), named by "as"
    bool triCall(const model::Call& call, std::string* error) {
        const std::string where = "mtri." + call.method;
        if (call.method != "Triangulation") return *error = where + " is not supported yet", false;
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        if (call.args.size() > 4) return *error = where + ": too many positional arguments", false;
        if (!keywords(call, {"x", "y", "triangles", "mask"}, error)) return false;
        const Json* xs = argument(call, 0, "x");
        const Json* ys = argument(call, 1, "y");
        std::vector<double> x, y;
        if (!xs || !ys || !numbers(*xs, &x) || !numbers(*ys, &y)) return *error = where + ": x and y must be lists of numbers", false;
        std::optional<tri::Triangulation::Triangles> triangles;
        if (const Json* t = argument(call, 2, "triangles"); t && !t->isNull()) {
            tri::Triangulation::Triangles rows;
            if (!triangleRows(*t, &rows)) return *error = where + ": triangles must be a (N, 3) int array", false;
            triangles = std::move(rows);
        }
        std::optional<std::vector<bool>> mask;
        if (const Json* m = argument(call, 3, "mask"); m && !m->isNull()) {
            std::vector<bool> values;
            if (!maskValues(*m, &values, where, error)) return false;
            mask = std::move(values);
        }
        auto t = std::make_shared<tri::Triangulation>();
        if (!tri::Triangulation::make(std::move(x), std::move(y), std::move(triangles), std::move(mask), t.get(), error))
            return *error = where + ": " + *error, false;
        if (!call.as.empty()) triangulations_[call.as[0]] = t;
        return true;
    }

    // Triangulation.get_from_args_and_kwargs: a triangulation the file made ({"$ref": name}) or one of x, y,
    // [triangles] and the keywords triangles and mask; the arguments it leaves (`rest`, from `next`) and the other
    // keywords
    bool triangulationOf(const model::Call& call, const std::string& where, std::shared_ptr<tri::Triangulation>* out,
                         std::size_t* next, std::string* error) {
        if (call.args.empty()) return *error = where + ": the triangulation or x and y are required", false;
        const Json& first = call.args[0];
        if (first.isObject() && first.members().size() == 1 && first.members()[0].first == "$ref") {
            const auto it = triangulations_.find(first.members()[0].second.asString());
            if (it == triangulations_.end()) return *error = where + ": the triangulation must be one the figure file named", false;
            *out = it->second;
            *next = 1;  // (triangles and mask keywords have no effect then: a warning)
            return true;
        }
        if (call.args.size() < 2) return *error = where + ": x and y are required", false;
        std::vector<double> x, y;
        if (!numbers(call.args[0], &x) || !numbers(call.args[1], &y)) return *error = where + ": x and y must be lists of numbers", false;
        *next = 2;
        std::optional<tri::Triangulation::Triangles> triangles;
        tri::Triangulation::Triangles rows;
        if (const Json* t = keyword(call, "triangles"); t && !t->isNull()) {
            if (triangleRows(*t, &rows)) triangles = rows;
        } else if (call.args.size() > 2 && triangleRows(call.args[2], &rows)) {
            triangles = rows;
            *next = 3;
        }
        std::optional<std::vector<bool>> mask;
        if (const Json* m = keyword(call, "mask"); m && !m->isNull()) {
            std::vector<bool> values;
            if (!maskValues(*m, &values, where, error)) return false;
            mask = std::move(values);
        }
        *out = std::make_shared<tri::Triangulation>();
        if (!tri::Triangulation::make(std::move(x), std::move(y), std::move(triangles), std::move(mask), out->get(), error))
            return *error = where + ": " + *error, false;
        return true;
    }

    // triplot(triangulation or x, y, [triangles], [fmt], *, mask=, **kwargs): the edges as one line broken by NaN
    // (Path's default zorder 1, no markers), then the points as markers (no line, no label)
    bool triplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        std::shared_ptr<tri::Triangulation> t;
        std::size_t next = 0;
        if (!triangulationOf(call, where, &t, &next, error)) return false;
        if (call.args.size() > next + 1) return *error = where + ": only a format string may follow the triangulation", false;
        std::string fmt;
        if (call.args.size() > next) {
            if (!call.args[next].isString()) return *error = where + ": the format must be a string", false;
            fmt = call.args[next].asString();
        }
        std::optional<std::string> linestyle, marker;
        std::optional<colors::Spec> color;
        if (!plotFormat(fmt, &linestyle, &marker, &color, error)) return *error = where + ": " + *error, false;
        // normalize_kwargs(kwargs, Line2D), then the format's parts where not given (a dict: keys keep their place)
        std::vector<std::pair<std::string, Json>> kw;
        const auto put = [&](const std::string& key, const Json& value, bool replace) {
            for (auto& [k, v] : kw)
                if (k == key) {
                    if (replace) v = value;
                    return;
                }
            kw.emplace_back(key, value);
        };
        for (const auto& [name, value] : call.kwargs) {
            if (name == "triangles" || name == "mask" || name == "x" || name == "y") continue;
            const std::string full = fullName(name, lineAliases());
            for (const auto& [k, v] : kw)
                if (k == full) return *error = where + ": Got both '" + full + "' and an alias of it, which are aliases of one another", false;
            kw.emplace_back(full, value);
        }
        put("linestyle", Json::string(*linestyle), false);
        put("marker", Json::string(*marker), false);
        if (color) put("color", colorJson(*color), false);
        const auto valueOf = [&](const char* key) -> const Json* {
            for (const auto& [k, v] : kw)
                if (k == key) return &v;
            return nullptr;
        };
        const auto none = [](const Json* v) {
            return !v || v->isNull() || (v->isString() && (v->asString() == "None" || v->asString().empty() || v->asString() == " "));
        };
        const bool lines = !none(valueOf("linestyle")), markers = !none(valueOf("marker"));
        // the lines
        std::vector<double> lx, ly;
        if (lines) {
            const double nan = std::numeric_limits<double>::quiet_NaN();
            for (const auto& e : t->edges()) {
                lx.insert(lx.end(), {t->x()[std::size_t(e[0])], t->x()[std::size_t(e[1])], nan});
                ly.insert(ly.end(), {t->y()[std::size_t(e[0])], t->y()[std::size_t(e[1])], nan});
            }
        }
        model::Call p;
        p.on = call.on;
        p.method = "plot";
        p.args = {numbersJson(lx), numbersJson(ly)};
        p.kwargs = kw;
        for (auto& [k, v] : p.kwargs)
            if (k == "marker") v = Json::string("None");
        if (const Json* z = valueOf("zorder")) {
            (void)z;
        } else {
            p.kwargs.emplace_back("zorder", Json::integer(1));
        }
        if (!plot(ax, p, error)) return false;
        // the markers
        p.args = markers ? std::vector<Json>{numbersJson(t->x()), numbersJson(t->y())} : std::vector<Json>{numbersJson({}), numbersJson({})};
        p.kwargs.clear();
        for (const auto& [k, v] : kw) {
            if (k == "label") continue;
            p.kwargs.emplace_back(k, k == "linestyle" ? Json::string("None") : v);
        }
        return plot(ax, p, error);
    }

    // tripcolor(triangulation or x, y, [triangles], c, *, alpha=1.0, norm=None, cmap=None, vmin=None, vmax=None,
    // shading='flat', facecolors=None, **kwargs): a PolyCollection of the unmasked triangles coloured by c at the
    // triangles or the mean of c at their points, or a Gouraud-shaded TriMesh of c at the points
    bool tripcolor(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        std::string shading = "flat";
        if (const Json* sh = keyword(call, "shading")) {
            if (!sh->isString() || (sh->asString() != "flat" && sh->asString() != "gouraud"))
                return *error = where + ": " + repr(*sh) + " is not a valid value for shading; supported values are 'flat', 'gouraud'", false;
            shading = sh->asString();
        }
        std::shared_ptr<tri::Triangulation> t;
        std::size_t next = 0;
        if (!triangulationOf(call, where, &t, &next, error)) return false;
        const std::size_t ntri = t->triangles().size(), npoints = t->x().size();
        // the colours: of the faces or of the points
        std::vector<double> pointColors, faceColors;
        bool pointsGiven = false, integral = false;
        const auto values = [&](const Json& v, std::vector<double>* out) {
            if (!numbers(v, out)) return *error = where + ": the colour values must be a list of numbers", false;
            integral = integers(v);
            return true;
        };
        if (const Json* fc = keyword(call, "facecolors"); fc && !fc->isNull()) {
            if (!values(*fc, &faceColors)) return false;  // (positional c has no effect then: a warning)
            if (faceColors.size() != ntri) return *error = where + ": The length of facecolors must match the number of triangles", false;
        } else {
            if (call.args.size() <= next)
                return *error = where + ": tripcolor() missing 1 required positional argument: 'c'; or 1 required keyword-only argument: 'facecolors'", false;
            if (call.args.size() > next + 1) return *error = where + ": Unexpected positional parameters", false;
            std::vector<double> c;
            if (!values(call.args[next], &c)) return false;
            if (c.size() == npoints) pointColors = c, pointsGiven = true;
            else if (c.size() == ntri) faceColors = c;
            else return *error = where + ": The length of c must match either the number of points or the number of triangles", false;
        }
        // linewidths, edgecolors and antialiased as in pcolor
        const Json* ec = keyword(call, "edgecolors");
        if (!ec) ec = keyword(call, "edgecolor");
        if (ec && !ec->isString()) return *error = where + ": edgecolors other than a colour name are not supported yet", false;
        const Json* aa = keyword(call, "antialiased");
        if (!aa) aa = keyword(call, "antialiaseds");
        if (shading == "gouraud" && !pointsGiven)
            return *error = where + ": shading='gouraud' can only be used when the colors are specified at the points, not at the faces.", false;
        std::unique_ptr<Collection> col;
        colors::MaskedArray array;
        const tri::Triangulation::Triangles masked = t->maskedTriangles();
        if (shading == "gouraud") {
            std::vector<std::array<std::array<double, 2>, 3>> corners;
            for (const auto& tr : masked) {
                std::array<std::array<double, 2>, 3> c{};
                for (std::size_t k = 0; k < 3; ++k) c[k] = {t->x()[std::size_t(tr[k])], t->y()[std::size_t(tr[k])]};
                corners.push_back(c);
            }
            col = std::make_unique<TriMesh>(rc_, std::move(corners), masked);
            array.data = pointColors;
            array.dtype = integral ? colors::DType::Int64 : colors::DType::Float64;
        } else {
            std::vector<std::vector<std::array<double, 2>>> verts;
            for (const auto& tr : masked) {
                std::vector<std::array<double, 2>> v;
                for (int k : tr) v.push_back({t->x()[std::size_t(k)], t->y()[std::size_t(k)]});
                verts.push_back(v);
            }
            col = std::make_unique<PolyCollection>(rc_, verts);
            if (pointsGiven) {
                // point_colors[maskedTris].mean(axis=1): ((a + b) + c) / 3 in float64
                for (const auto& tr : masked)
                    array.data.push_back(((pointColors[std::size_t(tr[0])] + pointColors[std::size_t(tr[1])]) + pointColors[std::size_t(tr[2])]) / 3.0);
                array.dtype = colors::DType::Float64;
            } else {
                for (std::size_t i = 0; i < ntri; ++i)
                    if (!t->isMasked(int(i))) array.data.push_back(faceColors[i]);
                array.dtype = integral ? colors::DType::Int64 : colors::DType::Float64;
            }
        }
        // safe_masked_invalid: what is not finite is masked
        array.mask.assign(array.data.size(), false);
        bool anyMasked = false;
        for (std::size_t i = 0; i < array.data.size(); ++i)
            if (!std::isfinite(array.data[i])) array.mask[i] = anyMasked = true;
        if (!anyMasked) array.mask.clear();
        col->array = array;
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &col->norm, keyword(call, "cmap"), keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
        col->cmap = cmap;
        double a = 1.0;
        if (const Json* al = keyword(call, "alpha"); al && !al->isNull() && !readAlpha(*al, &a))
            return *error = where + ": alpha must be a number from 0 to 1", false;
        col->setLinewidths({0.25});
        col->setEdgecolor(ColorArg{ColorArg::None, {}});
        std::string ecLower = ec ? ec->asString() : std::string("none");  // ec.lower()
        for (char& ch : ecLower) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (!aa && ecLower == "none") col->setAntialiaseds({false});
        if (aa) {
            if (!aa->isBool()) return *error = where + ": antialiased must be True or False", false;
            col->setAntialiaseds({aa->asBool()});
        }
        col->setAlpha(a);
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*col, call, {"shading", "antialiased", "antialiaseds", "cmap", "norm", "facecolors", "triangles", "mask", "x", "y", "c"},
                          where, &vmin, &vmax, error))
            return false;
        if (!scaleNorm(*col, normObject, vmin, vmax, where, error)) return false;
        // ax.grid(False); the points' box in the data limits and an autoscale at once; the collection without limits
        ax.xaxis->grid(false, "major", TickSwitches(), false);
        ax.yaxis->grid(false, "major", TickSwitches(), false);
        const double minx = support::npMin(t->x()), maxx = support::npMax(t->x()), miny = support::npMin(t->y()), maxy = support::npMax(t->y());
        ax.updateDatalim({{minx, miny}, {maxx, maxy}});
        ax.requestAutoscaleView(true, true);
        ax.unstaleViewLim();
        ax.addCollection(std::move(col), false);
        return true;
    }

    // stackplot(x, *args, labels=(), colors=None, baseline='zero', **kwargs): the data sets stacked (np.cumsum in
    // float64) over a baseline, each band a fill_between; facecolor (the colours given, else the lines' cycle),
    // edgecolor, hatch and linewidth cycle over the bands (_style_helpers.style_generator)
    bool stackplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.args.size() < 2) return *error = where + ": stackplot() needs x and at least one data set", false;
        if (!call.as.empty()) return *error = where + ": naming stackplot's collections is not supported yet", false;
        std::vector<double> x;
        if (!numbers(call.args[0], &x)) return *error = where + ": x must be a list of numbers", false;
        std::vector<std::vector<double>> y;
        for (std::size_t i = 1; i < call.args.size(); ++i) {
            const Json* seq = sequence(call.args[i]);
            std::vector<double> row;
            if (seq && seq->size() && sequence((*seq)[0])) {  // a 2-D array: its rows
                for (const Json& r : seq->items()) {
                    if (!numbers(r, &row) || row.size() != x.size()) return *error = where + ": the data sets must be as long as x", false;
                    y.push_back(row);
                }
                continue;
            }
            if (!numbers(call.args[i], &row) || row.size() != x.size()) return *error = where + ": the data sets must be as long as x", false;
            y.push_back(row);
        }
        const std::size_t m = y.size(), n = x.size();
        std::vector<std::string> labels;
        std::string baseline = "zero";
        const Json* colorsJson = nullptr;
        std::vector<std::pair<std::string, Json>> rest;
        std::vector<std::pair<std::string, std::vector<Json>>> cycles;  // the style keywords, in their order
        for (const auto& [rawName, value] : call.kwargs) {
            if (rawName == "labels") {
                const Json* seq = sequence(value);
                if (!seq) return *error = where + ": labels must be a list of strings", false;
                for (const Json& l : seq->items()) {
                    if (!l.isString()) return *error = where + ": labels must be strings", false;
                    labels.push_back(l.asString());
                }
                continue;
            }
            if (rawName == "colors") {
                if (!value.isNull()) colorsJson = &value;
                continue;
            }
            if (rawName == "baseline") {
                if (!value.isString() || (value.asString() != "zero" && value.asString() != "sym" && value.asString() != "wiggle" &&
                                          value.asString() != "weighted_wiggle"))
                    return *error = where + ": " + repr(value) + " is not a valid value for baseline; supported values are 'zero', 'sym', 'wiggle', 'weighted_wiggle'", false;
                baseline = value.asString();
                continue;
            }
            const std::string name = fullName(rawName, collectionAliases());  // normalize_kwargs(kwargs, PolyCollection)
            if (name == "linestyle") return *error = where + ": linestyle in stackplot is not supported yet", false;
            if (name != "facecolor" && name != "edgecolor" && name != "hatch" && name != "linewidth") {
                rest.emplace_back(name, value);
                continue;
            }
            std::vector<Json> items;
            std::string lower = value.isString() ? value.asString() : "";
            for (char& ch : lower) ch = char(std::tolower(static_cast<unsigned char>(ch)));
            const Json* seq = value.isString() ? nullptr : sequence(value);
            if (seq && seq->items().empty()) return *error = where + ": " + name + " must not be an empty sequence", false;
            if ((name == "facecolor" || name == "edgecolor") && !(value.isNull() || lower == "none")) {
                std::vector<colors::Spec> list;  // itertools.cycle(to_rgba_array(value))
                if (!colorList(value, &list)) return *error = where + ": " + name + " is not a colour", false;
                for (const auto& c : list) {
                    const auto rgba = colors::toRgba(c, std::nullopt, rc_, nullptr);
                    if (!rgba) return *error = where + ": " + name + " is not a colour", false;
                    items.push_back(colorJson(std::vector<double>(rgba->begin(), rgba->end())));
                }
            } else if ((name == "hatch" || name == "linewidth") && seq) {
                items = seq->items();  // itertools.cycle(np.atleast_1d(value))
            } else {
                items = {value};
            }
            cycles.emplace_back(name, items);
        }
        // colors=None: one colour of the lines' cycle per data set; facecolor defaults to the colours
        bool faceGiven = false;
        for (const auto& c : cycles) faceGiven = faceGiven || c.first == "facecolor";
        std::vector<colors::Spec> palette;
        if (colorsJson) {
            if (!colorList(*colorsJson, &palette)) return *error = where + ": colors must be colours", false;
        } else {
            for (std::size_t i = 0; i < m; ++i) palette.push_back(ax.nextLineColor());
        }
        if (!faceGiven) {
            std::vector<Json> items;
            for (const auto& c : palette) {
                const auto rgba = colors::toRgba(c, std::nullopt, rc_, nullptr);
                if (!rgba) return *error = where + ": colors must be colours", false;
                items.push_back(colorJson(std::vector<double>(rgba->begin(), rgba->end())));
            }
            if (items.empty()) return *error = where + ": facecolor must not be an empty sequence", false;
            cycles.emplace_back("facecolor", items);  // (kwargs.setdefault: after the others)
        }
        // the stack and its first line
        std::vector<std::vector<double>> stack = y;
        for (std::size_t i = 1; i < m; ++i)
            for (std::size_t j = 0; j < n; ++j) stack[i][j] = stack[i - 1][j] + y[i][j];
        std::vector<double> firstLine(n, 0.0);
        bool scalarFirst = baseline == "zero";
        if (baseline == "sym") {
            for (std::size_t j = 0; j < n; ++j) {
                double sum = 0;
                for (std::size_t i = 0; i < m; ++i) sum += y[i][j];
                firstLine[j] = -sum * 0.5;
            }
        } else if (baseline == "wiggle") {
            for (std::size_t j = 0; j < n; ++j) {
                double sum = 0;
                for (std::size_t i = 0; i < m; ++i) sum += y[i][j] * ((double(m) - 0.5) - double(i));
                firstLine[j] = sum / -double(m);
            }
        } else if (baseline == "weighted_wiggle") {
            std::vector<double> total(n, 0.0), inv(n, 0.0), centerSum(n, 0.0);
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) total[j] += y[i][j];
                if (total[j] > 0) inv[j] = 1.0 / total[j];
            }
            for (std::size_t i = 0; i < m; ++i)
                for (std::size_t j = 0; j < n; ++j) {
                    const double increase = j == 0 ? y[i][0] : y[i][j] - y[i][j - 1];
                    double below = total[j] - stack[i][j];
                    below += 0.5 * y[i][j];
                    const double moveUp = j == 0 ? 0.5 : below * inv[j];
                    centerSum[j] += (moveUp - 0.5) * increase;  // center.sum(0)
                }
            double acc = 0;
            for (std::size_t j = 0; j < n; ++j) {
                acc += centerSum[j];  // np.cumsum
                firstLine[j] = acc - 0.5 * total[j];
            }
        }
        if (!scalarFirst)
            for (auto& row : stack)
                for (std::size_t j = 0; j < n; ++j) row[j] += firstLine[j];
        // the bands
        for (std::size_t i = 0; i < m; ++i) {
            model::Call f;
            f.on = call.on;
            f.method = "fill_between";
            f.args = {numbersJson(x), i == 0 ? (scalarFirst ? Json::real(0.0) : numbersJson(firstLine)) : numbersJson(stack[i - 1]),
                      numbersJson(stack[i])};
            if (i < labels.size()) f.kwargs.emplace_back("label", Json::string(labels[i]));
            for (const auto& [name, items] : cycles) f.kwargs.emplace_back(name, items[i % items.size()]);
            for (const auto& kw : rest) f.kwargs.push_back(kw);
            if (!fillBetween(ax, f, error)) return false;
            if (i == 0) ax.children.back()->stickyY = {0.0};  // coll.sticky_edges.y[:] = [0]
        }
        return true;
    }

    // broken_barh(xranges, yrange, align='bottom', **kwargs): one PolyCollection of a rectangle per (x0, width) at
    // (ypos, height), the height standing on, centred on or hanging from ypos
    bool brokenBarh(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.args.size() > 2) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming broken_barh's collection is not supported yet", false;
        const Json* xr = argument(call, 0, "xranges");
        const Json* yr = argument(call, 1, "yrange");
        if (!xr || !yr) return *error = where + ": broken_barh() missing required arguments 'xranges' and 'yrange'", false;
        std::vector<double> yv;
        if (!numbers(*yr, &yv) || yv.size() != 2) return *error = where + ": yrange must be (ypos, height)", false;
        std::string align = "bottom";
        if (const Json* a = keyword(call, "align")) {
            if (!a->isString() || (a->asString() != "bottom" && a->asString() != "center" && a->asString() != "top"))
                return *error = where + ": " + repr(*a) + " is not a valid value for align; supported values are 'bottom', 'center', 'top'", false;
            align = a->asString();
        }
        double y0 = yv[0], y1 = 0;
        const double dy = yv[1];
        if (align == "bottom") {
            y1 = y0 + dy;
        } else if (align == "center") {
            y1 = y0 + dy / 2;
            y0 = y0 - dy / 2;
        } else {
            y1 = y0;
            y0 = y0 - dy;
        }
        const Json* seq = sequence(*xr);
        if (!seq) return *error = where + ": xranges must be a sequence of (x0, width) pairs", false;
        std::vector<std::vector<std::array<double, 2>>> verts;
        for (const Json& r : seq->items()) {
            std::vector<double> pair;
            if (!numbers(r, &pair) || pair.size() != 2)
                return *error = where + ": each range in xrange must be a sequence with two elements (i.e. xrange must be an (N, 2) array)", false;
            const double x0 = pair[0], x1 = pair[0] + pair[1];
            verts.push_back({{x0, y0}, {x0, y1}, {x1, y1}, {x1, y0}});
        }
        auto col = std::make_unique<PolyCollection>(rc_, verts);
        // Collection's own parameters (facecolors, edgecolors, linewidths, hatch, zorder) first, then the others
        // through their setters (color sets the face and the edge)
        const auto colours = [&](const Json& v, const std::string& name, std::vector<colors::Spec>* out) {
            colors::Spec one;
            if (color(v, &one)) return *out = {one}, true;
            if (colorList(v, out)) return true;
            return *error = where + ": " + name + " must be a colour or a list of colours", false;
        };
        std::vector<colors::Spec> list;
        model::Call rest;
        std::vector<std::pair<std::string, const Json*>> after;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "xranges" || name == "yrange" || name == "align") continue;
            if (name == "facecolors") {
                if (value.isNull()) continue;
                if (!colours(value, name, &list)) return false;
                col->setFacecolor(ColorArg{ColorArg::Colors, list});
            } else if (name == "hatch") {
                if (!value.isNull() && !value.isString()) return *error = where + ": hatch must be a string", false;
                if (value.isString()) col->hatch = value.asString();
            } else if (name == "color" || name == "facecolor" || name == "fc") {
                after.emplace_back(name, &value);
            } else {
                rest.kwargs.push_back({name, value});
            }
        }
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*col, rest, {}, where, &vmin, &vmax, error)) return false;
        if (vmin || vmax) return *error = where + ": vmin and vmax are not supported yet", false;
        for (const auto& [name, value] : after) {
            if (value->isNull()) continue;
            if (!colours(*value, name, &list)) return false;
            col->setFacecolor(ColorArg{ColorArg::Colors, list});
            if (name == "color") col->setEdgecolor(ColorArg{ColorArg::Colors, list});
        }
        ax.addCollection(std::move(col), true);
        return true;
    }

    // Axes.axline(xy1, xy2=None, *, slope=None, **kwargs): an AxLine in data coordinates, clipped to the Axes, the
    // points it is given in the data limits
    bool infiniteLine(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.args.size() > 2) return *error = where + ": too many positional arguments", false;
        const auto point = [&](const Json* j, std::array<double, 2>* out) {
            std::vector<double> v;
            if (!j || !numbers(*j, &v) || v.size() != 2) return false;
            *out = {v[0], v[1]};
            return true;
        };
        std::array<double, 2> xy1{};
        std::optional<std::array<double, 2>> xy2;
        std::optional<double> slope;
        if (!point(argument(call, 0, "xy1"), &xy1)) return *error = where + ": xy1 must be a point (x, y)", false;
        if (const Json* j = argument(call, 1, "xy2"); j && !j->isNull()) {
            std::array<double, 2> p{};
            if (!point(j, &p)) return *error = where + ": xy2 must be a point (x, y)", false;
            xy2 = p;
        }
        if (const Json* j = keyword(call, "slope"); j && !j->isNull()) {
            double v = 0;
            if (!readNumber(*j, &v)) return *error = where + ": slope must be a number", false;
            slope = v;
        }
        if (slope && (ax.xaxis->scale() != "linear" || ax.yaxis->scale() != "linear"))
            return *error = where + ": 'slope' cannot be used with non-linear scales", false;
        if (!xy2 == !slope) return *error = where + ": Exactly one of 'xy2' and 'slope' must be given", false;
        if (ax.polar) return *error = where + ": axline on polar axes is not supported yet", false;
        auto line = std::make_unique<AxLine>(rc_, xy1, xy2, slope);
        for (const auto& [name, value] :
             ownFirst(call, {"linewidth", "linestyle", "color", "markersize", "markeredgewidth", "markeredgecolor", "markerfacecolor",
                             "antialiased"})) {
            if (name == "xy1" || name == "xy2" || name == "slope") continue;
            if (name == "transform") return *error = where + ": transform is not supported yet", false;
            if (name == "c" && value->isNull()) return *error = where + ": bad color", false;
            if (!lineProperty(*line, fullName(name, lineAliases()), *value, where, error)) return false;
        }
        // like add_line, without the line's own limits
        line->axes = &ax;
        line->transform = ax.transData;
        ax.clipToPatch(*line);
        if (line->label.empty()) line->label = "_child" + std::to_string(ax.children.size());
        ax.children.push_back(std::move(line));
        std::vector<std::array<double, 2>> points{xy1};
        if (xy2) points.push_back(*xy2);
        ax.updateDatalim(points);
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // eventplot(positions, orientation='horizontal', lineoffsets=1, linelengths=1, linewidths=None, colors=None,
    // alpha=None, linestyles='solid', **kwargs): an EventCollection (a LineCollection of the sorted positions'
    // segments) per row of positions, the per-row parameters broadcast as matplotlib does; the data limits by the
    // corners of all the rows
    bool eventplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        static const char* const names[] = {"positions", "orientation", "lineoffsets", "linelengths", "linewidths", "colors", "alpha", "linestyles"};
        if (call.args.size() > std::size(names)) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming eventplot's collections is not supported yet", false;
        const auto get = [&](std::size_t i) -> const Json* {
            const Json* v = argument(call, i, names[i]);
            return v;
        };
        const Json* posJson = get(0);
        if (!posJson) return *error = where + ": eventplot() missing 1 required positional argument: 'positions'", false;
        std::vector<std::vector<double>> positions;
        double one = 0;
        if (readNumber(*posJson, &one)) {
            positions = {{one}};
        } else if (const Json* seq = sequence(*posJson)) {
            bool nested = false;
            for (const Json& e : seq->items()) nested = nested || sequence(e);
            if (nested) {
                for (const Json& e : seq->items()) {
                    std::vector<double> row;
                    if (!numbers(e, &row)) return *error = where + ": positions must be numbers or lists of numbers", false;
                    positions.push_back(row);
                }
            } else {
                std::vector<double> row;
                if (!numbers(*posJson, &row)) return *error = where + ": positions must be numbers or lists of numbers", false;
                positions = {row};
            }
        } else {
            return *error = where + ": positions must be numbers or lists of numbers", false;
        }
        const std::size_t n = positions.size();
        std::string orientation = "horizontal";
        if (const Json* o = get(1)) {
            if (!o->isString() || (o->asString() != "horizontal" && o->asString() != "vertical"))
                return *error = where + ": " + repr(*o) + " is not a valid value for orientation; supported values are 'horizontal', 'vertical'", false;
            orientation = o->asString();
        }
        // a number, or a list of them
        const auto list = [&](const Json* v, double fallback, const char* name, std::vector<double>* out) {
            double x = fallback;
            if (!v || v->isNull()) return *out = {fallback}, true;
            if (readNumber(*v, &x)) return *out = {x}, true;
            if (numbers(*v, out)) return true;
            return *error = where + ": " + name + " must be a number or a list of numbers", false;
        };
        std::vector<double> offsets, lengths, widths, alphas;
        if (!list(get(2), 1, "lineoffsets", &offsets) || !list(get(3), 1, "linelengths", &lengths)) return false;
        // the keyword color / linewidth / linestyle when the parameter is not given (_local_over_kwdict)
        const auto localOver = [&](std::size_t i, const char* kw) -> const Json* {
            const Json* v = get(i);
            if (v && !v->isNull()) return v;
            return keyword(call, kw);
        };
        const Json* widthJson = localOver(4, "linewidth");
        bool widthNone = !widthJson || widthJson->isNull();
        if (!widthNone && !list(widthJson, 0, "linewidths", &widths)) return false;
        if (widthNone) widths = {std::numeric_limits<double>::quiet_NaN()};  // (None: the default width)
        std::vector<std::optional<double>> alpha;
        if (const Json* a = get(6); a && !a->isNull()) {
            if (!list(a, 1, "alpha", &alphas)) return false;
            for (double v : alphas) alpha.push_back(v);
        } else {
            alpha = {std::nullopt};
        }
        std::vector<std::optional<colors::Rgba>> colours;
        if (const Json* c = localOver(5, "color"); c && !c->isNull()) {
            std::vector<colors::Spec> specs;
            if (!colorList(*c, &specs)) return *error = where + ": colors must be a colour or a list of colours", false;
            for (const auto& spec : specs) {
                const auto rgba = colors::toRgba(spec, std::nullopt, rc_, nullptr);
                if (!rgba) return *error = where + ": colors must be colours", false;
                colours.push_back(*rgba);
            }
        } else {
            colours = {std::nullopt};
        }
        std::vector<std::string> styles;
        if (const Json* l = localOver(7, "linestyle"); l && !l->isNull()) {
            if (l->isString()) styles = {l->asString()};
            else if (const Json* seq = sequence(*l)) {
                for (const Json& st : seq->items()) {
                    if (!st.isString()) return *error = where + ": linestyles must be named styles (others are not supported yet)", false;
                    styles.push_back(st.asString());
                }
            } else {
                return *error = where + ": linestyles must be a style or a list of styles", false;
            }
        } else {
            styles = {"solid"};
        }
        if (offsets.empty()) return *error = where + ": lineoffsets cannot be empty", false;
        if (lengths.empty()) return *error = where + ": linelengths cannot be empty", false;
        if (styles.empty()) return *error = where + ": linestyles cannot be empty", false;
        if (widths.empty()) return *error = where + ": linewidths cannot be empty", false;
        if (alpha.empty()) return *error = where + ": alpha cannot be empty", false;
        if (colours.empty()) colours = {std::nullopt};
        // one value of each for every row: lineoffsets 0, d, 2d, ... (np.cumsum of the tiled offset with a first 0)
        if (offsets.size() == 1 && n != 1) {
            std::vector<double> tiled(n, offsets[0]);
            tiled[0] = 0;
            for (std::size_t i = 1; i < n; ++i) tiled[i] += tiled[i - 1];
            offsets = tiled;
        }
        // (each value copied before the vector is filled with it)
        if (lengths.size() == 1) lengths = std::vector<double>(n, lengths[0]);
        if (widths.size() == 1) widths = std::vector<double>(n, widths[0]);
        if (colours.size() == 1) colours = std::vector<std::optional<colors::Rgba>>(n, colours[0]);
        if (alpha.size() == 1) alpha = std::vector<std::optional<double>>(n, alpha[0]);
        if (styles.size() == 1) styles = std::vector<std::string>(n, styles[0]);
        const auto unequal = [&](std::size_t size, const char* name) {
            if (size == n) return false;
            *error = where + ": " + name + " and positions are unequal sized sequences";
            return true;
        };
        if (unequal(offsets.size(), "lineoffsets") || unequal(lengths.size(), "linelengths") || unequal(widths.size(), "linewidths") ||
            unequal(colours.size(), "colors") || unequal(alpha.size(), "alpha") || unequal(styles.size(), "linestyles"))
            return false;
        const bool horizontal = orientation == "horizontal";
        for (std::size_t r = 0; r < n; ++r) {
            std::vector<double> sorted = positions[r];
            std::sort(sorted.begin(), sorted.end(), [](double a, double b) { return !std::isnan(a) && (std::isnan(b) || a < b); });
            std::vector<std::vector<std::array<double, 2>>> segments;
            const double hi = offsets[r] + lengths[r] / 2, lo = offsets[r] - lengths[r] / 2;
            for (double p : sorted) segments.push_back(horizontal ? std::vector<std::array<double, 2>>{{p, hi}, {p, lo}}
                                                                  : std::vector<std::array<double, 2>>{{hi, p}, {lo, p}});
            auto coll = std::make_unique<LineCollection>(rc_, segments);
            coll->groupName = "EventCollection";
            if (!std::isnan(widths[r])) coll->setLinewidths({widths[r]});
            if (!coll->setLinestyles({styles[r]})) return *error = where + ": linestyles other than 'solid', 'dashed', 'dashdot' and 'dotted' are not supported yet", false;
            if (colours[r]) coll->setEdgecolor(ColorArg::of(colors::Spec(std::vector<double>(colours[r]->begin(), colours[r]->end()))));
            coll->setAlpha(alpha[r]);
            // add_collection(autolim=False), then the other keywords
            model::Call rest;
            for (const auto& kw : call.kwargs) {
                bool own = false;
                for (const char* nm : names) own = own || kw.first == nm;
                if (!own && kw.first != "color" && kw.first != "linewidth" && kw.first != "linestyle") rest.kwargs.push_back(kw);
            }
            Collection& added = ax.addCollection(std::move(coll), false);
            std::optional<double> vmin, vmax;
            if (!meshKeywords(added, rest, {}, where, &vmin, &vmax, error)) return false;
            if (vmin || vmax) return *error = where + ": vmin and vmax are not supported yet", false;
        }
        // the corners: the positions' extremes, the offsets' reach by whole lengths
        bool any = false;
        double minpos = 0, maxpos = 0;
        for (const auto& row : positions) {
            if (row.empty()) continue;
            const double mn = support::npMin(row), mx = support::npMax(row);
            minpos = any ? std::min(minpos, mn) : mn;
            maxpos = any ? std::max(maxpos, mx) : mx;
            any = true;
        }
        if (any) {
            double minline = offsets[0] - lengths[0], maxline = offsets[0] + lengths[0];
            for (std::size_t r = 1; r < n; ++r) minline = std::min(minline, offsets[r] - lengths[r]), maxline = std::max(maxline, offsets[r] + lengths[r]);
            if (horizontal) ax.updateDatalim({{minpos, minline}, {maxpos, maxline}});
            else ax.updateDatalim({{minline, minpos}, {maxline, maxpos}});
            ax.requestAutoscaleView(true, true);
        }
        return true;
    }

    // Axes.arrow(x, y, dx, dy, **kwargs): a FancyArrow (width=0.001, length_includes_head=False, head_width=None,
    // head_length=None, shape='full', overhang=0, head_starts_at_zero=False): a closed Polygon of the arrow's outline
    bool arrow(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        static const char* const names[4] = {"x", "y", "dx", "dy"};
        if (call.args.size() > 4) return *error = where + ": arrow() takes 4 positional arguments", false;
        double v[4] = {0, 0, 0, 0};
        for (std::size_t i = 0; i < 4; ++i) {
            const Json* j = argument(call, i, names[i]);
            if (!j) return *error = where + ": arrow() missing required argument '" + names[i] + "'", false;
            if (!readNumber(*j, &v[i])) return *error = where + ": " + names[i] + " must be a number", false;
        }
        const double x = v[0], y = v[1], dx = v[2], dy = v[3];
        double width = 0.001, overhang = 0;
        std::optional<double> headWidth, headLength;
        bool includesHead = false, startsAtZero = false;
        std::string shape = "full";
        std::vector<std::pair<std::string, const Json*>> rest;
        for (const auto& [name, value] : ownFirst(call, {"edgecolor", "facecolor", "color", "linewidth", "linestyle"})) {
            if (name == "x" || name == "y" || name == "dx" || name == "dy") continue;
            double n = 0;
            if (name == "width" || name == "overhang") {
                if (!readNumber(*value, &n)) return *error = where + ": " + name + " must be a number", false;
                (name == "width" ? width : overhang) = n;
            } else if (name == "head_width" || name == "head_length") {
                if (value->isNull()) continue;
                if (!readNumber(*value, &n)) return *error = where + ": " + name + " must be a number", false;
                (name == "head_width" ? headWidth : headLength) = n;
            } else if (name == "length_includes_head" || name == "head_starts_at_zero") {
                if (!value->isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name == "length_includes_head" ? includesHead : startsAtZero) = value->asBool();
            } else if (name == "shape") {
                if (!value->isString()) return *error = where + ": Got unknown shape: " + repr(*value), false;
                shape = value->asString();
            } else {
                rest.emplace_back(name, value);
            }
        }
        // _make_verts
        const double hw = headWidth ? *headWidth : 3 * width;
        const double hl = headLength ? *headLength : 1.5 * hw;
        const double distance = std::hypot(dx, dy);
        const double length = includesHead ? distance : distance + hl;
        std::vector<std::array<double, 2>> left{{0.0, 0.0}, {-hl, -hw / 2}, {-hl * (1 - overhang), -width / 2}, {-length, -width / 2}, {-length, 0}};
        if (!includesHead)
            for (auto& p : left) p[0] += hl;
        if (startsAtZero)
            for (auto& p : left) p[0] += hl / 2;
        std::vector<std::array<double, 2>> coords;
        if (shape == "left") {
            coords = left;
        } else {
            std::vector<std::array<double, 2>> right;
            for (const auto& p : left) right.push_back({p[0], p[1] * -1});
            if (shape == "right") {
                coords = right;
            } else if (shape == "full") {
                coords.assign(left.begin(), left.end() - 1);
                for (std::size_t k = right.size() - 1; k-- > 0;) coords.push_back(right[k]);
            } else {
                return *error = where + ": Got unknown shape: '" + shape + "'", false;
            }
        }
        double cx = 0, sx = 1;
        if (distance != 0) cx = dx / distance, sx = dy / distance;
        // np.dot(coords, [[cx, sx], [-sx, cx]]) as BLAS's dgemm sums (fused, from +0.0), then the tip's position
        std::vector<double> xy;
        for (const auto& p : coords) {
            const double u = std::fma(p[1], -sx, std::fma(p[0], cx, 0.0));
            const double w = std::fma(p[1], cx, std::fma(p[0], sx, 0.0));
            xy.push_back(u + (x + dx));
            xy.push_back(w + (y + dy));
        }
        auto poly = std::make_unique<Polygon>(rc_, xy, true);
        poly->setEdgeDefault(true);  // FancyArrow._edge_default
        for (const auto& [name, value] : rest)
            if (!patchProperty(*poly, name, *value, where, error)) return false;
        ax.addPatch(std::move(poly));
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // table(cellText=None, cellColours=None, cellLoc='right', colWidths=None, rowLabels=None, rowColours=None,
    // rowLoc='left', colLabels=None, colColours=None, colLoc='center', loc='bottom', bbox=None, edges='closed',
    // **kwargs): a Table of the cells, the column labels in row 0 and the row labels in column -1 (sized to their text
    // unless there are row colours alone), added to the Axes (add_table)
    bool table(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        static const char* const names[] = {"cellText", "cellColours", "cellLoc", "colWidths", "rowLabels", "rowColours", "rowLoc",
                                            "colLabels", "colColours", "colLoc", "loc", "bbox", "edges"};
        if (call.args.size() > std::size(names)) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming the table is not supported yet", false;
        const auto get = [&](std::size_t i) -> const Json* {
            const Json* v = argument(call, i, names[i]);
            return v && !v->isNull() ? v : nullptr;
        };
        const auto textOf = [&](const Json& v, std::string* out) {
            if (v.isString()) return *out = v.asString(), true;  // (str() of what is not text)
            if (v.isNumber()) return *out = v.isInteger() ? v.numberText() : support::pythonRepr(v.asNumber()), true;
            return false;
        };
        const auto strings = [&](const Json* v, const char* name, std::vector<std::string>* out) {
            const Json* seq = v ? sequence(*v) : nullptr;
            if (!seq) return *error = where + ": " + name + " must be a list of strings", false;
            out->clear();
            for (const Json& e : seq->items()) {
                std::string t;
                if (!textOf(e, &t)) return *error = where + ": " + name + " must be a list of strings", false;
                out->push_back(t);
            }
            return true;
        };
        // a list of colours, or a string of one-letter colours ('w' * n)
        const auto colourList = [&](const Json* v, const char* name, std::vector<colors::Spec>* out) {
            out->clear();
            if (v->isString()) {
                for (char ch : v->asString()) out->push_back(colors::Spec(std::string(1, ch)));
                return true;
            }
            const Json* seq = sequence(*v);
            if (!seq) return *error = where + ": " + name + " must be a list of colours", false;
            for (const Json& e : seq->items()) {
                colors::Spec c;
                if (!color(e, &c)) return *error = where + ": " + name + " must be a list of colours", false;
                out->push_back(c);
            }
            return true;
        };
        const Json* cellTextJson = get(0);
        const Json* cellColoursJson = get(1);
        if (!cellTextJson && !cellColoursJson)
            return *error = where + ": At least one argument from \"cellColours\" or \"cellText\" must be provided to create a table.", false;
        std::vector<std::vector<std::string>> cellText;
        std::vector<std::vector<colors::Spec>> cellColours;
        if (cellColoursJson) {
            const Json* rows = sequence(*cellColoursJson);
            if (!rows || rows->size() == 0) return *error = where + ": cellColours must be rows of colours", false;
            for (const Json& r : rows->items()) {
                std::vector<colors::Spec> row;
                if (!colourList(&r, "cellColours", &row)) return false;
                cellColours.push_back(row);
            }
        }
        if (cellTextJson) {
            const Json* rows = sequence(*cellTextJson);
            if (!rows || rows->size() == 0) return *error = where + ": cellText must be rows of strings", false;
            for (const Json& r : rows->items()) {
                std::vector<std::string> row;
                if (!strings(&r, "cellText", &row)) return false;
                cellText.push_back(row);
            }
        } else {
            cellText.assign(cellColours.size(), std::vector<std::string>(cellColours[0].size(), ""));
        }
        const std::size_t rows = cellText.size(), cols = cellText[0].size();
        for (const auto& row : cellText)
            if (row.size() != cols) return *error = where + ": Each row in 'cellText' must have " + std::to_string(cols) + " columns", false;
        if (cellColoursJson) {
            if (cellColours.size() != rows) return *error = where + ": 'cellColours' must have " + std::to_string(rows) + " rows", false;
            for (const auto& row : cellColours)
                if (row.size() != cols) return *error = where + ": Each row in 'cellColours' must have " + std::to_string(cols) + " columns", false;
        } else {
            cellColours.assign(rows, std::vector<colors::Spec>(cols, colors::Spec(std::string("w"))));
        }
        const auto textArg = [&](std::size_t i, const char* fallback, std::string* out) {
            *out = fallback;
            if (const Json* v = get(i)) {
                if (!v->isString()) return *error = where + ": " + names[i] + " must be a string", false;
                *out = v->asString();
            }
            return true;
        };
        std::string cellLoc, rowLoc, colLoc, loc, edges;
        if (!textArg(2, "right", &cellLoc) || !textArg(6, "left", &rowLoc) || !textArg(9, "center", &colLoc) || !textArg(10, "bottom", &loc) ||
            !textArg(12, "closed", &edges))
            return false;
        for (const std::string* l : {&cellLoc, &rowLoc, &colLoc})
            if (*l != "left" && *l != "center" && *l != "right")
                return *error = where + ": " + repr(Json::string(*l)) + " is not a valid value for horizontalalignment; supported values are 'left', 'center', 'right'", false;
        std::vector<double> colWidths(cols, 1.0 / double(cols));
        if (const Json* w = get(3)) {
            if (!numbers(*w, &colWidths) || colWidths.size() < cols) return *error = where + ": colWidths must be a number per column", false;
        }
        std::optional<std::vector<std::string>> rowLabels, colLabels;
        std::optional<std::vector<colors::Spec>> rowColours, colColours;
        std::vector<std::string> list;
        std::vector<colors::Spec> clist;
        if (const Json* v = get(4)) {
            if (!strings(v, "rowLabels", &list)) return false;
            rowLabels = list;
        }
        if (const Json* v = get(5)) {
            if (!colourList(v, "rowColours", &clist)) return false;
            rowColours = clist;
        }
        if (const Json* v = get(7)) {
            if (!strings(v, "colLabels", &list)) return false;
            colLabels = list;
        }
        if (const Json* v = get(8)) {
            if (!colourList(v, "colColours", &clist)) return false;
            colColours = clist;
        }
        std::optional<std::array<double, 4>> bbox;
        if (const Json* v = get(11)) {
            std::vector<double> b;
            if (!numbers(*v, &b) || b.size() != 4) return *error = where + ": bbox must be [left, bottom, width, height]", false;
            bbox = std::array<double, 4>{b[0], b[1], b[2], b[3]};
        }
        double rowLabelWidth = 0;
        if (!rowLabels) {
            if (rowColours) {
                rowLabels = std::vector<std::string>(rows, "");
                rowLabelWidth = colWidths[0];
            }
        } else if (!rowColours) {
            rowColours = std::vector<colors::Spec>(rows, colors::Spec(std::string("w")));
        }
        if (rowLabels && rowLabels->size() != rows) return *error = where + ": 'rowLabels' must be of length " + std::to_string(rows), false;
        int offset = 1;
        if (!colLabels) {
            if (colColours) colLabels = std::vector<std::string>(cols, "");
            else offset = 0;
        } else if (!colColours) {
            colColours = std::vector<colors::Spec>(cols, colors::Spec(std::string("w")));
        }
        if (rowColours && rowColours->size() < rows) return *error = where + ": rowColours must have a colour per row", false;
        if (colColours && colColours->size() < cols) return *error = where + ": colColours must have a colour per column", false;
        if (colLabels && colLabels->size() < cols) return *error = where + ": colLabels must have a label per column", false;
        auto t = Table::make(rc_, ax, loc, bbox, error);
        if (!t) return *error = where + ": " + *error, false;
        // the other keywords (Table._internal_update): the font size, the zorder
        std::optional<double> fontsize;
        for (const auto& [name, value] : call.kwargs) {
            if (std::find(std::begin(names), std::end(names), name) != std::end(names)) continue;
            double n = 0;
            if (name == "fontsize") {
                if (!readNumber(value, &n)) return *error = where + ": fontsize must be a number", false;
                fontsize = n;
            } else if (name == "zorder") {
                if (!readFinite(value, &n)) return *error = where + ": zorder must be a number", false;
                t->zorder = n;
            } else {
                return *error = where + ": " + name + " is not supported yet", false;
            }
        }
        if (edges != "open" && edges != "closed" && edges != "horizontal" && edges != "vertical")
            for (char ch : edges)
                if (std::string("BRTL").find(ch) == std::string::npos)
                    return *error = where + ": Invalid edge param " + edges + ", must only be one of open, closed, horizontal, vertical or string of B, R, T, L", false;
        t->edges = edges;
        const double height = t->approxTextHeight();
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c)
                t->addCell(int(r) + offset, int(c), colWidths[c], height, cellText[r][c], cellColours[r][c], cellLoc);
        if (colLabels)
            for (std::size_t c = 0; c < cols; ++c) t->addCell(0, int(c), colWidths[c], height, (*colLabels)[c], (*colColours)[c], colLoc);
        if (rowLabels) {
            for (std::size_t r = 0; r < rows; ++r)
                t->addCell(int(r) + offset, -1, rowLabelWidth != 0 ? rowLabelWidth : 1e-15, height, (*rowLabels)[r], (*rowColours)[r], rowLoc);
            if (rowLabelWidth == 0) t->autoColumns.push_back(-1);
        }
        if (fontsize) t->setFontsize(*fontsize);
        // add_table: the Axes' figure; its own transform (transAxes); not clipped (clip_on False)
        if (t->label.empty()) t->label = "_child" + std::to_string(ax.children.size());
        ax.children.push_back(std::move(t));
        return true;
    }

    // ecdf(x, weights=None, *, complementary=False, orientation='vertical', compress=False, **kwargs): the empirical
    // cumulative distribution as a stepped line (plot), its sticky edges at 0 and 1
    bool ecdf(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.args.size() > 2) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming ecdf's line is not supported yet", false;
        const Json* xj = argument(call, 0, "x");
        std::vector<double> x, weights;
        if (!xj || !numbers(*xj, &x)) return *error = where + ": x must be a list of numbers", false;
        const Json* wj = argument(call, 1, "weights");
        const bool weighted = wj && !wj->isNull();
        if (weighted && (!numbers(*wj, &weights) || weights.size() != x.size()))
            return *error = where + ": weights must be a number per value of x", false;
        bool complementary = false, compress = false;
        std::string orientation = "vertical";
        if (const Json* v = keyword(call, "orientation")) {
            if (!v->isString() || (v->asString() != "horizontal" && v->asString() != "vertical"))
                return *error = where + ": " + repr(*v) + " is not a valid value for orientation; supported values are 'horizontal', 'vertical'", false;
            orientation = v->asString();
        }
        for (const char* name : {"complementary", "compress"})
            if (const Json* v = keyword(call, name)) {
                if (!v->isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name[1] == 'o' && name[2] == 'm' && name[3] == 'p' && name[4] == 'l' ? complementary : compress) = v->asBool();
            }
        if (keyword(call, "drawstyle") || keyword(call, "ds")) return *error = where + ": Cannot pass 'drawstyle' or 'ds' to ecdf()", false;
        for (double v : x)
            if (std::isnan(v)) return *error = where + ": ecdf() does not support NaNs", false;
        if (x.empty()) return *error = where + ": ecdf() of no values is not supported yet", false;
        // np.argsort (not stable): ties that carry different weights would take NumPy's order
        std::vector<std::size_t> order(x.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return x[a] < x[b]; });
        if (weighted)
            for (std::size_t i = 1; i < order.size(); ++i)
                if (x[order[i]] == x[order[i - 1]] && weights[order[i]] != weights[order[i - 1]])
                    return *error = where + ": equal values with different weights are not supported yet", false;
        std::vector<double> xs, cum;
        for (std::size_t i : order) xs.push_back(x[i]);
        const std::size_t n = xs.size();
        if (!weighted) {
            for (std::size_t i = 0; i < n; ++i) cum.push_back(double(1 + i) / double(n));
        } else {
            std::vector<double> w;
            for (std::size_t i : order) w.push_back(weights[i]);
            const double total = support::npSum(w);
            double acc = 0;
            for (double v : w) cum.push_back(acc += v / total);  // np.cumsum(weights / np.sum(weights))
        }
        if (compress) {
            std::vector<double> cx{xs[0]}, cc{cum[0]};
            for (std::size_t i = 1; i < n; ++i)
                if (xs[i - 1] != xs[i]) cx.push_back(xs[i]), cc.push_back(cum[i]);
            xs = cx, cum = cc;
        }
        std::vector<double> a, b;  // the line's points
        std::string drawstyle;
        if (!complementary) {
            a = {xs[0]};
            a.insert(a.end(), xs.begin(), xs.end());
            b = {0.0};
            b.insert(b.end(), cum.begin(), cum.end());
        } else {
            a = xs;
            a.push_back(xs.back());
            b = {1.0};
            for (double c : cum) b.push_back(1 - c);
        }
        const bool vertical = orientation == "vertical";
        drawstyle = vertical == !complementary ? "steps-post" : "steps-pre";
        model::Call p;
        p.on = call.on;
        p.method = "plot";
        p.args = vertical ? std::vector<Json>{numbersJson(a), numbersJson(b)} : std::vector<Json>{numbersJson(b), numbersJson(a)};
        p.kwargs.emplace_back("drawstyle", Json::string(drawstyle));
        for (const auto& kw : call.kwargs)
            if (kw.first != "x" && kw.first != "weights" && kw.first != "complementary" && kw.first != "orientation" && kw.first != "compress")
                p.kwargs.push_back(kw);
        if (!plot(ax, p, error)) return false;
        Artist& line = *ax.children.back();
        (vertical ? line.stickyY : line.stickyX) = {0.0, 1.0};
        return true;
    }

    // xcorr(x, y, normed=True, detrend=mlab.detrend_none, usevlines=True, maxlags=10, **kwargs) and acorr(x, **kwargs)
    // (xcorr(x, x)): the correlations at the lags, as vlines from 0 and an axhline, or as markers (plot)
    bool xcorr(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const bool auto_ = call.method == "acorr";
        static const char* const names[] = {"x", "y", "normed", "detrend", "usevlines", "maxlags"};
        const std::size_t positional = auto_ ? 1 : 2;
        if (call.args.size() > positional) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming the result is not supported yet", false;
        std::vector<double> x, y;
        const Json* xj = argument(call, 0, "x");
        if (!xj || !numbers(*xj, &x)) return *error = where + ": x must be a list of numbers", false;
        const Json* yj = auto_ ? xj : argument(call, 1, "y");
        if (!yj || !numbers(*yj, &y)) return *error = where + ": y must be a list of numbers", false;
        const bool integer = integers(*xj) && integers(*yj);
        bool normed = true, usevlines = true;
        long long maxlags = 10;
        bool allLags = false;
        for (const char* name : {"normed", "usevlines"})
            if (const Json* v = keyword(call, name)) {
                if (!v->isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name[0] == 'n' ? normed : usevlines) = v->asBool();
            }
        if (const Json* d = keyword(call, "detrend"); d && !d->isNull()) return *error = where + ": detrend is not supported yet", false;
        if (const Json* m = keyword(call, "maxlags")) {
            if (m->isNull()) allLags = true;
            else if (!m->isNumber() || !m->isInteger()) return *error = where + ": maxlags must be an integer or None", false;
            else maxlags = (long long)m->asNumber();
        }
        const long long nx = (long long)x.size();
        if (nx != (long long)y.size()) return *error = where + ": x and y must be equal length", false;
        if (nx == 0) return *error = where + ": first array argument cannot be empty", false;
        std::vector<double> c = support::npCorrelateFull(x, y, integer);
        if (normed) {
            const double xx = integer ? support::npCorrelateFull(x, x, true)[std::size_t(nx - 1)] : 0.0 + support::blasDot(x, x);
            const double yy = integer ? support::npCorrelateFull(y, y, true)[std::size_t(nx - 1)] : 0.0 + support::blasDot(y, y);
            const double norm = std::sqrt(xx * yy);
            for (double& v : c) v = v / norm;
        }
        if (allLags) maxlags = nx - 1;
        if (maxlags >= nx || maxlags < 1) return *error = where + ": maxlags must be None or strictly positive < " + std::to_string(nx), false;
        std::vector<Json> lags;
        std::vector<double> correls;
        for (long long k = -maxlags; k <= maxlags; ++k) {
            lags.push_back(Json::integer(k));
            correls.push_back(c[std::size_t(nx - 1 + k)]);
        }
        std::vector<std::pair<std::string, Json>> kwargs;
        for (const auto& kw : call.kwargs)
            if (std::find(std::begin(names), std::end(names), kw.first) == std::end(names)) kwargs.push_back(kw);
        model::Call p;
        p.on = call.on;
        if (usevlines) {
            p.method = "vlines";
            p.args = {Json::array(lags), Json::array({Json::integer(0)}), numbersJson(correls)};
            p.kwargs = kwargs;
            if (!lines(ax, p, error)) return false;
            // the label goes to the vertical lines alone
            kwargs.erase(std::remove_if(kwargs.begin(), kwargs.end(), [](const auto& kw) { return kw.first == "label"; }), kwargs.end());
            p.method = "axhline";
            p.args.clear();
            p.kwargs = kwargs;
            return axline(ax, p, error);
        }
        const auto has = [&](const char* k) {
            return std::any_of(kwargs.begin(), kwargs.end(), [&](const auto& kw) { return kw.first == k; });
        };
        if (!has("marker")) kwargs.emplace_back("marker", Json::string("o"));
        if (!has("linestyle")) kwargs.emplace_back("linestyle", Json::string("None"));
        p.method = "plot";
        p.args = {Json::array(lags), numbersJson(correls)};
        p.kwargs = kwargs;
        return plot(ax, p, error);
    }

    bool colorsCall(const model::Call& call, std::string* error) {
        const std::string where = "colors." + call.method;
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        struct Parameters {
            const char* cls;
            std::vector<const char*> positional, keywordOnly, required;
        };
        static const Parameters classes[] = {
            {"Normalize", {"vmin", "vmax", "clip"}, {}, {}},
            {"LogNorm", {"vmin", "vmax", "clip"}, {}, {}},
            {"SymLogNorm", {"linthresh", "linscale", "vmin", "vmax", "clip"}, {"base"}, {"linthresh"}},
            {"AsinhNorm", {"linear_width", "vmin", "vmax", "clip"}, {}, {}},
            {"PowerNorm", {"gamma", "vmin", "vmax", "clip"}, {}, {"gamma"}},
            {"TwoSlopeNorm", {"vcenter", "vmin", "vmax"}, {}, {"vcenter"}},
            {"CenteredNorm", {"vcenter", "halfrange", "clip"}, {}, {}},
            {"BoundaryNorm", {"boundaries", "ncolors", "clip"}, {"extend"}, {"boundaries", "ncolors"}},
            {"NoNorm", {"vmin", "vmax", "clip"}, {}, {}}};
        const Parameters* params = nullptr;
        for (const auto& c : classes)
            if (call.method == c.cls) params = &c;
        if (!params) return *error = where + " is not supported yet", false;
        std::map<std::string, const Json*> given;
        if (call.args.size() > params->positional.size()) return *error = where + ": too many positional arguments", false;
        for (std::size_t i = 0; i < call.args.size(); ++i) given[params->positional[i]] = &call.args[i];
        for (const auto& kw : call.kwargs) {
            const std::string& name = kw.first;
            const Json& value = kw.second;
            const bool known = std::find_if(params->positional.begin(), params->positional.end(), [&](const char* n) { return name == n; }) !=
                                   params->positional.end() ||
                               std::find_if(params->keywordOnly.begin(), params->keywordOnly.end(), [&](const char* n) { return name == n; }) !=
                                   params->keywordOnly.end();
            if (!known) return *error = where + ": got an unexpected keyword argument '" + name + "'", false;
            if (given.count(name)) return *error = where + ": got multiple values for argument '" + name + "'", false;
            given[name] = &value;
        }
        for (const char* r : params->required)
            if (!given.count(r)) return *error = where + ": missing 1 required positional argument: '" + r + "'", false;
        colors::NormSpec spec;
        spec.kind = call.method;
        for (const auto& entry : given) {
            const std::string& name = entry.first;
            const Json* value = entry.second;
            double v = 0;
            const auto number = [&](std::optional<double>* out, bool noneAllowed) {
                if (value->isNull() && noneAllowed) return out->reset(), true;
                if (!readNumber(*value, &v)) return *error = where + ": " + name + " must be a number", false;
                return *out = v, true;
            };
            std::optional<double> o;
            if (name == "vmin" || name == "vmax" || name == "halfrange") {
                if (!number(name == "vmin" ? &spec.vmin : name == "vmax" ? &spec.vmax : &spec.halfrange, true)) return false;
            } else if (name == "clip") {
                if (!value->isBool()) return *error = where + ": clip must be True or False", false;
                spec.clip = value->asBool();
            } else if (name == "vcenter" || name == "linthresh") {
                if (!number(name == "vcenter" ? &spec.vcenter : &spec.linthresh, false)) return false;
            } else if (name == "linscale" || name == "base" || name == "linear_width" || name == "gamma") {
                if (!number(&o, false)) return false;
                (name == "linscale" ? spec.linscale : name == "base" ? spec.base : name == "gamma" ? spec.gamma : spec.linearWidth) = *o;
            } else if (name == "boundaries") {
                const Json* list = sequence(*value);
                if (!list) return *error = where + ": boundaries must be a list of numbers", false;
                for (const Json& b : list->items()) {
                    if (!readNumber(b, &v)) return *error = where + ": boundaries must be a list of numbers", false;
                    spec.boundaries.push_back(v);
                }
            } else if (name == "ncolors") {
                if (!value->isNumber() || !value->isInteger()) return *error = where + ": ncolors must be an integer", false;
                spec.ncolors = (long long)value->asNumber();
            } else if (name == "extend") {
                if (!value->isString()) return *error = where + ": extend must be a string", false;
                spec.extend = value->asString();
            }
        }
        if (spec.kind == "CenteredNorm" && !spec.vcenter) spec.vcenter = 0.0;
        auto norm = std::make_shared<colors::Norm>();
        if (!colors::Norm::make(spec, norm.get(), error)) return *error = where + ": " + *error, false;
        if (!call.as.empty()) norms_[call.as[0]] = norm;
        return true;
    }


    // The Axes a value names ({"$ref": "ax"} or {"$ref": "axs[0, 1]"}); nullptr for anything else
    Axes* axesOf(const Json& v) {
        if (!(v.isObject() && v.members().size() == 1 && v.members()[0].first == "$ref" && v.members()[0].second.isString())) return nullptr;
        const auto it = axes_.find(v.members()[0].second.asString());
        return it == axes_.end() ? nullptr : it->second;
    }

    // ScalarMappable of an artist: an image's or a collection's colormap and norm, the data, its scalar alpha
    std::optional<Mappable> mappableOf(Artist* a, Axes* ax) {
        Mappable m;
        m.artist = a;
        m.axes = ax;
        if (auto* im = dynamic_cast<ImageBase*>(a)) {
            m.cmap = &im->cmap;
            m.norm = im->norm;
            colors::MaskedArray raw;
            raw.data = im->data().data;
            raw.mask = im->data().mask;
            raw.dtype = im->data().dtype;
            m.array = raw;
            if (!im->alphaArray) m.alpha = im->alpha;
            return m;
        }
        if (auto* c = dynamic_cast<Collection*>(a)) {
            if (!c->cmap) return std::nullopt;
            m.contour = dynamic_cast<const ContourSet*>(c);
            m.cmap = &*c->cmap;
            m.norm = c->norm;
            m.array = c->array;
            m.alpha = c->alpha;
            return m;
        }
        return std::nullopt;
    }

    // After a call that made a mappable: its "as" name, and pyplot's current image (plt.sci) for pyplot's calls
    bool noteMappable(Axes& ax, const model::Call& call) {
        if (ax.children.empty()) return true;
        const auto m = mappableOf(ax.children.back().get(), &ax);
        if (!m) return true;
        if (call.as.size() == 1) mappables_[call.as[0]] = *m;
        if (call.on == "plt") currentImages_[&ax] = *m;
        return true;
    }

    // Figure.colorbar(mappable, cax=None, ax=None, use_gridspec=True, **kwargs), and plt.colorbar(mappable=None,
    // cax=None, ax=None, **kwargs) whose mappable is the current image (gci)
    bool colorbar(const model::Call& call, std::string* error) {
        const std::string where = (call.on.empty() ? std::string("plt") : call.on) + ".colorbar";
        const bool pyplot = call.on == "plt" || call.on.empty();
        static const char* const positional[] = {"mappable", "cax", "ax", "use_gridspec"};
        if (call.args.size() > (pyplot ? 3u : 4u)) return *error = where + ": too many positional arguments", false;
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (keyword(call, positional[i])) return *error = where + ": got multiple values for argument '" + positional[i] + "'", false;
        if (!call.as.empty()) return *error = where + ": naming the colorbar is not supported yet", false;
        Figure& fig = gcf();
        std::optional<Mappable> m;
        const Json* mv = argument(call, 0, "mappable");
        if (mv && !mv->isNull()) {
            const bool ref = mv->isObject() && mv->members().size() == 1 && mv->members()[0].first == "$ref";
            const auto it = ref ? mappables_.find(mv->members()[0].second.asString()) : mappables_.end();
            if (it == mappables_.end()) return *error = where + ": the mappable must be an image or a collection the figure file named ({\"$ref\": name})", false;
            m = it->second;
        } else if (pyplot) {
            // gci: the current Axes' current image, else the latest of the other Axes
            const auto cur = currentImages_.find(&gca());
            if (cur != currentImages_.end()) m = cur->second;
            for (auto it = fig.axes.rbegin(); !m && it != fig.axes.rend(); ++it) {
                const auto c = currentImages_.find(it->get());
                if (c != currentImages_.end()) m = c->second;
            }
            if (!m) return *error = where + ": No mappable was found to use for colorbar creation. First define a mappable such as an image (with imshow) or a contour set (with contourf).", false;
        } else {
            return *error = where + ": missing 1 required positional argument: 'mappable'", false;
        }
        Axes* cax = nullptr;
        if (const Json* c = argument(call, 1, "cax"); c && !c->isNull()) {
            cax = axesOf(*c);
            if (!cax) return *error = where + ": cax must be an Axes of the figure ({\"$ref\": name})", false;
        }
        std::vector<Axes*> parents;
        if (const Json* a = argument(call, 2, "ax"); a && !a->isNull()) {
            if (Axes* one = axesOf(*a)) {
                parents = {one};
            } else if (a->isObject() && a->members()[0].first == "$ref" && axesArrays_.count(a->members()[0].second.asString())) {
                parents = axesArrays_[a->members()[0].second.asString()];
            } else if (const Json* list = sequence(*a)) {
                for (const Json& item : list->items()) {
                    Axes* listed = axesOf(item);
                    if (!listed) return *error = where + ": ax must be Axes of the figure ({\"$ref\": name}) or a list of them", false;
                    parents.push_back(listed);
                }
            } else {
                return *error = where + ": ax must be Axes of the figure ({\"$ref\": name}) or a list of them", false;
            }
        } else {
            parents = {m->axes};
        }
        bool useGridspec = true;
        if (const Json* g = pyplot ? keyword(call, "use_gridspec") : argument(call, 3, "use_gridspec"); g && !g->isNull()) {
            if (!g->isBool()) return *error = where + ": use_gridspec must be True or False", false;
            useGridspec = g->asBool();
        }
        ColorbarOptions o;
        for (const auto& kw : call.kwargs) {
            const std::string& name = kw.first;
            const Json& value = kw.second;
            double v = 0;
            if (std::find(std::begin(positional), std::end(positional), name) != std::end(positional)) continue;
            const auto text = [&](std::optional<std::string>* out) {
                if (value.isNull()) return true;
                if (!value.isString()) return *error = where + ": " + name + " must be a string", false;
                return *out = value.asString(), true;
            };
            const auto pair = [&](std::optional<std::array<double, 2>>* out) {
                const Json* p = sequence(value);
                std::array<double, 2> a{};
                if (!p || p->size() != 2 || !readNumber((*p)[0], &a[0]) || !readNumber((*p)[1], &a[1])) return false;
                return *out = a, true;
            };
            if (name == "location" || name == "orientation" || name == "extend") {
                if (!text(name == "location" ? &o.location : name == "orientation" ? &o.orientation : &o.extend)) return false;
            } else if (name == "ticklocation" || name == "spacing" || name == "label") {
                if (!value.isString()) return *error = where + ": " + name + " must be a string", false;
                (name == "ticklocation" ? o.ticklocation : name == "spacing" ? o.spacing : o.label) = value.asString();
            } else if (name == "extendrect" || name == "drawedges") {
                if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name == "extendrect" ? o.extendrect : o.drawedges) = value.asBool();
            } else if (name == "extendfrac") {
                if (value.isNull()) continue;
                if (value.isString()) {
                    std::string s = value.asString();
                    for (char& ch : s) ch = char(std::tolower((unsigned char)ch));
                    if (s != "auto") return *error = where + ": '" + s + "' is not a valid value for extendfrac; supported values are 'auto'", false;
                    o.extendfracAuto = true;
                } else if (readNumber(value, &v)) {
                    o.extendfrac = std::array<double, 2>{v, v};
                } else if (!pair(&o.extendfrac)) {
                    return *error = where + ": invalid value for extendfrac", false;
                }
            } else if (name == "ticks") {
                if (value.isNull()) continue;
                std::vector<double> t;
                if (!numbers(value, &t)) return *error = where + ": ticks must be a list of numbers (locators are not supported yet)", false;
                o.ticks = t;
            } else if (name == "fraction" || name == "shrink" || name == "aspect" || name == "pad") {
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                if (name == "pad") o.pad = v;
                else (name == "fraction" ? o.fraction : name == "shrink" ? o.shrink : o.aspect) = v;
            } else if (name == "anchor" || name == "panchor") {
                if (name == "panchor" && value.isBool() && !value.asBool()) {
                    o.panchorFalse = true;
                } else if (!pair(name == "anchor" ? &o.anchor : &o.panchor)) {
                    return *error = where + ": " + name + " must be a pair of numbers", false;
                }
            } else if (name == "format") {
                if (value.isNull()) continue;
                if (!value.isString()) return *error = where + ": format must be a format string (formatters are not supported yet)", false;
                o.format = value.asString();
            } else if (name == "values" || name == "boundaries") {
                if (value.isNull()) continue;
                std::vector<double> list;
                if (!numbers(value, &list)) return *error = where + ": " + name + " must be a list of numbers", false;
                (name == "values" ? o.values : o.boundaries) = list;
            } else if (name == "cmap" || name == "norm") {
                if (!value.isNull()) return *error = where + ": " + name + " is not supported yet", false;
            } else if (name == "alpha") {
                continue;  // (the mappable's alpha replaces it)
            } else {
                return *error = where + ": got an unexpected keyword argument '" + name + "'", false;
            }
        }
        if (!makeColorbar(fig, *m, cax, parents, useGridspec, o, error)) return *error = where + ": " + *error, false;
        return true;
    }

    // The colorizer of an image or a collection: a colormap by name (or `cmapDefault`), and a norm by scale name or one the
    // figure file made ({"$ref"}, shared), else Normalize (or NoNorm with `noNorm`). `normObject`: a norm object.
    bool colorizer(colors::Colormap* cmapOut, std::shared_ptr<colors::Norm>* normOut, const Json* c, const Json* n,
                   const colors::Colormap* cmapDefault, bool noNorm, const std::string& where, bool* normObject, std::string* error) {
        if (c && !c->isNull()) {
            if (!c->isString()) return *error = where + ": cmap must be a colormap name", false;
            const auto cmap = colors::Colormap::named(c->asString());
            if (!cmap) return *error = where + ": '" + c->asString() + "' is not a valid value for cmap", false;
            *cmapOut = *cmap;
        } else if (cmapDefault) {
            *cmapOut = *cmapDefault;
        } else {
            const auto cmap = colors::Colormap::named(rc_.text("image.cmap"));
            if (!cmap) return *error = where + ": '" + rc_.text("image.cmap") + "' is not a valid value for cmap", false;
            *cmapOut = *cmap;
        }
        *normObject = false;
        if (n && n->isObject() && n->members()[0].first == "$ref") {
            const auto it = norms_.find(n->members()[0].second.asString());
            if (it == norms_.end()) return *error = where + ": norm must be a norm the figure file made, or a scale name", false;
            *normOut = it->second;
            *normObject = true;
            return true;
        }
        colors::NormSpec spec;
        if (n && !n->isNull()) {
            static const char* const scales[] = {"linear", "log", "symlog", "asinh", "logit"};
            if (!n->isString()) return *error = where + ": norm must be a norm the figure file made, or a scale name", false;
            const std::string name = n->asString();
            if (name == "function" || name == "functionlog")
                return *error = where + ": the norm of the " + name + " scale is not supported yet", false;
            if (std::find(std::begin(scales), std::end(scales), name) == std::end(scales))
                return *error = where + ": '" + name + "' is not a valid value for norm; supported values are 'asinh', 'function', 'functionlog', 'linear', 'log', 'logit', 'symlog'", false;
            spec.kind = "scale:" + name;
        } else if (noNorm) {
            spec.kind = "NoNorm";
        }
        *normOut = std::make_shared<colors::Norm>();
        if (!colors::Norm::make(spec, normOut->get(), error)) return *error = where + ": " + *error, false;
        return true;
    }

    // set_alpha of an image: a number in [0, 1], a 2-D array of them, or None
    bool imageAlpha(ImageBase& im, const Json* alpha, const std::string& where, std::string* error) {
        if (!alpha || alpha->isNull()) return true;
        double a = 0;
        if (readNumber(*alpha, &a)) {
            if (!(0 <= a && a <= 1)) return *error = where + ": alpha (" + repr(*alpha) + ") is outside 0-1 range", false;
            im.alpha = a;
            return true;
        }
        ImageArray al;
        if (alpha->isString() || !readArray(*alpha, &al, error) || al.channels != 0)
            return *error = where + ": alpha must be a float, two-dimensional array, or None", false;
        const auto [mn, mx] = std::minmax_element(al.data.begin(), al.data.end());
        if (!(0 <= *mn && *mx <= 1)) return *error = where + ": alpha must be between 0 and 1, inclusive", false;
        al.dtype = colors::DType::Float64;
        im.alphaArray = al;
        return true;
    }

    // The properties an image takes as keywords (its constructor's and the Artist's); false and why for one it
    // does not take, `*taken` false for a name that is not one of them
    bool imageProperty(ImageBase& im, const std::string& name, const Json& value, const std::string& where, bool* taken,
                       std::string* error) {
        static const char* const interpolations[] = {"auto", "none", "nearest", "bilinear", "bicubic", "spline16", "spline36",
                                                     "hanning", "hamming", "hermite", "kaiser", "quadric", "catrom", "gaussian",
                                                     "bessel", "mitchell", "sinc", "lanczos", "blackman", "antialiased"};
        double v = 0;
        *taken = true;
        if (name == "interpolation") {
            std::string s = value.isNull() ? rc_.text("image.interpolation") : value.isString() ? value.asString() : "";
            for (char& ch : s) ch = char(std::tolower((unsigned char)ch));
            if (std::find(std::begin(interpolations), std::end(interpolations), s) == std::end(interpolations))
                return *error = where + ": that is not a valid value for interpolation", false;
            im.interpolation = s;
        } else if (name == "interpolation_stage") {
            const std::string s = value.isNull() ? rc_.text("image.interpolation_stage") : value.isString() ? value.asString() : "";
            if (s != "data" && s != "rgba" && s != "auto")
                return *error = where + ": that is not a valid value for s; supported values are 'data', 'rgba', 'auto'", false;
            im.interpolationStage = s;
        } else if (name == "origin") {
            const std::string s = value.isNull() ? rc_.text("image.origin") : value.isString() ? value.asString() : "";
            if (s != "upper" && s != "lower")
                return *error = where + ": that is not a valid value for origin; supported values are 'upper', 'lower'", false;
            im.origin = s;
        } else if (name == "filternorm") {
            if (!value.isBool() && !readNumber(value, &v)) return *error = where + ": filternorm must be True or False", false;
            im.filternorm = value.isBool() ? value.asBool() : v != 0;
        } else if (name == "filterrad") {
            if (!readNumber(value, &v)) return *error = where + ": filterrad must be a number", false;
            if (!(v > 0)) return *error = where + ": The filter radius must be a positive number", false;
            im.filterrad = v;
        } else if (name == "resample") {
            if (value.isNull()) return true;
            if (!value.isBool()) return *error = where + ": resample must be True or False", false;
            im.resample = value.asBool();
        } else if (name == "url" || name == "gid") {
            if (value.isNull()) return true;
            if (!value.isString()) return *error = where + ": " + name + " must be a string", false;
            (name == "url" ? im.url : im.gid) = value.asString();
        } else if (name == "label") {
            if (!value.isString()) return *error = where + ": label must be a string", false;
            im.label = value.asString();
        } else if (name == "colorizer") {
            if (!value.isNull()) return *error = where + ": colorizer is not supported yet", false;
        } else if (name == "zorder") {
            if (!readFinite(value, &v)) return *error = where + ": zorder must be a number", false;
            im.zorder = v;
        } else if (name == "visible" || name == "clip_on") {
            if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
            (name == "visible" ? im.visible : im.clipOn) = value.asBool();
        } else {
            *taken = false;
        }
        return true;
    }

    // Axes.imshow(X, cmap=None, norm=None, *, aspect=None, interpolation=None, alpha=None, vmin=None, vmax=None,
    // colorizer=None, origin=None, extent=None, interpolation_stage=None, filternorm=True, filterrad=4.0,
    // resample=None, url=None, **artist properties). spy's image takes its own colormap and NoNorm as defaults.
    bool imshow(Axes& ax, const model::Call& call, std::string* error, const colors::Colormap* cmapDefault = nullptr,
                bool noNorm = false, AxesImage** made = nullptr) {
        const std::string where = call.on + "." + call.method;
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        const Json* X = argument(call, 0, "X");
        if (!X) return *error = where + ": missing 1 required positional argument: 'X'", false;
        ImageArray data;
        if (!readArray(*X, &data, error)) return *error = where + ": " + *error, false;
        auto im = std::make_unique<AxesImage>(rc_, ax);
        bool normObject = false;
        if (!colorizer(&im->cmap, &im->norm, argument(call, 1, "cmap"), argument(call, 2, "norm"), cmapDefault, noNorm, where, &normObject, error))
            return false;
        std::optional<double> vmin, vmax;
        const Json* aspect = nullptr;
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            bool taken = false;
            if (name == "X" || name == "cmap" || name == "norm" || name == "alpha") continue;
            if (name == "aspect") {
                aspect = &value;
            } else if (name == "vmin" || name == "vmax") {
                if (value.isNull()) continue;
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "vmin" ? vmin : vmax) = v;
            } else if (name == "extent") {
                if (value.isNull()) continue;
                const Json* list = sequence(value);
                std::array<double, 4> e{};
                if (!list || list->size() != 4) return *error = where + ": extent must be (left, right, bottom, top)", false;
                for (std::size_t k = 0; k < 4; ++k)
                    if (!readNumber((*list)[k], &e[k])) return *error = where + ": extent must be (left, right, bottom, top)", false;
                im->extent = e;
            } else {
                if (!imageProperty(*im, name, value, where, &taken, error)) return false;
                if (!taken) return *error = where + ": " + name + " is not supported yet", false;
            }
        }
        // aspect: rcParams["image.aspect"] unless given
        if (!aspect || aspect->isNull()) aspect = &rc_.value("image.aspect");
        model::Call setAspect;
        setAspect.on = call.on;
        setAspect.method = "set_aspect";
        setAspect.args = {*aspect};
        if (!axesCall(ax, setAspect, error)) return *error = where + ": " + *error, false;
        if (!im->setData(data, error)) return *error = where + ": " + *error, false;
        if (!imageAlpha(*im, keyword(call, "alpha"), where, error)) return false;
        // clipped to the Axes patch (a rectangle: its box)
        ax.clipToPatch(*im);
        // _scale_norm: vmin / vmax set the norm's limits (not with a norm object), then autoscale_None
        if (vmin || vmax) {
            if (normObject)
                return *error = where + ": Passing a Normalize instance simultaneously with vmin/vmax is not supported.  "
                                        "Please pass vmin/vmax directly to the norm when creating it.", false;
            if (vmin) im->norm->setVmin(*vmin);
            if (vmax) im->norm->setVmax(*vmax);
        }
        colors::MaskedArray raw;
        raw.data = im->data().data;
        raw.mask = im->data().mask;
        raw.dtype = im->data().dtype;
        im->norm->autoscaleNone(colors::processValue(raw));
        // set_extent(get_extent()): the data limits, the sticky edges, and the view when autoscaling
        const auto e = im->getExtent();
        for (double v : e)
            if (!std::isfinite(v)) return *error = where + ": Axis limits cannot be NaN or Inf", false;
        im->extent = e;
        ax.updateDatalim({{e[0], e[2]}, {e[1], e[3]}});
        im->stickyX = {e[0], e[1]};
        im->stickyY = {e[2], e[3]};
        if (ax.xaxis->autoscaleOn) ax.xaxis->setLim(e[0], e[1], std::nullopt);
        if (ax.yaxis->autoscaleOn) ax.yaxis->setLim(e[2], e[3], std::nullopt);
        AxesImage* ref = im.get();
        ax.addImage(std::move(im));
        if (made) *made = ref;
        return true;
    }

    // What matshow and spy do after drawing: the title a little higher, the x ticks on top (spy: on the side of
    // origin 0) and on both sides, and integer MaxNLocator(nbins=9, steps=[1, 2, 5, 10]) for both axes.
    bool matrixAxes(Axes& ax, bool top, std::string* error) {
        ax.title->y = 1.05;
        if (!ax.xaxis->tickSide(top ? "top" : "bottom", error) || !ax.xaxis->setTicksPosition("both", error)) return false;
        for (Axis* axis : {ax.xaxis.get(), ax.yaxis.get()}) {
            axes::MaxNLocator locator;
            locator.nbins = 9;
            locator.setSteps({1, 2, 5, 10});
            locator.integer = true;
            axis->tickerAxis().locator = locator;
        }
        return true;
    }

    // Axes.matshow(Z, **kwargs): imshow with origin 'upper', interpolation 'nearest' and aspect 'equal' unless given
    bool matshow(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".matshow";
        if (call.args.size() > 1) return *error = where + ": too many positional arguments", false;
        const Json* Z = argument(call, 0, "Z");
        if (!Z) return *error = where + ": missing 1 required positional argument: 'Z'", false;
        model::Call c;
        c.on = call.on;
        c.method = "matshow";
        c.as = call.as;
        c.args = {*Z};
        c.kwargs = {{"origin", Json::string("upper")}, {"interpolation", Json::string("nearest")}, {"aspect", Json::string("equal")}};
        for (const auto& kw : call.kwargs) {  // {**defaults, **kwargs}: a given value takes the default's place
            const std::string& name = kw.first;
            const Json& value = kw.second;
            if (name == "Z") continue;
            if (name == "X") return *error = where + ": imshow() got multiple values for argument 'X'", false;
            auto it = std::find_if(c.kwargs.begin(), c.kwargs.end(), [&](const auto& entry) { return entry.first == name; });
            if (it != c.kwargs.end()) it->second = value;
            else c.kwargs.emplace_back(name, value);
        }
        if (!imshow(ax, c, error)) return false;
        return matrixAxes(ax, true, error);
    }

    // Axes.spy(Z, precision=0, marker=None, markersize=None, aspect='equal', origin='upper', **kwargs): the
    // entries whose magnitude exceeds precision, as an image (no marker) or as markers.
    bool spy(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".spy";
        static const char* const own[] = {"Z", "precision", "marker", "markersize", "aspect", "origin"};
        if (call.args.size() > 6) return *error = where + ": too many positional arguments", false;
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (keyword(call, own[i])) return *error = where + ": got multiple values for argument '" + own[i] + "'", false;
        const Json* Z = argument(call, 0, "Z");
        if (!Z) return *error = where + ": missing 1 required positional argument: 'Z'", false;
        double precision = 0;
        if (const Json* p = argument(call, 1, "precision"); p && !readNumber(*p, &precision))
            return *error = where + ": precision must be a number ('present' is for sparse arrays)", false;
        const Json* marker = argument(call, 2, "marker");
        const Json* markersize = argument(call, 3, "markersize");
        if (marker && marker->isNull()) marker = nullptr;
        if (markersize && markersize->isNull()) markersize = nullptr;
        const Json* aspectArg = argument(call, 4, "aspect");
        const Json aspect = aspectArg ? *aspectArg : Json::string("equal");
        const Json* originArg = argument(call, 5, "origin");
        const std::string origin = originArg && originArg->isString() ? originArg->asString() : originArg ? "" : "upper";
        if (origin != "upper" && origin != "lower")
            return *error = where + ": that is not a valid value for origin; supported values are 'upper', 'lower'", false;
        ImageArray z;
        if (!readArray(*Z, &z, error) || z.channels != 0) return *error = where + ": Z must be a 2-D array", false;
        std::vector<std::pair<std::string, Json>> rest;
        for (const auto& [name, value] : call.kwargs)
            if (std::find(std::begin(own), std::end(own), name) == std::end(own)) rest.emplace_back(name, value);
        if (!marker && !markersize) {
            // an image of the mask: ListedColormap(['w', 'k']) and NoNorm unless given
            for (const auto& kw : rest)
                if (kw.first == "interpolation") return *error = where + ": spy() got an unexpected keyword argument 'interpolation'", false;
            std::vector<Json> rows;
            for (int r = 0; r < z.rows; ++r) {
                std::vector<Json> row;
                for (int c = 0; c < z.cols; ++c) row.push_back(Json::boolean(std::fabs(z.data[std::size_t(r) * z.cols + c]) > precision));
                rows.push_back(Json::array(row));
            }
            model::Call c;
            c.on = call.on;
            c.method = "spy";
            c.as = call.as;
            c.args = {Json::array(rows)};
            c.kwargs = {{"interpolation", Json::string("nearest")}, {"aspect", aspect}, {"origin", Json::string(origin)}};
            for (const auto& kw : rest) c.kwargs.push_back(kw);
            const colors::Colormap binary = colors::Colormap::listed({{1, 1, 1, 1}, {0, 0, 0, 1}}, "binary");
            if (!imshow(ax, c, error, &binary, !keyword(call, "norm"))) return false;
        } else {
            // Line2D(x, y, linestyle='None', marker=marker, markersize=markersize, **kwargs) of np.nonzero
            if (!call.as.empty()) return *error = where + ": naming spy's line is not supported yet", false;
            std::vector<double> xs, ys;
            for (int r = 0; r < z.rows; ++r)
                for (int c = 0; c < z.cols; ++c)
                    if (std::fabs(z.data[std::size_t(r) * z.cols + c]) > precision) ys.push_back(r), xs.push_back(c);
            auto line = std::make_unique<Line2D>(rc_, xs, ys);
            for (const auto& kw : rest)
                if (kw.first == "linestyle" || kw.first == "ls") return *error = where + ": spy() got an unexpected keyword argument 'linestyle'", false;
            const Json markerValue = marker ? *marker : Json::string("s");
            const Json sizeValue = markersize ? *markersize : Json::integer(10);
            if (!lineProperty(*line, "linestyle", Json::string("None"), where, error) ||
                !lineProperty(*line, "marker", markerValue, where, error) ||
                !lineProperty(*line, "markersize", sizeValue, where, error))
                return false;
            for (const auto& [name, value] : rest)
                if (!lineProperty(*line, fullName(name, lineAliases()), value, where, error)) return false;
            ax.addLine(std::move(line));
            model::Call lim;
            lim.on = call.on;
            lim.method = "set_xlim";
            lim.args = {Json::real(-0.5), Json::real(z.cols - 0.5)};
            if (!axesCall(ax, lim, error)) return false;
            lim.method = "set_ylim";
            lim.args = origin == "upper" ? std::vector<Json>{Json::real(z.rows - 0.5), Json::real(-0.5)}
                                         : std::vector<Json>{Json::real(-0.5), Json::real(z.rows - 0.5)};
            if (!axesCall(ax, lim, error)) return false;
            model::Call setAspect;
            setAspect.on = call.on;
            setAspect.method = "set_aspect";
            setAspect.args = {aspect};
            if (!axesCall(ax, setAspect, error)) return false;
        }
        return matrixAxes(ax, origin == "upper", error);
    }

    // Figure.figimage(X, xo=0, yo=0, alpha=None, norm=None, cmap=None, vmin=None, vmax=None, origin=None,
    // resize=False, *, colorizer=None, **kwargs): an image at (xo, yo) pixels on the figure
    bool figimage(const model::Call& call, std::string* error) {
        const std::string where = (call.on.empty() ? std::string("plt") : call.on) + ".figimage";
        static const char* const names[] = {"X", "xo", "yo", "alpha", "norm", "cmap", "vmin", "vmax", "origin", "resize"};
        if (call.args.size() > 10) return *error = where + ": too many positional arguments", false;
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (keyword(call, names[i])) return *error = where + ": got multiple values for argument '" + names[i] + "'", false;
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        const Json* X = argument(call, 0, "X");
        if (!X) return *error = where + ": missing 1 required positional argument: 'X'", false;
        double xo = 0, yo = 0;
        if (const Json* v = argument(call, 1, "xo"); v && !readNumber(*v, &xo)) return *error = where + ": xo must be a number", false;
        if (const Json* v = argument(call, 2, "yo"); v && !readNumber(*v, &yo)) return *error = where + ": yo must be a number", false;
        ImageArray data;
        if (!readArray(*X, &data, error)) return *error = where + ": " + *error, false;
        Figure& fig = *figure_;
        const Json* resize = argument(call, 9, "resize");
        if (resize && !resize->isNull() && !resize->isBool()) return *error = where + ": resize must be True or False", false;
        if (resize && resize->isBool() && resize->asBool()) fig.setSizeInches(data.cols / fig.dpi, data.rows / fig.dpi);
        auto im = std::make_unique<FigureImage>(rc_, fig, xo, yo);
        const Json* n = argument(call, 4, "norm");
        bool normObject = false;
        if (!colorizer(&im->cmap, &im->norm, argument(call, 5, "cmap"), n, nullptr, false, where, &normObject, error)) return false;
        if (const Json* o = argument(call, 8, "origin")) {
            bool taken = false;
            if (!imageProperty(*im, "origin", *o, where, &taken, error)) return false;
        }
        for (const auto& [name, value] : call.kwargs) {
            if (std::find(std::begin(names), std::end(names), name) != std::end(names)) continue;
            bool taken = false;
            if (!imageProperty(*im, name, value, where, &taken, error)) return false;
            if (!taken) return *error = where + ": " + name + " is not supported yet", false;
        }
        if (!im->setData(data, error)) return *error = where + ": " + *error, false;
        if (!imageAlpha(*im, argument(call, 3, "alpha"), where, error)) return false;
        if (!n || n->isNull()) {  // set_clim(vmin, vmax); the norm scales itself when the image is drawn
            double v = 0;
            for (int k : {6, 7}) {
                const Json* lim = argument(call, std::size_t(k), names[k]);
                if (!lim || lim->isNull()) continue;
                if (!readNumber(*lim, &v)) return *error = where + ": " + names[k] + " must be a number", false;
                k == 6 ? im->norm->setVmin(v) : im->norm->setVmax(v);
            }
        }
        fig.images.push_back(std::move(im));
        return nameResult(call, nullptr, error);
    }

    // A 1-D or 2-D array of finite numbers (a grid's X or Y): rows x cols, row by row (a 1-D array is one row)
    bool gridArray(const Json& v, std::vector<std::vector<double>>* out) {
        out->clear();
        const Json* seq = sequence(v);
        if (!seq || seq->size() == 0) return false;
        const bool twoD = sequence((*seq)[0]) != nullptr;
        if (!twoD) {
            std::vector<double> row;
            if (!numbers(v, &row)) return false;
            out->push_back(row);
            return true;
        }
        for (const Json& r : seq->items()) {
            std::vector<double> row;
            if (!numbers(r, &row) || (!out->empty() && row.size() != out->front().size())) return false;
            out->push_back(row);
        }
        return true;
    }

    // The keywords of a pcolor collection: vmin / vmax (returned), alpha, edgecolors, line widths, snap, zorder,
    // label, colorizer (None); `skip` names those the caller takes itself
    bool meshKeywords(Collection& c, const model::Call& call, const std::vector<std::string>& skip, const std::string& where,
                      std::optional<double>* vmin, std::optional<double>* vmax, std::string* error) {
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            if (std::find(skip.begin(), skip.end(), name) != skip.end()) continue;
            if (name == "vmin" || name == "vmax") {
                if (value.isNull()) continue;
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "vmin" ? *vmin : *vmax) = v;
            } else if (name == "alpha") {
                if (value.isNull()) continue;
                if (!readAlpha(value, &v)) return *error = where + ": alpha must be a number from 0 to 1 (arrays are not supported yet)", false;
                c.setAlpha(v);
            } else if (name == "edgecolors" || name == "edgecolor" || name == "ec") {
                if (value.isString() && value.asString() == "none") c.setEdgecolor(ColorArg{ColorArg::None, {}});
                else if (value.isString() && value.asString() == "face") c.setEdgecolor(ColorArg{ColorArg::Face, {}});
                else if (value.isNull()) c.setEdgecolor(ColorArg{});
                else {
                    std::vector<colors::Spec> l;
                    if (!colorList(value, &l)) return *error = where + ": bad edgecolors", false;
                    c.setEdgecolor(ColorArg{ColorArg::Colors, l});
                }
            } else if (name == "linewidth" || name == "linewidths" || name == "lw") {
                std::vector<double> lw;
                if (!numbers(value, &lw) && !(readNumber(value, &v) && (lw = {v}, true))) return *error = where + ": bad linewidth", false;
                c.setLinewidths(lw);
            } else if (name == "snap") {
                if (value.isNull()) c.snap.reset();
                else if (value.isBool()) c.snap = value.asBool();
                else return *error = where + ": snap must be True, False or None", false;
            } else if (name == "zorder") {
                if (!readFinite(value, &v)) return *error = where + ": zorder must be a number", false;
                c.zorder = v;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                c.label = value.asString();
            } else if (name == "colorizer") {
                if (!value.isNull()) return *error = where + ": colorizer is not supported yet", false;
            } else {
                return *error = where + ": " + name + " is not supported yet", false;
            }
        }
        return true;
    }

    // Axes._pcolorargs(funcname, *args, shading=): X and Y as 2-D grids of the cells' corners (centres expanded),
    // C, and the shading ('flat' or 'gouraud' after 'auto' and 'nearest'). pcolormesh refuses non-finite X and Y;
    // pcolor masks them.
    bool pcolorArgs(const model::Call& call, const std::string& funcname, std::vector<std::vector<double>>* Xout,
                    std::vector<std::vector<double>>* Yout, ImageArray* Cout, std::string* shadingOut, std::string* error) {
        const std::string where = call.on + "." + funcname;
        std::vector<std::vector<double>>& X = *Xout;
        std::vector<std::vector<double>>& Y = *Yout;
        ImageArray& C = *Cout;
        std::string& shading = *shadingOut;
        if (!readArray(call.args.back(), &C, error)) return *error = where + ": " + *error, false;
        if (C.channels != 0 && C.channels != 3 && C.channels != 4) return *error = where + ": C must be (M, N), (M, N, 3) or (M, N, 4)", false;
        shading = rc_.text("pcolor.shading");
        if (const Json* sh = keyword(call, "shading"); sh && !sh->isNull()) {
            if (!sh->isString()) return *error = where + ": shading must be a string", false;
            shading = sh->asString();
        }
        for (char& ch : shading) ch = char(std::tolower((unsigned char)ch));
        if (shading != "gouraud" && shading != "nearest" && shading != "flat" && shading != "auto") shading = "auto";  // (a warning)
        const int nrows = C.rows, ncols = C.cols;
        if (call.args.size() == 1) {
            const bool corners = !(shading == "gouraud" || shading == "nearest");
            const int nx = corners ? ncols + 1 : ncols, ny = corners ? nrows + 1 : nrows;
            X.assign(ny, std::vector<double>(nx));
            Y.assign(ny, std::vector<double>(nx));
            for (int i = 0; i < ny; ++i)
                for (int j = 0; j < nx; ++j) X[i][j] = j, Y[i][j] = i;
            if (corners) shading = "flat";
        } else {
            const bool mesh = funcname == "pcolormesh";
            const std::string bad = where + ": x and y arguments to pcolormesh cannot have non-finite values or be of type numpy.ma.MaskedArray with masked values";
            if (!gridArray(call.args[0], &X) || !gridArray(call.args[1], &Y))
                return *error = mesh ? bad : where + ": X and Y must be arrays of numbers", false;
            for (const auto* g : {&X, &Y})
                for (const auto& row : *g)
                    for (double v : row)
                        if (!std::isfinite(v) && mesh) return *error = bad, false;
            // 1-D (or one-row) X repeated over the rows, 1-D (or one-column) Y over the columns
            const std::size_t Nx = X.front().size();
            const std::size_t Ny = Y.size() == 1 ? Y.front().size() : Y.size();
            if (X.size() == 1) {
                const std::vector<double> row = X.front();  // (assign must not read from the vector it fills)
                X.assign(Ny, row);
            }
            if (Y.size() == 1 || (Y.front().size() == 1)) {
                std::vector<double> col;
                if (Y.size() == 1) col = Y.front();
                else for (const auto& r : Y) col.push_back(r[0]);
                Y.assign(col.size(), std::vector<double>(Nx));
                for (std::size_t i = 0; i < col.size(); ++i) std::fill(Y[i].begin(), Y[i].end(), col[i]);
            }
            if (X.size() != Y.size() || X.front().size() != Y.front().size())
                return *error = where + ": Incompatible X, Y inputs to " + funcname + "; see help(" + funcname + ")", false;
        }
        const int Nx = int(X.front().size()), Ny = int(X.size());
        if (shading == "auto") shading = ncols == Nx && nrows == Ny ? "nearest" : "flat";
        if (shading == "flat") {
            if (Nx != ncols + 1 || Ny != nrows + 1)
                return *error = where + ": Dimensions of C should be one smaller than X(" + std::to_string(Nx) + ") and Y(" +
                                std::to_string(Ny) + ") while using shading='flat' see help(" + funcname + ")", false;
        } else {
            if (Nx != ncols || Ny != nrows)
                return *error = where + ": Dimensions of C are incompatible with X (" + std::to_string(Nx) + ") and/or Y (" +
                                std::to_string(Ny) + "); see help(" + funcname + ")", false;
            if (shading == "nearest") {
                // the corners halfway between the centres (_interp_grid along rows, then along columns)
                const auto interp = [](const std::vector<std::vector<double>>& g) {
                    std::vector<std::vector<double>> out;
                    for (const auto& row : g) {
                        std::vector<double> r;
                        if (row.size() > 1) {
                            std::vector<double> d;
                            for (std::size_t j = 0; j + 1 < row.size(); ++j) d.push_back((row[j + 1] - row[j]) * 0.5);
                            r.push_back(row[0] - d[0]);
                            for (std::size_t j = 0; j + 1 < row.size(); ++j) r.push_back(row[j] + d[j]);
                            r.push_back(row.back() + d.back());
                        } else {
                            r = {row[0], row[0]};
                        }
                        out.push_back(r);
                    }
                    return out;
                };
                const auto transpose = [](const std::vector<std::vector<double>>& g) {
                    std::vector<std::vector<double>> t(g.front().size(), std::vector<double>(g.size()));
                    for (std::size_t i = 0; i < g.size(); ++i)
                        for (std::size_t j = 0; j < g[i].size(); ++j) t[j][i] = g[i][j];
                    return t;
                };
                X = interp(X);
                Y = interp(Y);
                X = transpose(interp(transpose(X)));
                Y = transpose(interp(transpose(Y)));
                shading = "flat";
            }
        }
        return true;
    }

    // The colours of a pcolor-like collection: C mapped (its non-finite values masked), or RGB(A) cells
    bool meshColours(Collection& c, const ImageArray& C, const std::string& where, std::string* error) {
        if (C.channels == 0) {
            colors::MaskedArray values;
            values.data = C.data;
            values.dtype = C.dtype;
            values.mask.assign(C.size(), false);
            bool anyMasked = false;
            for (std::size_t i = 0; i < C.size(); ++i) {
                values.mask[i] = !std::isfinite(C.data[i]);
                anyMasked = anyMasked || values.mask[i];
            }
            if (!anyMasked) values.mask.clear();
            c.array = values;
            return true;
        }
        if (C.dtype != colors::DType::UInt8 && C.dtype != colors::DType::Float32 && C.dtype != colors::DType::Float64)
            return *error = where + ": Image RGB array must be uint8 or floating point; found " + colors::dtypeName(C.dtype), false;
        c.rgbChannels = C.channels;
        c.rgbBytes = C.dtype == colors::DType::UInt8;
        for (int i = 0; i < C.rows * C.cols; ++i) {
            colors::Rgba rgba{0, 0, 0, 1};
            for (int k = 0; k < C.channels; ++k) rgba[std::size_t(k)] = C.data[std::size_t(i) * C.channels + k];
            c.rgbData.push_back(rgba);
        }
        return true;
    }

    // _scale_norm of a collection: vmin / vmax (not with a norm object), then autoscale_None on its array
    bool scaleNorm(Collection& c, bool normObject, std::optional<double> vmin, std::optional<double> vmax,
                   const std::string& where, std::string* error) {
        if (vmin || vmax) {
            if (normObject)
                return *error = where + ": Passing a Normalize instance simultaneously with vmin/vmax is not supported.  "
                                        "Please pass vmin/vmax directly to the norm when creating it.", false;
            if (vmin) c.norm->setVmin(*vmin);
            if (vmax) c.norm->setVmax(*vmax);
        }
        if (c.array) c.norm->autoscaleNone(colors::processValue(*c.array));
        return true;
    }

    // Axes._update_pcolor_lims: sticky edges and data limits from the (finite) corners, an autoscale asked for
    void pcolorLimits(Axes& ax, Collection& c, const std::vector<std::array<double, 2>>& coords) {
        std::vector<std::array<double, 2>> finite;
        for (const auto& p : coords)
            if (std::isfinite(p[0]) && std::isfinite(p[1])) finite.push_back(p);
        if (!finite.empty()) {
            double minx = finite[0][0], maxx = minx, miny = finite[0][1], maxy = miny;
            for (const auto& p : finite) {
                minx = std::min(minx, p[0]), maxx = std::max(maxx, p[0]);
                miny = std::min(miny, p[1]), maxy = std::max(maxy, p[1]);
            }
            c.stickyX = {minx, maxx};
            c.stickyY = {miny, maxy};
        }
        ax.updateDatalim(finite);
        ax.requestAutoscaleView(true, true);
    }

    // Axes.contour and Axes.contourf([X, Y,] Z, /, [levels], **kwargs): a QuadContourSet of lines or filled regions,
    // its limits tight around the grid
    bool contour(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const std::size_t nargs = call.args.size();
        const bool triangular = call.method == "tricontour" || call.method == "tricontourf";
        if (!triangular && (nargs < 1 || nargs > 4))
            return *error = where + ": " + call.method + "() takes from 1 to 4 positional arguments but " + std::to_string(nargs) + " were given", false;
        if (call.as.size() > 1) return *error = where + ": a contour set is one object (\"as\" names one)", false;
        ContourArgs a;
        a.filled = call.method == "contourf" || call.method == "tricontourf";
        const Json* levels = keyword(call, "levels");
        if (triangular) {
            // TriContourSet._contour_args: the triangulation, then z, then the levels
            std::size_t next = 0;
            if (!triangulationOf(call, where, &a.triangulation, &next, error)) return false;
            if (nargs <= next) return *error = where + ": not enough values to unpack (expected at least 1, got 0)", false;
            if (!numbers(call.args[next], &a.triZ)) return *error = where + ": z must be a list of numbers", false;
            a.triZDtype = integers(call.args[next]) ? colors::DType::Int64 : colors::DType::Float64;
            if ((!levels || levels->isNull()) && nargs > next + 1) levels = &call.args[next + 1];
        } else {
        const auto dims = [](const Json& v) {  // np.ndim of nested lists (of an $array's data)
            const Json* e = &v;
            if (e->isObject() && e->members().size() == 1 && e->members()[0].first == "$array") e = e->members()[0].second.find("data");
            int d = 0;
            for (const Json* s = sequence(*e); s; s = s->size() ? sequence((*s)[0]) : nullptr) ++d;
            return d;
        };
        const std::size_t zIndex = nargs <= 2 ? 0 : 2;
        const Json& zJson = call.args[zIndex];
        if (const int d = dims(zJson); d == 1 || d == 3) return *error = where + ": Input z must be 2D, not " + std::to_string(d) + "D", false;
        if (!readArray(zJson, &a.z, error)) return *error = where + ": " + *error, false;
        if (zIndex == 2) {
            a.xDims = dims(call.args[0]), a.yDims = dims(call.args[1]);
            std::vector<std::vector<double>> x, y;
            if (a.xDims < 1 || a.xDims > 2 || a.yDims < 1 || a.yDims > 2 || !gridArray(call.args[0], &x) || !gridArray(call.args[1], &y))
                return *error = where + ": X and Y must be arrays of numbers of one or two dimensions", false;
            a.x = x, a.y = y;
        }
        // levels: positional after Z, or the keyword (which wins); an integer asks a locator for them
        if ((!levels || levels->isNull()) && (nargs == 2 || nargs == 4)) levels = &call.args.back();
        }
        if (levels && !levels->isNull()) {
            std::vector<double> l;
            if (levels->isBool()) a.nlevels = levels->asBool() ? 1 : 0;
            else if (levels->isInteger()) a.nlevels = (long long)levels->asNumber();
            else if (numbers(*levels, &l)) a.levels = l;
            else return *error = where + ": levels must be an integer or a sequence of numbers", false;
        }
        const Json* cmapJson = keyword(call, "cmap");
        a.cmapGiven = cmapJson && !cmapJson->isNull();
        bool normObject = false;
        if (!colorizer(&a.cmap, &a.norm, cmapJson, keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
        a.logNorm = normObject && a.norm->spec().kind == "LogNorm";
        const Json* normJson = keyword(call, "norm");
        if (!normJson || normJson->isNull()) a.norm = nullptr;  // (the set's own Normalize)
        std::optional<double> zorder;
        std::optional<std::string> label;
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            if (name == "levels" || name == "cmap" || name == "norm") continue;
            if (name == "color") return *error = where + ": ContourSet.__init__() got an unexpected keyword argument 'color'", false;
            if (triangular && (name == "triangles" || name == "mask")) continue;
            if (triangular && (name == "corner_mask" || name == "nchunk" || name == "algorithm"))
                return *error = where + ": TriContourSet.set() got an unexpected keyword argument '" + name + "'", false;
            const bool none = value.isNull();
            if (name == "linewidths") {
                if (none) continue;
                std::vector<double> lw;
                if (readNumber(value, &v)) lw = {v};
                else if (!numbers(value, &lw)) return *error = where + ": linewidths must be a number or a sequence of numbers", false;
                a.linewidths = lw;
            } else if (name == "linestyles" || name == "negative_linestyles") {
                if (none) continue;
                std::vector<std::string> styles;
                if (value.isString()) styles = {value.asString()};
                else if (const Json* seq = sequence(value)) {
                    for (const Json& st : seq->items()) {
                        if (!st.isString()) return *error = where + ": " + name + " as on-off ink sequences are not supported yet", false;
                        styles.push_back(st.asString());
                    }
                } else {
                    return *error = where + ": Unrecognized type for linestyles kwarg", false;
                }
                if (name == "linestyles") a.linestyles = styles;
                else if (styles.size() == 1) a.negativeLinestyles = styles[0];
                else return *error = where + ": negative_linestyles as an on-off ink sequence is not supported yet", false;
            } else if (name == "alpha") {
                if (none) continue;
                if (!readAlpha(value, &v)) return *error = where + ": alpha must be a number from 0 to 1 (arrays are not supported yet)", false;
                a.alpha = v;
            } else if (name == "origin") {
                if (none) continue;
                if (!value.isString()) return *error = where + ": " + repr(value) + " is not a valid value for origin. Supported values are None, 'lower', 'upper', 'image'", false;
                a.origin = value.asString();
            } else if (name == "extent") {
                if (none) continue;
                std::vector<double> e;
                if (!numbers(value, &e) || e.size() != 4) return *error = where + ": If given, 'extent' must be None or (x0, x1, y0, y1)", false;
                a.extent = std::array<double, 4>{e[0], e[1], e[2], e[3]};
            } else if (name == "colors") {
                if (none) continue;
                std::vector<colors::Spec> specs;
                if (!colorList(value, &specs)) return *error = where + ": colors must be a colour or a sequence of colours", false;
                std::vector<colors::Rgba> rgba;
                for (const auto& spec : specs) {
                    std::string why;
                    const auto c = colors::toRgba(spec, std::nullopt, rc_, &why);
                    if (!c) return *error = where + ": " + why, false;
                    rgba.push_back(*c);
                }
                a.colors = rgba;
            } else if (name == "vmin" || name == "vmax") {
                if (none) continue;
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "vmin" ? a.vmin : a.vmax) = v;
            } else if (name == "extend") {
                if (!value.isString()) return *error = where + ": extend must be one of 'neither', 'both', 'min', 'max'", false;
                a.extend = value.asString();
            } else if (name == "antialiased" || name == "corner_mask") {
                if (none) continue;
                if (!value.isBool()) return *error = where + ": " + name + " must be True, False or None", false;
                (name == "antialiased" ? a.antialiased : a.cornerMask) = value.asBool();
            } else if (name == "nchunk") {
                if (!value.isInteger()) return *error = where + ": nchunk must be an integer", false;
                a.nchunk = (long long)value.asNumber();
            } else if (name == "algorithm") {
                if (none) continue;
                if (!value.isString()) return *error = where + ": algorithm must be a string", false;
                a.algorithm = value.asString();
            } else if (name == "locator" || name == "transform" || name == "colorizer") {
                if (!none) return *error = where + ": " + name + " is not supported yet", false;
            } else if (name == "hatches") {
                // patterns or None, cycled over the regions (a string's characters, as Python indexes it); lines
                // never look at them. What is not iterable fails when matplotlib draws (here when made).
                if (!a.filled) continue;
                const auto notIterable = [&](const Json& item) {
                    const char* type = item.isNull() ? "NoneType" : item.isBool() ? "bool" : item.isInteger() ? "int" : item.isNumber() ? "float" : nullptr;
                    *error = where + (type ? ": '" + std::string(type) + "' object is not iterable" : ": hatches must be a sequence of patterns or None");
                    return false;
                };
                a.hatches.clear();
                if (value.isString()) {
                    for (char ch : value.asString()) a.hatches.push_back(std::string(1, ch));
                } else if (const Json* seq = sequence(value)) {
                    for (const Json& h : seq->items()) {
                        if (!h.isNull() && !h.isString()) return notIterable(h);
                        a.hatches.push_back(h.isNull() ? std::nullopt : std::optional<std::string>(h.asString()));
                    }
                } else {
                    return notIterable(value);
                }
            } else if (name == "zorder") {
                if (!readFinite(value, &v)) return *error = where + ": zorder must be a number", false;
                zorder = v;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                label = value.asString();
            } else {
                return *error = where + ": " + name + " is not supported yet", false;
            }
        }
        auto cs = ContourSet::make(rc_, a, error);
        if (!cs) return *error = where + ": " + *error, false;
        // self.set(**kwargs) over the defaults
        if (zorder) cs->zorder = *zorder;
        if (label) cs->label = *label;
        const std::array<double, 2> mins = cs->mins, maxs = cs->maxs;
        if (call.as.size() == 1) contourSets_[call.as[0]] = {cs.get(), &ax};
        ax.addCollection(std::move(cs), false);
        ax.updateDatalim({mins, maxs});
        ax.tight = true;  // autoscale_view(tight=True)
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // ContourSet.clabel(levels=None, *, fontsize=None, inline=True, inline_spacing=5, fmt=None, colors=None,
    // use_clabeltext=False, manual=False, rightside_up=True, zorder=None), and Axes.clabel(CS, levels=None,
    // **kwargs) which calls it: labels placed automatically on the named contour set
    bool clabel(const std::string& setName, const Json* levels, const model::Call& call, const std::string& where, std::string* error) {
        if (!call.as.empty()) return *error = where + ": naming the labels is not supported yet", false;
        const auto it = contourSets_.find(setName);
        if (it == contourSets_.end()) return *error = where + ": the contour set must be one the figure file named ({\"$ref\": name})", false;
        ContourSet& cs = *it->second.first;
        Axes& ax = *it->second.second;
        ClabelOptions o;
        if (levels && !levels->isNull()) {
            std::vector<double> l;
            if (!numbers(*levels, &l)) return *error = where + ": levels must be a list of numbers", false;
            o.levels = l;
            o.levelsText = repr(*levels);
        }
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            if (name == "levels" || name == "CS") continue;
            const bool none = value.isNull();
            if (name == "fontsize") {
                if (none) continue;
                if (readNumber(value, &v)) {
                    o.fontsize = text::atLeastOnePoint(v);
                } else if (value.isString() && text::fontSize(value.asString(), rc_)) {
                    o.fontsize = text::atLeastOnePoint(*text::fontSize(value.asString(), rc_));
                } else {
                    return *error = where + ": Size is invalid. Valid font size are xx-small, x-small, small, medium, large, x-large, xx-large, larger, smaller, None", false;
                }
            } else if (name == "inline" || name == "rightside_up") {
                if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name == "inline" ? o.inlineLabels : o.rightsideUp) = value.asBool();
            } else if (name == "inline_spacing") {
                if (!readNumber(value, &v)) return *error = where + ": inline_spacing must be a number", false;
                o.inlineSpacing = v;
            } else if (name == "fmt") {
                if (none) continue;
                if (!value.isString()) return *error = where + ": fmt as a formatter, a function or a dict is not supported yet", false;
                o.fmt = value.asString();
            } else if (name == "colors") {
                if (none) continue;
                std::vector<colors::Spec> specs;
                if (!colorList(value, &specs)) return *error = where + ": colors must be a colour or a sequence of colours", false;
                std::vector<colors::Rgba> rgba;
                for (const auto& spec : specs) {
                    std::string why;
                    const auto c = colors::toRgba(spec, std::nullopt, rc_, &why);
                    if (!c) return *error = where + ": " + why, false;
                    rgba.push_back(*c);
                }
                if (rgba.empty()) return *error = where + ": an empty list of colors is not supported yet", false;
                o.colors = rgba;
            } else if (name == "use_clabeltext" || name == "manual") {
                if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
                if (value.asBool()) return *error = where + ": " + name + "=True is not supported yet", false;
            } else if (name == "zorder") {
                if (none) continue;
                if (!readFinite(value, &v)) return *error = where + ": zorder must be a number", false;
                o.zorder = v;
            } else {
                return *error = where + ": ContourLabeler.clabel() got an unexpected keyword argument '" + name + "'", false;
            }
        }
        // fig._get_renderer(): the Agg canvas' renderer at the figure's dpi, which measures the labels
        const auto size = figure_->canvasSize();
        AggTraceRenderer renderer(size[0], size[1], figure_->dpi, figure_->rc(), options_.locale, options_.fontDirectory);
        if (!engine::clabel(cs, ax, renderer, o, error)) return *error = where + ": " + *error, false;
        if (!renderer.error().empty()) return *error = where + ": " + renderer.error(), false;
        return true;
    }

    // Axes.hexbin(x, y, C=None, gridsize=100, bins=None, xscale='linear', yscale='linear', extent=None, cmap=None,
    // norm=None, vmin=None, vmax=None, alpha=None, linewidths=None, edgecolors='face', reduce_C_function=np.mean,
    // mincnt=None, marginals=False, colorizer=None, **kwargs): a PolyCollection of one hexagon at the offsets of the
    // bins that have data
    bool hexbin(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".hexbin";
        static const char* const names[] = {"x", "y", "C", "gridsize", "bins", "xscale", "yscale", "extent", "cmap", "norm", "vmin",
                                            "vmax", "alpha", "linewidths", "edgecolors", "reduce_C_function", "mincnt", "marginals", "colorizer"};
        if (call.args.size() > std::size(names)) return *error = where + ": too many positional arguments", false;
        if (call.as.size() > 1) return *error = where + ": hexbin makes one collection (\"as\" names one)", false;
        model::Call c = call;  // (the positional arguments by name)
        c.args.clear();
        for (std::size_t i = 0; i < call.args.size(); ++i) c.kwargs.insert(c.kwargs.begin() + long(i), {names[i], call.args[i]});
        const auto get = [&](const char* name) -> const Json* {
            const Json* v = keyword(c, name);
            return v && !v->isNull() ? v : nullptr;
        };
        std::vector<double> x, y, C;
        if (!get("x") || !get("y")) return *error = where + ": hexbin() missing required arguments 'x' and 'y'", false;
        if (!numbers(*get("x"), &x) || !numbers(*get("y"), &y)) return *error = where + ": x and y must be lists of numbers", false;
        if (x.size() != y.size()) return *error = where + ": x and y of different lengths are not supported yet", false;
        const Json* cJson = get("C");
        bool cInteger = false;
        if (cJson) {
            if (!numbers(*cJson, &C) || C.size() != x.size()) return *error = where + ": C must be a list of numbers, one per point", false;
            cInteger = true;
            for (const Json& v : sequence(*cJson)->items()) cInteger = cInteger && v.isInteger() && !v.isBool();
        }
        // cbook.delete_masked_points: the points where x, y or C is not finite left out
        {
            std::vector<double> kx, ky, kc;
            for (std::size_t i = 0; i < x.size(); ++i)
                if (std::isfinite(x[i]) && std::isfinite(y[i]) && (!cJson || std::isfinite(C[i])))
                    kx.push_back(x[i]), ky.push_back(y[i]), cJson ? kc.push_back(C[i]) : void();
            x = kx, y = ky, C = kc;
        }
        // the grid's size
        long long nx = 100, ny = 0;
        if (const Json* g = get("gridsize")) {
            const Json* pair = sequence(*g);
            if (pair && pair->size() == 2 && (*pair)[0].isInteger() && (*pair)[1].isInteger()) {
                nx = (long long)(*pair)[0].asNumber(), ny = (long long)(*pair)[1].asNumber();
            } else if (g->isInteger() && !g->isBool()) {
                nx = (long long)g->asNumber();
                ny = (long long)(double(nx) / std::sqrt(3.0));
            } else {
                return *error = where + ": gridsize must be an integer or a pair of integers", false;
            }
        } else {
            ny = (long long)(double(nx) / std::sqrt(3.0));
        }
        if (nx < 1 || ny < 1 || nx > 2000 || ny > 2000) return *error = where + ": grids other than 1 to 2000 hexagons a side are not supported yet", false;
        // the scales: the logarithms of the data
        const auto scaleOf = [&](const char* name, std::string* out) {
            const Json* v = get(name);
            *out = "linear";
            if (!v) return true;
            if (!v->isString()) return *error = where + ": " + name + " must be 'linear' or 'log'", false;
            *out = v->asString();
            return true;
        };
        std::string xscale, yscale;
        if (!scaleOf("xscale", &xscale) || !scaleOf("yscale", &yscale)) return false;
        std::vector<double> tx = x, ty = y;
        if (xscale == "log") {
            for (double v : x)
                if (v <= 0.0) return *error = where + ": x contains non-positive values, so cannot be log-scaled", false;
            for (double& v : tx) v = support::log10(v);
        }
        if (yscale == "log") {
            for (double v : y)
                if (v <= 0.0) return *error = where + ": y contains non-positive values, so cannot be log-scaled", false;
            for (double& v : ty) v = support::log10(v);
        }
        double xmin = 0, xmax = 1, ymin = 0, ymax = 1;
        if (const Json* e = get("extent")) {
            std::vector<double> ext;
            if (!numbers(*e, &ext) || ext.size() != 4) return *error = where + ": extent must be (xmin, xmax, ymin, ymax)", false;
            xmin = ext[0], xmax = ext[1], ymin = ext[2], ymax = ext[3];
            if (xmin > xmax) return *error = where + ": In extent, xmax must be greater than xmin", false;
            if (ymin > ymax) return *error = where + ": In extent, ymax must be greater than ymin", false;
        } else {
            if (!x.empty()) xmin = support::npMin(tx), xmax = support::npMax(tx), ymin = support::npMin(ty), ymax = support::npMax(ty);
            std::tie(xmin, xmax) = axes::nonsingular(xmin, xmax, 0.1);
            std::tie(ymin, ymax) = axes::nonsingular(ymin, ymax, 0.1);
        }
        const long long nx1 = nx + 1, ny1 = ny + 1, nx2 = nx, ny2 = ny;
        const double padding = 1.e-9 * (xmax - xmin);
        xmin -= padding;
        xmax += padding;
        const double sx = (xmax - xmin) / double(nx), sy = (ymax - ymin) / double(ny);
        // .astype(int) of a float: truncation (x86 gives the smallest int64 for what does not fit)
        const auto toInt = [](double v) {
            return std::isfinite(v) && std::fabs(v) < 9.2e18 ? (long long)v : std::numeric_limits<long long>::min();
        };
        const std::size_t npts = x.size();
        std::vector<long long> i1(npts), i2(npts);
        std::vector<bool> bdist(npts);
        for (std::size_t i = 0; i < npts; ++i) {
            const double ix = (tx[i] - xmin) / sx, iy = (ty[i] - ymin) / sy;
            const long long ix1 = toInt(std::nearbyint(ix)), iy1 = toInt(std::nearbyint(iy));
            const long long ix2 = toInt(std::floor(ix)), iy2 = toInt(std::floor(iy));
            i1[i] = 0 <= ix1 && ix1 < nx1 && 0 <= iy1 && iy1 < ny1 ? ix1 * ny1 + iy1 + 1 : 0;
            i2[i] = 0 <= ix2 && ix2 < nx2 && 0 <= iy2 && iy2 < ny2 ? ix2 * ny2 + iy2 + 1 : 0;
            const double a = ix - double(ix1), b = iy - double(iy1), c2 = ix - double(ix2) - 0.5, d = iy - double(iy2) - 0.5;
            bdist[i] = a * a + 3.0 * (b * b) < c2 * c2 + 3.0 * (d * d);
        }
        // the value of each hexagon: its count, or C reduced; NaN where it has too few points
        const std::size_t n1 = std::size_t(nx1 * ny1), n2 = std::size_t(nx2 * ny2);
        std::vector<double> accum(n1 + n2, 0.0);
        std::optional<double> mincnt;
        if (const Json* m = get("mincnt")) {
            double v = 0;
            if (!readNumber(*m, &v)) return *error = where + ": mincnt must be a number", false;
            mincnt = v;
        }
        std::string reduce = "np.mean";
        if (const Json* f = get("reduce_C_function")) {
            if (!(f->isObject() && f->members().size() == 1 && f->members()[0].first == "$func"))
                return *error = where + ": reduce_C_function must be a function named with {\"$func\": name}", false;
            reduce = f->members()[0].second.asString();
        }
        // reduce_C_function of the values of a bin (NumPy's reductions of float64, or of int64 for integer C)
        const auto reduceValues = [&](const std::vector<double>& v) {
            if (reduce == "len") return double(v.size());
            if (reduce == "np.max") return support::npMax(v);
            if (reduce == "np.min") return support::npMin(v);
            if (reduce == "np.median") return support::npMedian(v, cInteger);
            if (reduce == "np.sum") {
                if (!cInteger) return support::npSum(v);
                long long total = 0;
                for (double d : v) total += (long long)d;
                return double(total);
            }
            return support::npMean(v, cInteger);
        };
        if (!cJson) {
            for (std::size_t i = 0; i < npts; ++i) {
                if (bdist[i] && i1[i]) accum[std::size_t(i1[i] - 1)] += 1;
                if (!bdist[i] && i2[i]) accum[n1 + std::size_t(i2[i] - 1)] += 1;
            }
            if (mincnt)
                for (double& a : accum)
                    if (a < *mincnt) a = std::numeric_limits<double>::quiet_NaN();
            C.assign(npts, 1.0);
        } else {
            std::vector<std::vector<double>> values(n1 + n2);
            for (std::size_t i = 0; i < npts; ++i) {
                if (bdist[i] && i1[i]) values[std::size_t(i1[i] - 1)].push_back(C[i]);
                if (!bdist[i] && i2[i]) values[n1 + std::size_t(i2[i] - 1)].push_back(C[i]);
            }
            const double least = mincnt ? *mincnt : 1;
            for (std::size_t k = 0; k < values.size(); ++k)
                accum[k] = double(values[k].size()) >= least ? reduceValues(values[k]) : std::numeric_limits<double>::quiet_NaN();
        }
        // the offsets of the hexagons with a value: the two grids, one shifted by half a hexagon
        std::vector<std::array<double, 2>> offsets;
        std::vector<double> values;
        for (std::size_t k = 0; k < n1 + n2; ++k) {
            if (std::isnan(accum[k])) continue;
            double ox, oy;
            if (k < n1) ox = double(k / std::size_t(ny1)), oy = double(k % std::size_t(ny1));
            else ox = double((k - n1) / std::size_t(ny2)) + 0.5, oy = double((k - n1) % std::size_t(ny2)) + 0.5;
            offsets.push_back({ox * sx + xmin, oy * sy + ymin});
            values.push_back(accum[k]);
        }
        static const double unit[6][2] = {{.5, -.5}, {.5, .5}, {0., 1.}, {-.5, .5}, {-.5, -.5}, {0., -1.}};
        std::vector<std::array<double, 2>> polygon;
        for (const auto& u : unit) polygon.push_back({sx * u[0], (sy / 3) * u[1]});
        if (xscale == "log" || yscale == "log") {
            for (auto& p : polygon) {
                if (xscale == "log") p[0] = std::pow(10.0, p[0]);
                if (yscale == "log") p[1] = std::pow(10.0, p[1]);
            }
            for (const char* axis : {"x", "y"}) {
                if ((axis[0] == 'x' ? xscale : yscale) != "log") continue;
                double& lo = axis[0] == 'x' ? xmin : ymin;
                double& hi = axis[0] == 'x' ? xmax : ymax;
                lo = std::pow(10.0, lo), hi = std::pow(10.0, hi);
                model::Call sc;
                sc.on = call.on;
                sc.method = axis[0] == 'x' ? "set_xscale" : "set_yscale";
                sc.args = {Json::string("log")};
                if (!axesCall(ax, sc, error)) return false;
            }
        }
        auto col = std::make_unique<PolyCollection>(rc_, std::vector<std::vector<std::array<double, 2>>>{polygon});
        col->offsets = offsets;
        col->offsetTransform = std::make_shared<transforms::AffineDeltaTransform>(ax.transData);
        col->setEdgecolor(ColorArg{ColorArg::Face, {}});
        col->setLinewidths({rc_.number("patch.linewidth")});
        // bins: 'log' makes the norm a LogNorm; a number or edges replace the values by their bin indices
        const Json* binsJson = get("bins");
        const Json* normJson = get("norm");
        std::optional<double> vmin, vmax;
        for (const char* name : {"vmin", "vmax"})
            if (const Json* v = get(name)) {
                double d = 0;
                if (!readNumber(*v, &d)) return *error = where + ": " + name + " must be a number", false;
                (name[1] == 'm' && name[2] == 'i' ? vmin : vmax) = d;
            }
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &col->norm, get("cmap"), normJson, nullptr, false, where, &normObject, error)) return false;
        col->cmap = cmap;
        colors::MaskedArray array;
        array.data = values;
        if (binsJson && binsJson->isString()) {
            if (binsJson->asString() != "log") return *error = where + ": bins must be 'log', a number of bins or their edges", false;
            if (!normJson) {
                colors::NormSpec spec;
                spec.kind = "LogNorm";
                spec.vmin = vmin, spec.vmax = vmax;
                col->norm = std::make_shared<colors::Norm>();
                if (!colors::Norm::make(spec, col->norm.get(), error)) return *error = where + ": " + *error, false;
                vmin.reset(), vmax.reset();
            }
        } else if (binsJson) {
            std::vector<double> edges;
            if (binsJson->isInteger() && !binsJson->isBool()) {
                if (values.empty()) return *error = where + ": min() iterable argument is empty", false;
                const double minimum = *std::min_element(values.begin(), values.end());
                const double maximum = *std::max_element(values.begin(), values.end());
                const long long nb = (long long)binsJson->asNumber() - 1;  // one less edge than bins
                for (long long k = 0; k < nb; ++k) edges.push_back(minimum + (maximum - minimum) * double(k) / double(nb));
            } else if (!numbers(*binsJson, &edges)) {
                return *error = where + ": bins must be 'log', a number of bins or their edges", false;
            }
            std::sort(edges.begin(), edges.end());
            for (double& v : array.data) v = double(std::lower_bound(edges.begin(), edges.end(), v) - edges.begin());
            array.dtype = colors::DType::Int64;
        }
        col->array = array;
        model::Call rest = c;  // the collection's own keywords (alpha, linewidths, edgecolors, zorder, ...)
        rest.kwargs.clear();
        for (const auto& kw : c.kwargs) {
            static const std::set<std::string> own = {"x", "y", "C", "gridsize", "bins", "xscale", "yscale", "extent", "cmap", "norm", "vmin",
                                                      "vmax", "reduce_C_function", "mincnt", "marginals"};
            if (own.count(kw.first) || ((kw.first == "linewidths" || kw.first == "alpha") && kw.second.isNull())) continue;
            rest.kwargs.push_back(kw);
        }
        if (!meshKeywords(*col, rest, {}, where, &vmin, &vmax, error)) return false;
        bool marginals = false;
        if (const Json* m = get("marginals")) {
            if (!m->isBool()) return *error = where + ": marginals must be True or False", false;
            marginals = m->asBool();
        }
        // the norm given (or bins' LogNorm) is shared with the marginals; else each has a Normalize of its own
        const std::shared_ptr<colors::Norm> sharedNorm = normJson || (binsJson && binsJson->isString()) ? col->norm : nullptr;
        const std::optional<double> alpha = col->alpha;
        if (!scaleNorm(*col, normObject, vmin, vmax, where, error)) return false;
        ax.updateDatalim({{xmin, ymin}, {xmax, ymax}});
        ax.tight = true;  // _request_autoscale_view(tight=True)
        ax.requestAutoscaleView(true, true);
        ax.addCollection(std::move(col), false);
        if (!marginals) return true;
        // the marginals: bars along the bottom and the left edges (5 % of the Axes) of C reduced over the points in
        // each of nx (2 ny) bins of x (y)
        model::Call extra = rest;  // (hexbin's **kwargs: what is not one of its parameters)
        extra.kwargs.clear();
        for (const auto& kw : rest.kwargs)
            if (kw.first != "alpha" && kw.first != "linewidths" && kw.first != "edgecolors") extra.kwargs.push_back(kw);
        for (int axis = 0; axis < 2; ++axis) {
            const bool isX = axis == 0;
            const std::vector<double>& z = isX ? x : y;
            const double zmin = isX ? xmin : ymin, zmax = isX ? xmax : ymax;
            const long long nbins = isX ? nx : 2 * ny;
            std::vector<double> edges;
            if ((isX ? xscale : yscale) == "log") {
                // np.geomspace: 10 ** linspace(log10(zmin), log10(zmax)), its ends zmin and zmax
                for (double e : support::npLinspace(support::log10(zmin), support::log10(zmax), nbins + 1)) edges.push_back(std::pow(10.0, e));
                edges.front() = zmin, edges.back() = zmax;
            } else {
                edges = support::npLinspace(zmin, zmax, nbins + 1);
            }
            std::vector<std::vector<double>> inBin(static_cast<std::size_t>(nbins));
            for (std::size_t i = 0; i < z.size(); ++i) {
                const long long b = (long long)(std::lower_bound(edges.begin(), edges.end(), z[i]) - edges.begin()) - 1;
                if (b >= 0 && b < nbins) inBin[std::size_t(b)].push_back(C[i]);
            }
            std::vector<std::vector<std::array<double, 2>>> verts;
            colors::MaskedArray barValues;
            for (long long b = 0; b < nbins; ++b) {
                if (inBin[std::size_t(b)].empty()) continue;
                const double value = reduceValues(inBin[std::size_t(b)]);
                if (std::isnan(value)) continue;
                const double e0 = edges[std::size_t(b)], e1 = edges[std::size_t(b) + 1];
                std::vector<std::array<double, 2>> v{{e0, 0.0}, {e0, .05}, {e1, .05}, {e1, 0.0}};
                if (!isX)
                    for (auto& pt : v) std::swap(pt[0], pt[1]);
                verts.push_back(v);
                barValues.data.push_back(value);
            }
            auto bar = std::make_unique<PolyCollection>(rc_, verts);
            bar->transform = isX ? ax.xaxisTransform : ax.yaxisTransform;  // get_xaxis_transform(which="grid")
            bar->transformSet = true;
            bar->setEdgecolor(ColorArg{ColorArg::Face, {}});
            bar->array = barValues;
            bar->cmap = cmap;
            bar->norm = sharedNorm ? sharedNorm : std::make_shared<colors::Norm>();
            bar->setAlpha(alpha);
            std::optional<double> ignoredMin, ignoredMax;
            if (!meshKeywords(*bar, extra, {}, where, &ignoredMin, &ignoredMax, error)) return false;
            ax.addCollection(std::move(bar), false);
        }
        return true;
    }

    // A 1-D or 2-D array of numbers (np.atleast_1d of a number, a list or a list of lists), its rows and columns
    // (one row for 1-D), raveled
    bool raveled(const Json& v, std::vector<double>* out, std::size_t* rows, std::size_t* cols) {
        double d = 0;
        out->clear();
        if (readNumber(v, &d)) return *out = {d}, *rows = 1, *cols = 1, true;
        const Json* s = sequence(v);
        if (!s) return false;
        if (s->size() && sequence((*s)[0])) {
            *rows = s->size(), *cols = 0;
            for (const Json& row : s->items()) {
                std::vector<double> r;
                if (!numbers(row, &r) || (*cols && r.size() != *cols)) return false;
                *cols = r.size();
                out->insert(out->end(), r.begin(), r.end());
            }
            return true;
        }
        if (!numbers(v, out)) return false;
        *rows = 1, *cols = out->size();
        return true;
    }

    // quiver's _parse_args: U, V (and C) after X, Y when given; X, Y on U's grid when they match its shape, else
    // U's grid of indices
    bool vectorArgs(const model::Call& call, const std::string& where, std::vector<double>* X, std::vector<double>* Y,
                    std::vector<double>* U, std::vector<double>* V, std::vector<double>* C, std::string* error) {
        const std::size_t nargs = call.args.size();
        const std::size_t first = nargs >= 4 ? 2 : 0;
        std::size_t nr = 0, nc = 0, r = 0, c = 0;
        const char* bad = ": X, Y, U, V and C must be numbers or arrays of numbers of one or two dimensions";
        if (!raveled(call.args[first], U, &nr, &nc) || !raveled(call.args[first + 1], V, &r, &c)) return *error = where + bad, false;
        if ((nargs == 3 || nargs == 5) && !raveled(call.args.back(), C, &r, &c)) return *error = where + bad, false;
        if (first == 2) {
            if (!raveled(call.args[0], X, &r, &c) || !raveled(call.args[1], Y, &r, &c)) return *error = where + bad, false;
            if (X->size() == nc && Y->size() == nr) {
                std::vector<double> mx, my;
                for (double yv : *Y)
                    for (double xv : *X) mx.push_back(xv), my.push_back(yv);
                *X = mx, *Y = my;
            } else if (X->size() != Y->size()) {
                return *error = where + ": X and Y must be the same size, but X.size is " + std::to_string(X->size()) + " and Y.size is " +
                                std::to_string(Y->size()) + ".", false;
            }
        } else {
            for (std::size_t i = 0; i < nr; ++i)
                for (std::size_t j = 0; j < nc; ++j) X->push_back(double(j)), Y->push_back(double(i));
        }
        return true;
    }

    // Axes.barbs([X, Y], U, V, [C], /, *, pivot='tip', length=7, barbcolor=None, flagcolor=None, sizes=None,
    // fill_empty=False, barb_increments=None, rounding=True, flip_barb=False, **kwargs): a Barbs collection
    bool barbs(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".barbs";
        const std::size_t nargs = call.args.size();
        if (nargs < 2 || nargs > 5)
            return *error = where + ": barbs() takes from 2 to 5 positional arguments but " + std::to_string(nargs) + " were given", false;
        if (call.as.size() > 1) return *error = where + ": barbs make one collection (\"as\" names one)", false;
        std::vector<double> X, Y, U, V, C;
        if (!vectorArgs(call, where, &X, &Y, &U, &V, &C, error)) return false;
        const bool hasC = nargs == 3 || nargs == 5;
        BarbsOptions o;
        std::string pivot = "tip";
        std::optional<double> pivotNumber;
        std::optional<colors::Spec> barbcolor, flagcolor;
        model::Call rest = call;
        rest.kwargs.clear();
        const Json* cmapJson = nullptr;
        const Json* normJson = nullptr;
        bool linewidthGiven = false, faceGiven = false;
        std::optional<colors::Spec> face;
        std::optional<colors::Spec> both;
        // a {"$dict": {...}} of numbers by name
        const auto numberDict = [&](const Json& value, const std::string& name, const std::map<std::string, double*>& into) {
            if (value.isNull()) return true;
            const Json* d = value.isObject() && value.members().size() == 1 && value.members()[0].first == "$dict" ? &value.members()[0].second : nullptr;
            if (!d) return *error = where + ": " + name + " must be a dict ({\"$dict\": {...}})", false;
            for (const auto& [k, v] : d->members()) {
                double n = 0;
                const auto it = into.find(k);
                if (it == into.end()) continue;  // (other keys are not read)
                if (!readNumber(v, &n)) return *error = where + ": " + name + "['" + k + "'] must be a number", false;
                *it->second = n;
            }
            return true;
        };
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            if (name == "pivot") {
                if (readNumber(value, &v)) pivotNumber = v;
                else if (value.isString()) pivot = value.asString();
                else return *error = where + ": pivot must be 'tip', 'middle' or a number", false;
            } else if (name == "length") {
                if (!readNumber(value, &v)) return *error = where + ": length must be a number", false;
                o.length = v;
            } else if (name == "barbcolor" || name == "flagcolor") {
                if (value.isNull()) continue;
                colors::Spec spec;
                if (!color(value, &spec)) return *error = where + ": " + name + " must be a colour (colour lists are not supported yet)", false;
                (name == "barbcolor" ? barbcolor : flagcolor) = spec;
            } else if (name == "sizes") {
                if (!numberDict(value, name, {{"spacing", &o.spacing}, {"height", &o.height}, {"width", &o.width}, {"emptybarb", &o.emptybarb}})) return false;
            } else if (name == "barb_increments") {
                if (!numberDict(value, name, {{"half", &o.half}, {"full", &o.full}, {"flag", &o.flag}})) return false;
            } else if (name == "fill_empty" || name == "rounding") {
                if (!value.isBool()) return *error = where + ": " + name + " must be True or False", false;
                (name == "fill_empty" ? o.fillEmpty : o.rounding) = value.asBool();
            } else if (name == "flip_barb") {
                if (value.isBool()) {
                    o.flip = {value.asBool()};
                } else if (const Json* seq = sequence(value)) {
                    o.flip.clear();
                    for (const Json& f : seq->items()) {
                        if (!f.isBool()) return *error = where + ": flip_barb must be True, False or a list of them", false;
                        o.flip.push_back(f.asBool());
                    }
                } else {
                    return *error = where + ": flip_barb must be True, False or a list of them", false;
                }
            } else if (name == "cmap" || name == "norm") {
                (name == "cmap" ? cmapJson : normJson) = &value;
            } else if (name == "facecolor" || name == "facecolors" || name == "fc") {
                colors::Spec spec;
                if (!color(value, &spec)) return *error = where + ": " + name + " must be a colour (colour lists are not supported yet)", false;
                face = spec, faceGiven = true;
            } else if (name == "color") {
                colors::Spec spec;
                if (!color(value, &spec)) return *error = where + ": color must be a colour (colour lists are not supported yet)", false;
                both = spec;
            } else if (name == "transform") {
                return *error = where + ": transform is not supported yet", false;
            } else {
                linewidthGiven = linewidthGiven || name == "linewidth" || name == "lw";
                rest.kwargs.push_back({name, value});
            }
        }
        // the pivot: a number (float(pivot)), else 'tip' or 'middle'
        if (pivotNumber) {
            o.endy = *pivotNumber;
        } else {
            double parsed = 0;
            std::string lower = pivot;
            for (char& ch : lower) ch = char(std::tolower((unsigned char)ch));
            if (support::readDouble(pivot, parsed)) o.endy = parsed;
            else if (lower == "tip") o.endy = 0.0;
            else if (lower == "middle") o.endy = -o.length / 2.;
            else return *error = where + ": '" + lower + "'", false;  // (KeyError)
        }
        std::optional<std::vector<double>> cvalues;
        if (hasC) cvalues = C;
        auto b = Barbs::make(rc_, ax, X, Y, U, V, cvalues, o, error);
        if (!b) return *error = where + ": " + *error, false;
        // the flags' colour (facecolors) and the barbs' (edgecolors): the same unless both are given
        if (!barbcolor || !flagcolor) {
            b->setEdgecolor(ColorArg{ColorArg::Face, {}});
            if (flagcolor) b->setFacecolor(ColorArg::of(*flagcolor));
            else if (barbcolor) b->setFacecolor(ColorArg::of(*barbcolor));
            else b->setFacecolor(ColorArg::of(faceGiven ? *face : colors::Spec(std::string("k"))));
        } else {
            b->setEdgecolor(ColorArg::of(*barbcolor));
            b->setFacecolor(ColorArg::of(*flagcolor));
        }
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &b->norm, cmapJson, normJson, nullptr, false, where, &normObject, error)) return false;
        b->cmap = cmap;
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*b, rest, {}, where, &vmin, &vmax, error)) return false;
        if (vmin || vmax) return *error = where + ": vmin and vmax are not supported yet", false;
        if (both) b->setFacecolor(ColorArg::of(*both)), b->setEdgecolor(ColorArg::of(*both));  // (set_color, after)
        if (!linewidthGiven) b->setLinewidths({1.0});  // (kwargs['linewidth'] = 1, applied last)
        ax.addCollection(std::move(b), true);
        return true;
    }

    // Axes.streamplot(x, y, u, v, density=1, linewidth=None, color=None, cmap=None, norm=None, arrowsize=1,
    // arrowstyle='-|>', minlength=0.1, transform=None, zorder=None, start_points=None, maxlength=4.0,
    // integration_direction='both', broken_streamlines=True, *, integration_max_step_scale=1.0,
    // integration_max_error_scale=1.0, num_arrows=1): a LineCollection of the streamlines and their arrows
    bool streamplot(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".streamplot";
        static const char* const names[] = {"x", "y", "u", "v", "density", "linewidth", "color", "cmap", "norm", "arrowsize", "arrowstyle",
                                            "minlength", "transform", "zorder", "start_points", "maxlength", "integration_direction",
                                            "broken_streamlines"};
        if (call.args.size() > std::size(names)) return *error = where + ": too many positional arguments", false;
        if (!call.as.empty()) return *error = where + ": naming the streamplot's result is not supported yet", false;
        model::Call c = call;
        c.args.clear();
        for (std::size_t i = 0; i < call.args.size(); ++i) c.kwargs.insert(c.kwargs.begin() + long(i), {names[i], call.args[i]});
        static const std::set<std::string> known = {"x", "y", "u", "v", "density", "linewidth", "color", "cmap", "norm", "arrowsize",
                                                    "arrowstyle", "minlength", "transform", "zorder", "start_points", "maxlength",
                                                    "integration_direction", "broken_streamlines", "integration_max_step_scale",
                                                    "integration_max_error_scale", "num_arrows"};
        for (const auto& kw : c.kwargs)
            if (!known.count(kw.first)) return *error = where + ": streamplot() got an unexpected keyword argument '" + kw.first + "'", false;
        const auto get = [&](const char* name) -> const Json* {
            const Json* v = keyword(c, name);
            return v && !v->isNull() ? v : nullptr;
        };
        for (const char* required : {"x", "y", "u", "v"})
            if (!get(required)) return *error = where + ": streamplot() missing a required argument: '" + required + "'", false;
        const auto isArray = [](const Json& v) { return v.isObject() && v.members().size() == 1 && v.members()[0].first == "$array"; };
        // Grid(x, y): 1-D, or 2-D with equal rows (x) and columns (y)
        const auto axisOf = [&](const Json& value, bool isX, std::vector<double>* out) {
            std::vector<double> flat;
            std::size_t rows = 0, cols = 0;
            if (!raveled(value, &flat, &rows, &cols)) return *error = where + ": x and y must be arrays of numbers", false;
            const bool twoD = sequence(isArray(value) ? *value.members()[0].second.find("data") : value) &&
                              sequence((*sequence(isArray(value) ? *value.members()[0].second.find("data") : value))[0]);
            if (!twoD) return *out = flat, true;
            const std::size_t n = isX ? cols : rows;
            out->clear();
            for (std::size_t k = 0; k < n; ++k) out->push_back(isX ? flat[k] : flat[k * cols]);
            for (std::size_t r = 0; r < rows; ++r)
                for (std::size_t k = 0; k < cols; ++k) {
                    const double first = isX ? flat[k] : flat[r * cols];
                    const double v = flat[r * cols + k];
                    if (!(std::fabs(v - first) <= 1e-8 + 1e-5 * std::fabs(first)))  // np.allclose(x_row, x)
                        return *error = where + (isX ? ": The rows of 'x' must be equal" : ": The columns of 'y' must be equal"), false;
                }
            return true;
        };
        std::vector<double> xs, ys;
        if (!axisOf(*get("x"), true, &xs) || !axisOf(*get("y"), false, &ys)) return false;
        StreamGrid grid;
        if (!streamGrid(xs, ys, &grid, error)) return *error = where + ": " + *error, false;
        StreamOptions o;
        double v = 0;
        if (const Json* d = get("density")) {
            std::vector<double> pair;
            if (readNumber(*d, &v)) o.densityX = o.densityY = v;
            else if (numbers(*d, &pair) && pair.size() == 2) o.densityX = pair[0], o.densityY = pair[1];
            else return *error = where + ": 'density' must be a scalar or be of length 2", false;
        }
        double stepScale = 1.0, errorScale = 1.0, arrowsize = 1, numArrows = 1;
        const auto number = [&](const char* name, double* out) {
            const Json* j = get(name);
            if (!j) return true;
            if (!readNumber(*j, out)) return *error = where + ": " + name + " must be a number", false;
            return true;
        };
        if (!number("integration_max_step_scale", &stepScale) || !number("integration_max_error_scale", &errorScale) ||
            !number("num_arrows", &numArrows) || !number("arrowsize", &arrowsize) || !number("minlength", &o.minlength) ||
            !number("maxlength", &o.maxlength))
            return false;
        if (o.densityX * 30 < 0 || o.densityY * 30 < 0) return *error = where + ": 'density' must be positive", false;
        if (stepScale <= 0.0) return *error = where + ": The value of integration_max_step_scale must be > 0, got " + repr(*get("integration_max_step_scale")), false;
        if (errorScale <= 0.0) return *error = where + ": The value of integration_max_error_scale must be > 0, got " + repr(*get("integration_max_error_scale")), false;
        if (numArrows < 0) return *error = where + ": The value of num_arrows must be >= 0, got num_arrows=" + repr(*get("num_arrows")), false;
        if (numArrows != std::floor(numArrows)) return *error = where + ": num_arrows must be an integer", false;
        o.maxStepScale = stepScale, o.maxErrorScale = errorScale;
        double zorder = 2;  // Line2D.zorder
        if (!number("zorder", &zorder)) return false;
        if (get("transform")) return *error = where + ": transform is not supported yet", false;
        // the colour (the lines' next one), or an array of values mapped through the colormap
        const Json* colorJson = get("color");
        const bool multicolor = colorJson && isArray(*colorJson);
        colors::Spec color;
        if (!colorJson) color = ax.nextLineColor();
        else if (!multicolor && !this->color(*colorJson, &color)) return *error = where + ": color must be a colour or an array of values ({\"$array\": ...})", false;
        const Json* lwJson = get("linewidth");
        const bool varyingWidth = lwJson && isArray(*lwJson);
        double linewidth = rc_.number("lines.linewidth");
        if (lwJson && !varyingWidth && !readNumber(*lwJson, &linewidth)) return *error = where + ": linewidth must be a number or an array of widths ({\"$array\": ...})", false;
        std::string arrowstyle = "-|>";
        if (const Json* a = get("arrowstyle")) {
            if (!a->isString()) return *error = where + ": arrowstyle must be a string", false;
            arrowstyle = a->asString();
        }
        if (const Json* d = get("integration_direction")) {
            if (!d->isString() || (d->asString() != "both" && d->asString() != "forward" && d->asString() != "backward"))
                return *error = where + ": " + repr(*d) + " is not a valid value for integration_direction. Supported values are 'both', 'forward', 'backward'", false;
            o.direction = d->asString();
        }
        if (const Json* b = get("broken_streamlines")) {
            if (!b->isBool()) return *error = where + ": broken_streamlines must be True or False", false;
            o.broken = b->asBool();
        }
        // the arrays of the grid's shape (np.ndarray: lists have no shape)
        const std::size_t cells = std::size_t(grid.nx) * std::size_t(grid.ny);
        const auto gridArray_ = [&](const Json& value, const std::string& message, std::vector<double>* out) {
            if (!isArray(value)) return *error = where + ": 'list' object has no attribute 'shape'", false;
            ImageArray a;
            if (!readArray(value, &a, error)) return *error = where + ": " + *error, false;
            if (a.channels != 0 || a.rows != grid.ny || a.cols != grid.nx) return *error = where + ": " + message, false;
            out->resize(cells);
            for (std::size_t i = 0; i < cells; ++i)
                (*out)[i] = a.masked(i) || !std::isfinite(a.data[i]) ? std::numeric_limits<double>::quiet_NaN() : a.data[i];  // masked_invalid
            return true;
        };
        std::vector<double> colorValues, widths, u, v2;
        if (multicolor && !gridArray_(*colorJson, "If 'color' is given, it must match the shape of the (x, y) grid", &colorValues)) return false;
        if (varyingWidth && !gridArray_(*lwJson, "If 'linewidth' is given, it must match the shape of the (x, y) grid", &widths)) return false;
        const char* uvShape = "'u' and 'v' must match the shape of the (x, y) grid";
        if (!isArray(*get("u")) || !isArray(*get("v"))) return *error = where + ": 'list' object has no attribute 'shape'", false;
        if (!gridArray_(*get("u"), uvShape, &u) || !gridArray_(*get("v"), uvShape, &v2)) return false;
        if (const Json* sp = get("start_points")) {
            const Json* seq = sequence(*sp);
            std::vector<std::array<double, 2>> points;
            for (std::size_t i = 0; seq && i < seq->size(); ++i) {
                std::vector<double> pt;
                if (!numbers((*seq)[i], &pt) || pt.size() != 2) return *error = where + ": start_points must be a list of (x, y) points", false;
                points.push_back({pt[0], pt[1]});
            }
            if (!seq) return *error = where + ": start_points must be a list of (x, y) points", false;
            o.startPoints = points;
        }
        std::vector<std::vector<std::array<double, 2>>> trajectories;
        if (!streamTrajectories(grid, u, v2, o, &trajectories, error)) return *error = where + ": " + *error, false;
        // the multicolour lines' norm (Normalize of the values' extremes) and colormap
        std::shared_ptr<colors::Norm> norm;
        colors::Colormap cmap;
        bool normObject = false;
        if (multicolor) {
            if (!colorizer(&cmap, &norm, get("cmap"), get("norm"), nullptr, false, where, &normObject, error)) return false;
            if (!get("norm")) {
                double lo = std::numeric_limits<double>::infinity(), hi = -lo;
                for (double cv : colorValues)
                    if (!std::isnan(cv)) lo = std::min(lo, cv), hi = std::max(hi, cv);
                colors::NormSpec spec;
                spec.vmin = lo, spec.vmax = hi;
                norm = std::make_shared<colors::Norm>();
                if (!colors::Norm::make(spec, norm.get(), error)) return *error = where + ": " + *error, false;
            }
        }
        // interpgrid of an array along a trajectory (vectorised: masked where a corner is)
        const auto along = [&](const std::vector<double>& a, const std::vector<std::array<double, 2>>& t) {
            std::vector<double> out;
            for (const auto& p : t) {
                const int x = int(p[0]), y = int(p[1]);
                const int xn = std::min(std::max(x + 1, 0), grid.nx - 1), yn = std::min(std::max(y + 1, 0), grid.ny - 1);
                const auto at = [&](int r, int col) { return a[std::size_t(r) * std::size_t(grid.nx) + std::size_t(col)]; };
                const double xt = p[0] - x, yt = p[1] - y;
                const double a0 = at(y, x) * (1 - xt) + at(y, xn) * xt;
                const double a1 = at(yn, x) * (1 - xt) + at(yn, xn) * xt;
                out.push_back(a0 * (1 - yt) + a1 * yt);
            }
            return out;
        };
        std::vector<std::vector<std::array<double, 2>>> streamlines;
        std::vector<double> segmentWidths;
        colors::MaskedArray segmentValues;
        std::vector<std::unique_ptr<FancyArrowPatch>> arrows;
        const double xData2grid = 1. / grid.dx, yData2grid = 1. / grid.dy;
        for (const auto& t : trajectories) {
            std::vector<double> tx, ty;
            for (const auto& p : t) tx.push_back(p[0] / xData2grid + grid.xOrigin), ty.push_back(p[1] / yData2grid + grid.yOrigin);
            if (varyingWidth || multicolor) {
                for (std::size_t i = 0; i + 1 < tx.size(); ++i) streamlines.push_back({{tx[i], ty[i]}, {tx[i + 1], ty[i + 1]}});
            } else {
                std::vector<std::array<double, 2>> points;
                for (std::size_t i = 0; i < tx.size(); ++i) points.push_back({tx[i], ty[i]});
                streamlines.push_back(points);
            }
            // the distance along the streamline
            std::vector<double> dist;
            double total = 0;
            for (std::size_t i = 0; i + 1 < tx.size(); ++i) dist.push_back(total += std::hypot(tx[i + 1] - tx[i], ty[i + 1] - ty[i]));
            std::vector<double> lineWidths, lineValues;
            if (varyingWidth) {
                lineWidths = along(widths, t);
                lineWidths.pop_back();
                segmentWidths.insert(segmentWidths.end(), lineWidths.begin(), lineWidths.end());
            }
            if (multicolor) {
                lineValues = along(colorValues, t);
                lineValues.pop_back();
                for (double cv : lineValues) segmentValues.data.push_back(cv), segmentValues.mask.push_back(std::isnan(cv));
            }
            // the arrows along it, at fractions of its length
            for (long k = 1; k <= long(numArrows); ++k) {
                if (dist.empty()) return *error = where + ": index -1 is out of bounds for axis 0 with size 0", false;
                const double target = dist.back() * (double(k) / double(long(numArrows) + 1));
                const std::size_t idx = std::size_t(std::lower_bound(dist.begin(), dist.end(), target) - dist.begin());
                const std::vector<double> hx(tx.begin() + long(idx), tx.begin() + long(std::min(idx + 2, tx.size())));
                const std::vector<double> hy(ty.begin() + long(idx), ty.begin() + long(std::min(idx + 2, ty.size())));
                auto arrow = std::make_unique<FancyArrowPatch>(rc_, bezier::Point{tx[idx], ty[idx]}, bezier::Point{support::npMean(hx), support::npMean(hy)});
                if (!ArrowStyle::make(arrowstyle, {}, &arrow->arrowStyle, error)) return *error = where + ": " + *error, false;
                arrow->mutationScale = 10 * arrowsize;
                colors::Spec arrowColor = color;
                if (multicolor) {
                    // cmap(norm(value)): the norm autoscaled by the value when it has no limits yet (it is the
                    // lines' norm too). A masked value is a scalar there, which process_value takes as an unmasked
                    // NaN (the colormap's bad colour, but 0 when vmin == vmax).
                    colors::MaskedArray one;
                    one.data = {lineValues[idx]};
                    std::vector<colors::Rgba> rgba;
                    if (!colors::mapColors(one, *norm, cmap, std::nullopt, &rgba, error)) return *error = where + ": " + *error, false;
                    arrowColor = colors::Spec(std::vector<double>(rgba[0].begin(), rgba[0].end()));
                }
                arrow->setFacecolor(arrowColor);  // (color: face and edge)
                arrow->setEdgecolor(arrowColor);
                arrow->setLinewidth(varyingWidth ? lineWidths[idx] : linewidth);
                arrow->zorder = zorder;
                arrow->transform = ax.transData;
                arrow->transformSet = true;
                arrows.push_back(std::move(arrow));
            }
        }
        auto lc = std::make_unique<LineCollection>(rc_, streamlines);
        lc->transform = ax.transData;
        lc->transformSet = true;
        if (!multicolor) lc->setEdgecolor(ColorArg::of(color));
        lc->setLinewidths(varyingWidth ? segmentWidths : std::vector<double>{linewidth});
        lc->zorder = zorder;
        lc->stickyX = {grid.xOrigin, grid.xOrigin + grid.width};
        lc->stickyY = {grid.yOrigin, grid.yOrigin + grid.height};
        if (multicolor) {
            lc->array = segmentValues;
            lc->cmap = cmap;
            lc->norm = norm;
        }
        ax.addCollection(std::move(lc), true);
        for (auto& arrow : arrows) ax.addPatch(std::move(arrow));
        ax.requestAutoscaleView(true, true);  // autoscale_view(), at once
        ax.unstaleViewLim();
        return true;
    }

    // Axes.quiver([X, Y], U, V, [C], /, **kwargs): a Quiver of arrows at X, Y
    bool quiver(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".quiver";
        const std::size_t nargs = call.args.size();
        if (nargs < 2 || nargs > 5)
            return *error = where + ": quiver() takes from 2 to 5 positional arguments but " + std::to_string(nargs) + " were given", false;
        if (call.as.size() > 1) return *error = where + ": a quiver is one object (\"as\" names one)", false;
        std::vector<double> X, Y, U, V, C;
        if (!vectorArgs(call, where, &X, &Y, &U, &V, &C, error)) return false;
        const bool hasC = nargs == 3 || nargs == 5;
        QuiverOptions o;
        static const char* const unitNames[] = {"x", "y", "xy", "width", "height", "dots", "inches"};
        const auto validUnits = [&](const std::string& value) {
            if (std::find(std::begin(unitNames), std::end(unitNames), value) != std::end(unitNames)) return true;
            return *error = where + ": '" + value + "' is not a valid value for units.", false;  // (raised when drawn)
        };
        std::optional<colors::Spec> color = colors::Spec(std::string("k"));
        std::optional<std::vector<colors::Spec>> colorList_;
        model::Call rest = call;  // the PolyCollection's keywords
        rest.kwargs.clear();
        std::vector<std::pair<std::string, Json>> polykw;
        const Json* cmapJson = nullptr;
        const Json* normJson = nullptr;
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            const bool none = value.isNull();
            if (name == "scale" || name == "width") {
                if (none) continue;
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "scale" ? o.scale : o.width) = v;
            } else if (name == "headwidth" || name == "headlength" || name == "headaxislength" || name == "minshaft" || name == "minlength") {
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "headwidth" ? o.headwidth : name == "headlength" ? o.headlength : name == "headaxislength" ? o.headaxislength
                 : name == "minshaft" ? o.minshaft : o.minlength) = v;
            } else if (name == "units" || name == "scale_units") {
                if (none && name == "scale_units") continue;
                if (!value.isString()) return *error = where + ": " + name + " must be a string", false;
                if (name == "units" || value.asString() != "xy")
                    if (!validUnits(value.asString())) return false;
                if (name == "units") o.units = value.asString();
                else o.scaleUnits = value.asString();
            } else if (name == "angles") {
                std::vector<double> a;
                if (value.isString()) {
                    if (value.asString() != "uv" && value.asString() != "xy") return *error = where + ": angles must be 'uv', 'xy' or an array of angles", false;
                    o.angles = value.asString();
                } else if (numbers(value, &a) || (readNumber(value, &v) && (a = {v}, true))) {
                    if (a.size() != X.size()) return *error = where + ": angles must be one per arrow (others are not supported yet)", false;
                    o.anglesArray = a;
                } else {
                    return *error = where + ": angles must be 'uv', 'xy' or an array of angles", false;
                }
            } else if (name == "pivot") {
                if (!value.isString()) return *error = where + ": pivot must be 'tail', 'middle' or 'tip'", false;
                o.pivot = value.asString();
            } else if (name == "color" || name == "facecolor" || name == "facecolors" || name == "fc") {
                if (name != "color" && none) continue;
                std::vector<colors::Spec> l;
                colors::Spec one;
                if (this->color(value, &one)) colorList_ = std::vector<colors::Spec>{one};
                else if (colorList(value, &l)) colorList_ = l;
                else return *error = where + ": " + name + " must be a colour or a list of colours", false;
                polykw.push_back({name, value});
            } else if (name == "cmap" || name == "norm") {
                (name == "cmap" ? cmapJson : normJson) = &value;
                polykw.push_back({name, value});
            } else if (name == "transform") {
                return *error = where + ": transform is not supported yet", false;
            } else {
                rest.kwargs.push_back({name, value});
                polykw.push_back({name, value});
            }
        }
        std::optional<std::vector<double>> cvalues;
        if (hasC) cvalues = C;
        auto q = Quiver::make(rc_, ax, X, Y, U, V, cvalues, o, error);
        if (!q) return *error = where + ": " + *error, false;
        if (colorList_) q->setFacecolor(ColorArg{ColorArg::Colors, *colorList_});
        else q->setFacecolor(ColorArg::of(*color));
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &q->norm, cmapJson, normJson, nullptr, false, where, &normObject, error)) return false;
        q->cmap = cmap;
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*q, rest, {}, where, &vmin, &vmax, error)) return false;
        if (vmin || vmax) return *error = where + ": vmin and vmax are not supported yet", false;
        Quiver* ref = q.get();
        ax.addCollection(std::move(q), true);
        if (call.as.size() == 1) quivers_[call.as[0]] = {ref, polykw};
        return true;
    }

    // Axes.quiverkey(Q, X, Y, U, label, *, angle=0, coordinates='axes', color=None, labelsep=0.1, labelpos='N',
    // labelcolor=None, fontproperties=None, zorder=None, **kwargs): a QuiverKey added to the Axes
    bool quiverkey(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".quiverkey";
        static const char* const positional[] = {"Q", "X", "Y", "U", "label"};
        if (call.args.size() > 5) return *error = where + ": quiverkey() takes 6 positional arguments but " + std::to_string(call.args.size() + 1) + " were given", false;
        const Json* given[5];
        for (std::size_t i = 0; i < 5; ++i) {
            given[i] = argument(call, i, positional[i]);
            if (!given[i]) return *error = where + ": quiverkey() missing 1 required positional argument: '" + positional[i] + "'", false;
        }
        if (!call.as.empty()) return *error = where + ": naming the key is not supported yet", false;
        const Json& qref = *given[0];
        const auto it = qref.isObject() && qref.members().size() == 1 && qref.members()[0].first == "$ref" && qref.members()[0].second.isString()
                            ? quivers_.find(qref.members()[0].second.asString()) : quivers_.end();
        if (it == quivers_.end()) return *error = where + ": Q must be a quiver the figure file named ({\"$ref\": name})", false;
        double x = 0, y = 0, u = 0;
        if (!readNumber(*given[1], &x) || !readNumber(*given[2], &y) || !readNumber(*given[3], &u))
            return *error = where + ": X, Y and U must be numbers", false;
        if (!given[4]->isString()) return *error = where + ": label must be a string", false;
        Quiver& q = *it->second.first;
        auto key = std::make_unique<QuiverKey>(rc_, q, x, y, u, given[4]->asString());
        // the arrow takes the quiver's PolyCollection keywords, then the key's own
        model::Call vectorKw = call;
        vectorKw.args.clear();
        vectorKw.kwargs.clear();
        std::optional<colors::Spec> face = colors::Spec(std::string("k"));
        std::vector<std::pair<std::string, Json>> kws = it->second.second;
        static const std::set<std::string> own = {"Q", "X", "Y", "U", "label", "angle", "coordinates", "color", "labelsep", "labelpos",
                                                  "labelcolor", "fontproperties", "zorder"};
        for (const auto& kw : call.kwargs)
            if (!own.count(kw.first)) kws.push_back(kw);
        key->vector->setLinewidths({0.0});
        for (const auto& [name, value] : kws) {
            if (name == "color" || name == "facecolor" || name == "facecolors" || name == "fc") {
                std::vector<colors::Spec> l;
                colors::Spec one;
                if (color(value, &one)) key->vector->setFacecolor(ColorArg::of(one));
                else if (colorList(value, &l)) key->vector->setFacecolor(ColorArg{ColorArg::Colors, l});
            } else if (name != "cmap" && name != "norm") {
                vectorKw.kwargs.push_back({name, value});
            }
        }
        bool faceSet = false;
        for (const auto& kw : kws) faceSet = faceSet || kw.first == "color" || kw.first == "facecolor" || kw.first == "facecolors" || kw.first == "fc";
        if (!faceSet) key->vector->setFacecolor(ColorArg::of(*face));
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*key->vector, vectorKw, {}, where, &vmin, &vmax, error)) return false;
        std::vector<std::pair<std::string, const Json*>> textKw;
        for (const auto& [name, value] : call.kwargs) {
            double v = 0;
            if (name == "angle" || name == "labelsep") {
                if (!readNumber(value, &v)) return *error = where + ": " + name + " must be a number", false;
                (name == "angle" ? key->angle : key->labelsep) = v;
            } else if (name == "coordinates") {
                static const char* const coords[] = {"data", "axes", "figure", "inches"};
                if (!value.isString() || std::find(std::begin(coords), std::end(coords), value.asString()) == std::end(coords))
                    return *error = where + ": " + repr(value) + " is not a valid value for coordinates. Supported values are 'data', 'axes', 'figure', 'inches'", false;
                key->coordinates = value.asString();
            } else if (name == "color") {
                if (value.isNull()) continue;
                colors::Spec spec;
                if (!color(value, &spec)) return *error = where + ": color must be a colour", false;
                key->color = spec;
            } else if (name == "labelpos") {
                if (!value.isString() || value.asString().size() != 1 || std::string("NSEW").find(value.asString()) == std::string::npos)
                    return *error = where + ": " + repr(value), false;  // (KeyError)
                key->labelpos = value.asString();
            } else if (name == "labelcolor") {
                if (!value.isNull()) textKw.push_back({"color", &value});
            } else if (name == "fontproperties") {
                if (value.isNull()) continue;
                const Json* d = value.isObject() && value.members().size() == 1 && value.members()[0].first == "$dict" ? &value.members()[0].second : nullptr;
                if (!d) return *error = where + ": fontproperties must be a dict ({\"$dict\": {...}})", false;
                static const std::map<std::string, std::string> fontKeys = {{"family", "fontfamily"}, {"style", "fontstyle"}, {"variant", "fontvariant"},
                                                                            {"weight", "fontweight"}, {"stretch", "fontstretch"}, {"size", "fontsize"}};
                for (const auto& [k, fv] : d->members()) {
                    const auto f = fontKeys.find(k);
                    if (f == fontKeys.end()) return *error = where + ": FontProperties.__init__() got an unexpected keyword argument '" + k + "'", false;
                    textKw.push_back({f->second, &fv});
                }
            } else if (name == "zorder") {
                if (value.isNull()) continue;
                if (!readFinite(value, &v)) return *error = where + ": zorder must be a number", false;
                key->zorder = v;
            }
        }
        static const std::map<char, std::pair<const char*, const char*>> align = {
            {'N', {"center", "bottom"}}, {'S', {"center", "top"}}, {'E', {"left", "center"}}, {'W', {"right", "center"}}};
        key->text->horizontalAlignment = align.at(key->labelpos[0]).first;
        key->text->verticalAlignment = align.at(key->labelpos[0]).second;
        if (!textProperties(*key->text, textKw, TextKeywords::Construct, where, error)) return false;
        ax.addOtherArtist(std::move(key));
        return true;
    }

    // Axes.pcolor([X, Y,] C, *, shading=None, alpha=None, norm=None, cmap=None, vmin=None, vmax=None, colorizer=None,
    // **kwargs): a PolyQuadMesh of the cells, without those with a masked corner or value
    bool pcolor(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".pcolor";
        if (call.args.size() != 1 && call.args.size() != 3)
            return *error = where + ": pcolor() takes 1 or 3 positional arguments but " + std::to_string(call.args.size()) + " were given", false;
        if (!call.as.empty()) return *error = where + ": naming pcolor's collection is not supported yet", false;
        std::vector<std::vector<double>> X, Y;
        ImageArray C;
        std::string shading;
        if (!pcolorArgs(call, "pcolor", &X, &Y, &C, &shading, error)) return false;
        const int rows = int(X.size()), cols = int(X.front().size());
        if (C.rows != rows - 1 || C.cols != cols - 1)  // (gouraud: _MeshData.set_array of a flat mesh)
            return *error = where + ": For X (" + std::to_string(cols) + ") and Y (" + std::to_string(rows) +
                            ") with flat shading, A should have shape (" + std::to_string(rows - 1) + ", " + std::to_string(cols - 1) + ")", false;
        std::vector<std::array<double, 2>> coords;
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) coords.push_back({X[i][j], Y[i][j]});
        std::vector<bool> cellMasked(std::size_t(C.rows) * C.cols, false);
        const int ch = C.channels ? C.channels : 1;
        for (std::size_t i = 0; i < C.size(); ++i)
            if (!std::isfinite(C.data[i])) cellMasked[i / std::size_t(ch)] = true;
        auto mesh = std::make_unique<PolyQuadMesh>(rc_, rows, cols, coords, cellMasked);
        if (!meshColours(*mesh, C, where, error)) return false;
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &mesh->norm, keyword(call, "cmap"), keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
        mesh->cmap = cmap;
        // linewidths 0.25, edgecolors 'none' and snap False unless given; not antialiased without edges
        mesh->setLinewidths({0.25});
        mesh->setEdgecolor(ColorArg{ColorArg::None, {}});
        mesh->snap = false;
        const Json* ec = keyword(call, "edgecolors");
        if (!ec) ec = keyword(call, "edgecolor");
        const Json* aa = keyword(call, "antialiased");
        if (!aa) aa = keyword(call, "antialiaseds");
        if (!aa && (!ec || (ec->isString() && ec->asString() == "none"))) mesh->setAntialiaseds({false});
        if (aa) {
            if (!aa->isBool()) return *error = where + ": antialiased must be True or False", false;
            mesh->setAntialiaseds({aa->asBool()});
        }
        std::optional<double> vmin, vmax;
        if (!meshKeywords(*mesh, call, {"shading", "antialiased", "antialiaseds", "cmap", "norm"}, where, &vmin, &vmax, error)) return false;
        if (!scaleNorm(*mesh, normObject, vmin, vmax, where, error)) return false;
        Collection& ref = ax.addCollection(std::move(mesh), false);
        pcolorLimits(ax, ref, coords);
        return true;
    }

    // Axes.pcolorfast([X, Y,] C, *, alpha=None, norm=None, cmap=None, vmin=None, vmax=None, colorizer=None, **kwargs):
    // an image for a uniform grid, a PcolorImage for 1-D edges, a QuadMesh for 2-D corners
    bool pcolorfast(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".pcolorfast";
        if (call.args.size() != 1 && call.args.size() != 3)
            return *error = where + ": pcolorfast() takes 1 or 3 positional arguments but " + std::to_string(call.args.size()) + " were given", false;
        if (!call.as.empty()) return *error = where + ": naming pcolorfast's result is not supported yet", false;
        ImageArray C;
        if (!readArray(call.args.back(), &C, error)) return *error = where + ": " + *error, false;
        const int nr = C.rows, nc = C.cols;
        std::string style;
        std::vector<std::vector<double>> X, Y;
        std::vector<double> x, y;
        if (call.args.size() == 1) {
            style = "image";
            x = {0, double(nc)};
            y = {0, double(nr)};
        } else {
            if (!gridArray(call.args[0], &X) || !gridArray(call.args[1], &Y)) return *error = where + ": X and Y must be arrays of numbers", false;
            const bool x1 = sequence(call.args[0]) && !sequence((*sequence(call.args[0]))[0]);
            const bool y1 = sequence(call.args[1]) && !sequence((*sequence(call.args[1]))[0]);
            if (x1 && y1) {
                x = X.front();
                y = Y.front();
                if (x.size() == 2 && y.size() == 2) {
                    style = "image";
                } else {
                    if (int(x.size()) != nc + 1)
                        return *error = where + ": Length of X (" + std::to_string(x.size()) + ") must be one larger than the number of columns in C (" + std::to_string(nc) + ")", false;
                    if (int(y.size()) != nr + 1)
                        return *error = where + ": Length of Y (" + std::to_string(y.size()) + ") must be one larger than the number of rows in C (" + std::to_string(nr) + ")", false;
                    // uniform when the steps vary by less than 1 % of their mean
                    const auto uniform = [](const std::vector<double>& v) {
                        std::vector<double> d;
                        for (std::size_t i = 0; i + 1 < v.size(); ++i) d.push_back(v[i + 1] - v[i]);
                        const auto [mn, mx] = std::minmax_element(d.begin(), d.end());
                        return *mx - *mn < 0.01 * std::fabs(support::npMean(d));
                    };
                    style = uniform(x) && uniform(y) ? "image" : "pcolorimage";
                }
            } else if (!x1 && !y1) {
                style = "quadmesh";
            } else {
                return *error = where + ": When 3 positional parameters are passed to pcolorfast, the first two (X and Y) must be both 1D or both 2D", false;
            }
        }
        std::optional<double> vmin, vmax;
        for (const char* name : {"vmin", "vmax"})
            if (const Json* v = keyword(call, name); v && !v->isNull()) {
                double n = 0;
                if (!readNumber(*v, &n)) return *error = where + ": " + name + " must be a number", false;
                (name[1] == 'm' && name[2] == 'i' ? vmin : vmax) = n;
            }
        bool normObject = false;
        double xl, xr, yb, yt;
        Artist* ret = nullptr;
        if (style == "quadmesh") {
            if (X.size() != Y.size() || X.front().size() != Y.front().size()) return *error = where + ": X and Y must have the same shape", false;
            const int rows = int(X.size()), cols = int(X.front().size());
            std::vector<std::array<double, 2>> coords;
            for (int i = 0; i < rows; ++i)
                for (int j = 0; j < cols; ++j) coords.push_back({X[i][j], Y[i][j]});
            if (rows != nr + 1 || cols != nc + 1) return *error = where + ": C must have one row and one column less than X and Y", false;
            auto mesh = std::make_unique<QuadMesh>(rc_, rows, cols, coords, false, false);
            if (!meshColours(*mesh, C, where, error)) return false;
            colors::Colormap cmap;
            if (!colorizer(&cmap, &mesh->norm, keyword(call, "cmap"), keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
            mesh->cmap = cmap;
            mesh->setEdgecolor(ColorArg{ColorArg::None, {}});
            double a = 0;
            if (const Json* al = keyword(call, "alpha"); al && !al->isNull()) {
                if (!readAlpha(*al, &a)) return *error = where + ": alpha must be a number from 0 to 1", false;
                mesh->setAlpha(a);
            }
            if (C.channels == 0 && !scaleNorm(*mesh, normObject, vmin, vmax, where, error)) return false;
            xl = coords[0][0], xr = xl, yb = coords[0][1], yt = yb;
            for (const auto& p : coords) xl = std::min(xl, p[0]), xr = std::max(xr, p[0]), yb = std::min(yb, p[1]), yt = std::max(yt, p[1]);
            ret = &ax.addCollection(std::move(mesh), false);
        } else {
            xl = x.front(), xr = x.back(), yb = y.front(), yt = y.back();
            std::unique_ptr<AxesImage> im = style == "image" ? std::make_unique<AxesImage>(rc_, ax) : std::make_unique<PcolorImage>(rc_, ax);
            if (!colorizer(&im->cmap, &im->norm, keyword(call, "cmap"), keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
            im->extent = std::array<double, 4>{xl, xr, yb, yt};
            if (style == "image") {
                im->interpolation = "nearest";
                im->origin = "lower";
            }
            if (!im->setData(C, error)) return *error = where + ": " + *error, false;
            if (style == "pcolorimage" && !static_cast<PcolorImage&>(*im).setGrid(x, y, error)) return *error = where + ": " + *error, false;
            if (!imageAlpha(*im, keyword(call, "alpha"), where, error)) return false;
            for (const auto& [name, value] : call.kwargs) {
                if (name == "cmap" || name == "norm" || name == "alpha" || name == "vmin" || name == "vmax") continue;
                bool taken = false;
                if (!imageProperty(*im, name, value, where, &taken, error)) return false;
                if (!taken) return *error = where + ": " + name + " is not supported yet", false;
            }
            if (C.channels == 0) {
                if (vmin || vmax) {
                    if (normObject)
                        return *error = where + ": Passing a Normalize instance simultaneously with vmin/vmax is not supported.  "
                                                "Please pass vmin/vmax directly to the norm when creating it.", false;
                    if (vmin) im->norm->setVmin(*vmin);
                    if (vmax) im->norm->setVmax(*vmax);
                }
                colors::MaskedArray raw;
                raw.data = im->data().data;
                raw.mask = im->data().mask;
                raw.dtype = im->data().dtype;
                im->norm->autoscaleNone(colors::processValue(raw));
            }
            ax.clipToPatch(*im);
            ret = &ax.addImage(std::move(im));
        }
        ret->stickyX = {xl, xr};
        ret->stickyY = {yb, yt};
        ax.updateDatalim({{xl, yb}, {xr, yt}});
        ax.tight = true;  // _request_autoscale_view(tight=True)
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // Axes.pcolormesh([X, Y,] C, *, alpha=None, norm=None, cmap=None, vmin=None, vmax=None, colorizer=None,
    // shading=None, antialiased=False, **kwargs): a QuadMesh of the cells (_pcolorargs, _update_pcolor_lims)
    bool pcolormesh(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".pcolormesh";
        if (call.args.size() != 1 && call.args.size() != 3)
            return *error = where + ": pcolormesh() takes 1 or 3 positional arguments but " + std::to_string(call.args.size()) + " were given", false;
        if (call.as.size() > 1) return *error = where + " returns one object: name it once", false;
        std::vector<std::vector<double>> X, Y;
        ImageArray C;
        std::string shading;
        if (!pcolorArgs(call, "pcolormesh", &X, &Y, &C, &shading, error)) return false;
        const int rows = int(X.size()), cols = int(X.front().size());
        std::vector<std::array<double, 2>> coords;
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) coords.push_back({X[i][j], Y[i][j]});
        bool antialiased = false;
        if (const Json* a = keyword(call, "antialiased"); a && !a->isNull()) {
            if (!a->isBool()) return *error = where + ": antialiased must be True or False", false;
            antialiased = a->asBool();
        }
        auto mesh = std::make_unique<QuadMesh>(rc_, rows, cols, coords, shading == "gouraud", antialiased);
        mesh->snap = rc_.boolean("pcolormesh.snap");
        // the colours: C mapped (2-D), or RGB(A) itself; masked where not finite
        if (C.channels == 0) {
            colors::MaskedArray values;
            values.data = C.data;
            values.dtype = C.dtype;
            values.mask.assign(C.size(), false);
            bool anyMasked = false;
            for (std::size_t i = 0; i < C.size(); ++i) {
                values.mask[i] = !std::isfinite(C.data[i]);
                anyMasked = anyMasked || values.mask[i];
            }
            if (!anyMasked) values.mask.clear();
            mesh->array = values;
        } else {
            mesh->rgbChannels = C.channels;
            mesh->rgbBytes = C.dtype == colors::DType::UInt8 || (C.dtype != colors::DType::Float32 && C.dtype != colors::DType::Float64);
            if (C.dtype == colors::DType::Int64) mesh->rgbBytes = false;  // (int64 is neither uint8 nor float: refused below)
            if (C.dtype != colors::DType::UInt8 && C.dtype != colors::DType::Float32 && C.dtype != colors::DType::Float64)
                return *error = where + ": Image RGB array must be uint8 or floating point; found " + colors::dtypeName(C.dtype), false;
            for (int i = 0; i < C.rows * C.cols; ++i) {
                colors::Rgba c{0, 0, 0, 1};
                for (int k = 0; k < C.channels; ++k) c[std::size_t(k)] = C.data[std::size_t(i) * C.channels + k];
                mesh->rgbData.push_back(c);
            }
        }
        bool normObject = false;
        colors::Colormap cmap;
        if (!colorizer(&cmap, &mesh->norm, keyword(call, "cmap"), keyword(call, "norm"), nullptr, false, where, &normObject, error)) return false;
        mesh->cmap = cmap;
        std::optional<double> vmin, vmax;
        mesh->setEdgecolor(ColorArg{ColorArg::None, {}});  // kwargs.setdefault('edgecolors', 'none')
        if (!meshKeywords(*mesh, call, {"shading", "antialiased", "cmap", "norm"}, where, &vmin, &vmax, error)) return false;
        // _scale_norm
        if (vmin || vmax) {
            if (normObject)
                return *error = where + ": Passing a Normalize instance simultaneously with vmin/vmax is not supported.  "
                                        "Please pass vmin/vmax directly to the norm when creating it.", false;
            if (vmin) mesh->norm->setVmin(*vmin);
            if (vmax) mesh->norm->setVmax(*vmax);
        }
        if (mesh->array) mesh->norm->autoscaleNone(colors::processValue(*mesh->array));
        // _update_pcolor_lims: added without autolim; sticky edges and data limits from the corners
        double minx = coords[0][0], maxx = minx, miny = coords[0][1], maxy = miny;
        for (const auto& c : coords) {
            minx = std::min(minx, c[0]), maxx = std::max(maxx, c[0]);
            miny = std::min(miny, c[1]), maxy = std::max(maxy, c[1]);
        }
        mesh->stickyX = {minx, maxx};
        mesh->stickyY = {miny, maxy};
        ax.addCollection(std::move(mesh), false);
        ax.updateDatalim(coords);
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // Axes.add_patch(p): a patch the figure file made, into the Axes (its data limits grow by it; no
    // autoscale is asked for)
    bool addPatch(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".add_patch";
        const Json* p = argument(call, 0, "p");
        const bool ref = p && p->isObject() && p->members()[0].first == "$ref";
        const std::string name = ref ? p->members()[0].second.asString() : "";
        if (ref && addedPatches_.count(name)) return *error = where + ": " + name + " is in an Axes already (not supported yet)", false;
        const auto it = patches_.find(name);
        if (!ref || it == patches_.end())
            return *error = where + ": the patch must be one the figure file made (a {\"$ref\": name})", false;
        ax.addPatch(std::move(it->second));
        patches_.erase(it);
        addedPatches_.insert(name);
        return true;
    }

    // Axes.fill(*args, data=None, **kwargs): (x, y[, colour]) groups, as plot reads them
    // (_process_plot_var_args('Polygon')), one Polygon each (_make_polygon): its face colour is the
    // group's colour or, when the keywords do not leave every property of the patches' cycle set, the
    // next entry's; the entry's other properties (markers left out) go with the keywords, which
    // Polygon.set then sets in their order. `made`: the polygons, for hist.
    bool fill(Axes& ax, const model::Call& call, std::string* error, std::vector<Polygon*>* made = nullptr) {
        const std::string where = call.on + ".fill";
        if (!givenOnce(call, lineAliases(), where, error)) return false;  // (Line2D's aliases, "for compatibility")
        if (const Json* d = keyword(call, "data"); d && !d->isNull()) return *error = where + ": data is not supported yet", false;
        std::vector<std::pair<std::string, const Json*>> props;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "data") continue;
            if (name == "x" || name == "y") return *error = where + ": got an unexpected keyword argument '" + name + "'", false;
            props.emplace_back(fullName(name, lineAliases()), &value);
        }
        // Polygon.set's own normalize_kwargs: a patch property by two of its names is refused
        static const Aliases patchAliases = {{"aa", "antialiased"}, {"ec", "edgecolor"}, {"fc", "facecolor"},
                                             {"ls", "linestyle"}, {"lw", "linewidth"}};
        for (std::size_t i = 0; i < props.size(); ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (fullName(props[i].first, patchAliases) == fullName(props[j].first, patchAliases))
                    return *error = where + ": Got both '" + props[j].first + "' and '" + props[i].first +
                                    "', which are aliases of one another",
                           false;
        const auto find = [&](const std::string& name) -> const Json* {
            for (const auto& [n, v] : props)
                if (n == name) return v;
            return nullptr;
        };
        const Json* label = find("label");
        if (call.args.size() >= 4 && label && sequence(*label))
            return *error = where + ": plot() with multiple groups of data (i.e., pairs of x and y) does not support multiple labels", false;
        static const Json none;  // (null)
        static const Json yes = Json::boolean(true);
        const Json* closedValue = find("closed");
        if (!closedValue) closedValue = &yes;  // kw['closed'] = kwargs.get('closed', True)
        std::optional<bool> closed;
        if (!closedValue->isNull()) {
            double n = 0;
            if (closedValue->isBool()) closed = closedValue->asBool();
            else if (readNumber(*closedValue, &n)) closed = n != 0;
            else return *error = where + ": closed must be True, False or None", false;
        }
        std::size_t i = 0;
        while (i < call.args.size()) {
            std::vector<const Json*> group;
            for (std::size_t k = i; k < std::min(i + 2, call.args.size()); ++k) group.push_back(&call.args[k]);
            i += group.size();
            if (i < call.args.size() && call.args[i].isString()) group.push_back(&call.args[i++]);
            std::optional<std::string> linestyle, marker;
            std::optional<colors::Spec> fmtColor;
            if (group.size() > 1 && group.back()->isString()) {
                if (!plotFormat(group.back()->asString(), &linestyle, &marker, &fmtColor, error)) return false;
                group.pop_back();
            } else if (group.size() == 3) {
                return *error = where + ": third arg must be a format string", false;
            }
            std::vector<double> x, y;
            if (group.size() == 2) {
                if (!numbers(*group[0], &x) || !numbers(*group[1], &y))
                    return *error = where + ": x and y must be lists of numbers", false;
            } else {
                if (!numbers(*group[0], &y)) return *error = where + ": y must be a list of numbers", false;
                for (std::size_t k = 0; k < y.size(); ++k) x.push_back(double(k));  // index_of
            }
            if (x.size() != y.size()) return *error = where + ": x and y must have same first dimension", false;

            // _make_polygon: the cycle's defaults for kw (the format's properties and closed), leaving out the
            // markers and whatever the keywords set
            std::vector<std::string> ignore = {"marker", "markersize", "markeredgecolor", "markerfacecolor", "markeredgewidth"};
            for (const auto& [name, value] : props)
                if (!value->isNull()) ignore.push_back(name);
            if (!label) ignore.erase(std::remove(ignore.begin(), ignore.end(), "label"), ignore.end());
            std::vector<std::string> kw;
            if (linestyle) kw.push_back("linestyle");
            if (marker) kw.push_back("marker");
            if (fmtColor) kw.push_back("color");
            if (!closedValue->isNull()) kw.push_back("closed");
            std::optional<colors::Spec> face = fmtColor;
            // kwargs = {**kwargs, 'label': label}, then the defaults but the colour where kwargs say nothing
            auto polyProps = props;
            if (!label) polyProps.emplace_back("label", &none);
            const auto defaults = ax.patchDefaults(kw, ignore);
            for (const auto& d : defaults) {
                if (d.first == "color") {
                    if (!face) face = colors::specOf(d.second);
                    continue;
                }
                auto slot = std::find_if(polyProps.begin(), polyProps.end(), [&](const auto& p) { return p.first == d.first; });
                if (slot == polyProps.end()) polyProps.emplace_back(d.first, &d.second);
                else if (slot->second->isNull()) slot->second = &d.second;
            }
            std::vector<double> xy;
            for (std::size_t k = 0; k < x.size(); ++k) xy.push_back(x[k]), xy.push_back(y[k]);
            // Polygon(xy, facecolor=, fill=kwargs.get('fill', True), closed=kw['closed']), then seg.set(**kwargs)
            auto poly = std::make_unique<Polygon>(rc_, std::move(xy), closed);
            if (const Json* f = find("fill"); f && !patchProperty(*poly, "fill", *f, where, error)) return false;
            poly->setFacecolor(face);
            for (const auto& [name, value] : polyProps)
                if (!patchProperty(*poly, name, *value, where, error)) return false;
            Patch& added = ax.addPatch(std::move(poly));
            if (made) made->push_back(static_cast<Polygon*>(&added));
        }
        ax.requestAutoscaleView(true, true);
        return true;
    }

    // Axes.axhspan(ymin, ymax, xmin=0, xmax=1, **kwargs) and axvspan(xmin, xmax, ymin=0, ymax=1, **kwargs)
    bool axspan(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const bool horizontal = call.method == "axhspan";
        markDateUnits(ax, call);
        const char* names[4] = {horizontal ? "ymin" : "xmin", horizontal ? "ymax" : "xmax", horizontal ? "xmin" : "ymin",
                                horizontal ? "xmax" : "ymax"};
        double v[4] = {0, 0, 0, 1};
        for (int i = 0; i < 4; ++i) {
            const Json* a = argument(call, std::size_t(i), names[i]);
            if (!a) {
                if (i < 2) return *error = where + ": " + names[i] + " is required", false;
                continue;
            }
            if (!readNumber(*a, &v[i])) return *error = where + ": " + names[i] + " must be a number", false;
        }
        // Rectangle((xmin, ymin), xmax - xmin, ymax - ymin)
        const double x0 = horizontal ? v[2] : v[0], x1 = horizontal ? v[3] : v[1];
        const double y0 = horizontal ? v[0] : v[2], y1 = horizontal ? v[1] : v[3];
        auto rect = std::make_unique<Rectangle>(rc_, x0, y0, x1 - x0, y1 - y0);
        // Rectangle(..., **kwargs): Patch's own parameters, then the rest through their setters, the
        // aliases among them. A color among the parameters takes the place of edgecolor and facecolor.
        const Json* ownColor = keyword(call, "color");
        for (const auto& [name, value] :
             ownFirst(call, {"edgecolor", "facecolor", "color", "linewidth", "linestyle"})) {
            bool positional = false;
            for (const char* n : names) positional = positional || name == n;
            if (positional) continue;
            if (ownColor && (name == "edgecolor" || name == "facecolor")) {
                colors::Spec unused;  // (not used, but it has to be a colour for the call to be taken)
                if (!color(*value, &unused)) return *error = where + ": " + name + " is not a colour", false;
                continue;
            }
            if (!patchProperty(*rect, name, *value, where, error)) return false;
        }
        rect->transform = horizontal ? ax.yaxisTransform : ax.xaxisTransform;
        rect->transformSet = true;
        ax.addPatch(std::move(rect));  // (the other direction's limits are kept as they were)
        *ax.figure.unitRectangleSteps = 180;  // p.get_path()._interpolation_steps = GRIDLINE_INTERPOLATION_STEPS
        ax.requestAutoscaleView(!horizontal, horizontal);
        return true;
    }

    // Axes.bar(x, height, width=0.8, bottom=None, *, align='center', **kwargs) and barh(y, width,
    // height=0.8, left=None, ...), which is bar(left, height, width, y, orientation='horizontal')
    bool bar(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const bool barh = call.method == "barh";
        // cbook.normalize_kwargs(kwargs, Patch)
        static const Aliases patchAliases = {{"aa", "antialiased"}, {"ec", "edgecolor"}, {"fc", "facecolor"},
                                             {"ls", "linestyle"}, {"lw", "linewidth"}};
        if (!givenOnce(call, patchAliases, where, error)) return false;
        const char* const names[4] = {barh ? "y" : "x", barh ? "width" : "height", barh ? "height" : "width",
                                      barh ? "left" : "bottom"};
        const Json* given[4];
        for (int i = 0; i < 4; ++i) given[i] = argument(call, std::size_t(i), names[i]);
        if (!given[0] || !given[1]) return *error = where + ": " + names[given[0] ? 1 : 0] + " is required", false;
        std::string orientation = barh ? "horizontal" : "vertical", align = "center", label;
        std::vector<std::string> patchLabels;
        bool log = false, labelList = false;
        const Json *xerr = nullptr, *yerr = nullptr, *facecolor = nullptr, *colorArg = nullptr, *edgecolor = nullptr,
                   *linewidth = nullptr, *hatch = nullptr, *ecolor = nullptr, *capsize = nullptr;
        std::vector<std::pair<std::string, const Json*>> rest;  // through the setters, in order (_internal_update)
        for (const auto& [rawName, value] : call.kwargs) {
            const std::string name = fullName(rawName, patchAliases);
            bool positional = false;
            for (const char* n : names) positional = positional || name == n;
            if (positional) continue;
            if (name == "align") {
                if (!value.isString() || (value.asString() != "center" && value.asString() != "edge"))
                    return *error = where + ": align must be 'center' or 'edge'", false;
                align = value.asString();
            } else if (name == "orientation" && !barh) {
                if (!value.isString() || (value.asString() != "vertical" && value.asString() != "horizontal"))
                    return *error = where + ": orientation must be 'vertical' or 'horizontal'", false;
                orientation = value.asString();
            } else if (name == "log") {
                if (!value.isBool()) return *error = where + ": log must be True or False", false;
                log = value.asBool();
            } else if (name == "label") {
                if (value.isNull()) {
                    label.clear();  // (the container's label None: no legend entry, as '')
                } else if (value.isString()) {
                    label = value.asString();
                } else if (const Json* seq = sequence(value)) {
                    labelList = true;
                    for (const Json& l : seq->items()) {
                        if (!l.isString()) return *error = where + ": labels must be strings", false;
                        patchLabels.push_back(l.asString());
                    }
                } else {
                    return *error = where + ": label must be a string or a list of strings", false;
                }
            } else if (name == "xerr") xerr = &value;
            else if (name == "yerr") yerr = &value;
            else if (name == "color") colorArg = &value;
            else if (name == "facecolor") facecolor = &value;
            else if (name == "edgecolor") edgecolor = &value;
            else if (name == "linewidth") linewidth = &value;
            else if (name == "hatch") hatch = &value;
            else if (name == "ecolor") ecolor = &value;
            else if (name == "capsize") capsize = &value;
            else if (name == "tick_label") return *error = where + ": tick_label is not supported yet", false;
            else if (name == "error_kw") {
                if (!value.isNull()) return *error = where + ": error_kw is not supported yet", false;
            } else rest.emplace_back(name, &value);
        }
        const bool vertical = orientation == "vertical";
        // the numbers: one or one per bar each, broadcast together
        std::vector<double> v[4];
        const double defaults[4] = {0, 0, 0.8, 0};
        for (int i = 0; i < 4; ++i) {
            double one = 0;
            if (!given[i] || given[i]->isNull()) v[i] = {i == 3 ? 0.0 : defaults[i]};
            else if (readNumber(*given[i], &one)) v[i] = {one};
            else if (!numbers(*given[i], &v[i])) return *error = where + ": " + names[i] + " must be numbers", false;
        }
        std::vector<double> widths, hatchesUnused;
        std::vector<std::optional<double>> lws{std::nullopt};
        if (linewidth && !linewidth->isNull()) {
            double one = 0;
            std::vector<double> l;
            if (readNumber(*linewidth, &one)) l = {one};
            else if (!numbers(*linewidth, &l)) return *error = where + ": linewidth must be numbers", false;
            lws.assign(l.begin(), l.end());
        }
        std::vector<std::optional<std::string>> hatches{std::nullopt};
        if (hatch && !hatch->isNull()) {
            hatches.clear();
            if (hatch->isString()) hatches.push_back(hatch->asString());
            else if (const Json* seq = sequence(*hatch)) {
                for (const Json& h : seq->items()) {
                    if (!h.isNull() && !h.isString()) return *error = where + ": hatch must be strings", false;
                    hatches.push_back(h.isNull() ? std::nullopt : std::optional<std::string>(h.asString()));
                }
            } else {
                return *error = where + ": hatch must be a string", false;
            }
        }
        std::size_t n = 1;
        const std::size_t sizes[6] = {v[0].size(), v[1].size(), v[2].size(), v[3].size(), lws.size(), hatches.size()};
        for (std::size_t sz : sizes) if (sz != 1) n = sz;
        for (std::size_t sz : sizes)
            if (sz != 1 && sz != n) return *error = where + ": the arguments cannot be broadcast to one shape", false;
        if (std::any_of(std::begin(sizes), std::end(sizes), [](std::size_t sz) { return sz == 0; })) n = 0;
        const auto at = [](const auto& list, std::size_t i) { return list.size() == 1 ? list[0] : list[i]; };
        if (labelList && patchLabels.size() != n)
            return *error = where + ": number of labels (" + std::to_string(patchLabels.size()) + ") does not match number of bars (" +
                            std::to_string(n) + ").",
                   false;
        // _parse_bar_color_args: facecolor, else color, else the next colour of the cycle; to_rgba_array
        std::vector<colors::Spec> faces, edges;
        const Json* face = facecolor && !facecolor->isNull() ? facecolor : colorArg && !colorArg->isNull() ? colorArg : nullptr;
        if (face) {
            if (!colorList(*face, &faces)) return *error = where + ": facecolor is not a colour", false;
        } else {
            faces = {ax.nextPatchColor()};
        }
        const auto rgbaRows = [&](std::vector<colors::Spec>& list) {
            std::vector<colors::Spec> rows;
            for (const auto& c : list) {
                if (colors::isNone(c) && list.size() == 1) return rows;  // to_rgba_array('none'): no rows
                const auto rgba = colors::toRgba(c, std::nullopt, rc_, nullptr);
                if (!rgba) return std::vector<colors::Spec>{c};
                rows.push_back(std::vector<double>(rgba->begin(), rgba->end()));
            }
            return rows;
        };
        faces = rgbaRows(faces);
        bool edgeGiven = false;
        if (edgecolor && !edgecolor->isNull()) {
            if (!colorList(*edgecolor, &edges)) return *error = where + ": edgecolor is not a colour", false;
            edges = rgbaRows(edges);
            edgeGiven = true;
        }
        if (log) {
            ax.setScale(!vertical, "log");  // set_yscale('log', nonpositive='clip') (set_xscale for barh)
        }
        // Axes._convert_dx(dx, x0, xconv, convert): a width becomes (x0 + dx) - x0, with x0 the first finite
        // position (cbook._safe_first_finite: else the first), so it carries that addition's rounding;
        // the widths go with bar's x (barh's left), the heights and yerr with its y, xerr with its x
        std::vector<double>& xs = v[barh ? 3 : 0];
        std::vector<double>& ys = v[barh ? 0 : 3];
        const auto convertDx = [](const std::vector<double>& positions, std::vector<double>* dx) {
            if (positions.empty()) return;
            double x0 = positions[0];
            for (double p : positions)
                if (std::isfinite(p)) {
                    x0 = p;
                    break;
                }
            for (double& d : *dx) d = (x0 + d) - x0;
        };
        convertDx(xs, &v[barh ? 1 : 2]);
        convertDx(ys, &v[barh ? 2 : 1]);
        // xerr and yerr the same way when a number or a list of numbers (else x0 + ddx raises: as given)
        Json convertedErr[2];
        for (int k = 0; k < 2; ++k) {
            const Json*& err = k == 0 ? xerr : yerr;
            if (!err || err->isNull()) continue;
            std::vector<double> e;
            double one = 0;
            const bool scalar = readNumber(*err, &one);
            if (scalar) e = {one};
            else if (!numbers(*err, &e)) continue;
            const std::vector<double> before = e;
            convertDx(k == 0 ? xs : ys, &e);
            if (std::equal(e.begin(), e.end(), before.begin(), [](double a, double b) {
                    return a == b || (std::isnan(a) && std::isnan(b));
                }))
                continue;
            std::vector<Json> items;
            for (double d : e) items.push_back(Json::real(d));
            convertedErr[k] = scalar ? Json::real(e[0]) : Json::array(items);
            err = &convertedErr[k];
        }
        // align: the bars' left (vertical) or bottom (horizontal) edges
        std::vector<double> left(n), bottom(n), width(n), height(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double x = at(v[0], i), h = at(v[1], i), w = at(v[2], i), y = at(v[3], i);
            // (barh: y, width, height, left are bar's y, height-as-width... read as bar(left, height, width, y))
            const double bx = barh ? y : x, bh = barh ? w : h, bw = barh ? h : w, by = barh ? x : y;
            width[i] = bw, height[i] = bh;
            if (align == "center") {
                if (vertical) left[i] = bx - bw / 2, bottom[i] = by;
                else bottom[i] = by - bh / 2, left[i] = bx;
            } else {
                left[i] = bx, bottom[i] = by;
            }
        }
        ErrorbarContainer container;
        container.isBars = true;
        container.label = labelList ? "_nolegend_" : label;
        // datavalues: bar's height (barh's width) as _convert_dx made it, int64 when the values and the
        // base are integers ((y0 + h) - y of Python and NumPy ints)
        container.horizontal = !vertical;
        for (std::size_t i = 0; i < n; ++i) container.datavalues.push_back(at(v[1], i));
        container.datavaluesInt = given[1] && integers(*given[1]) && (!given[3] || given[3]->isNull() || integers(*given[3]));
        for (std::size_t i = 0; i < n; ++i) {
            auto r = std::make_unique<Rectangle>(rc_, left[i], bottom[i], width[i], height[i]);
            // Rectangle(xy, width, height, facecolor=, edgecolor=, linewidth=, label=, hatch=)
            if (edgeGiven) r->setEdgecolor(edges.empty() ? colors::Spec(std::string("none")) : edges[i % edges.size()]);
            r->setFacecolor(faces.empty() ? colors::Spec(std::string("none")) : faces[i % faces.size()]);
            r->setLinewidth(lws[i % lws.size()]);
            r->setHatch(hatches[i % hatches.size()], true);  // (itertools.cycle(np.atleast_1d(hatch)): np.str_)
            r->label = labelList ? patchLabels[i] : "_nolegend_";
            for (const auto& [name, value] : rest)
                if (!patchProperty(*r, name, *value, where, error)) return false;
            if (vertical) r->stickyY.push_back(bottom[i]);
            else r->stickyX.push_back(left[i]);
            container.bars.push_back(r.get());
            ax.addPatch(std::move(r));
            *ax.figure.unitRectangleSteps = 100;  // r.get_path()._interpolation_steps = 100
        }
        if ((xerr && !xerr->isNull()) || (yerr && !yerr->isNull())) {
            container.errorbar = ax.containers.size();  // (where errorbar puts its container)
            // errorbar(ex, ey, yerr=, xerr=, fmt='none', ecolor=, capsize=, zorder=, label='_nolegend_')
            model::Call e;
            e.on = call.on;
            e.method = "errorbar";
            std::vector<Json> ex, ey;
            for (std::size_t i = 0; i < n; ++i) {
                ex.push_back(Json::real(vertical ? left[i] + 0.5 * width[i] : left[i] + width[i]));
                ey.push_back(Json::real(vertical ? bottom[i] + height[i] : bottom[i] + 0.5 * height[i]));
            }
            e.args = {Json::array(ex), Json::array(ey)};
            if (yerr && !yerr->isNull()) e.kwargs.emplace_back("yerr", *yerr);
            if (xerr && !xerr->isNull()) e.kwargs.emplace_back("xerr", *xerr);
            e.kwargs.emplace_back("fmt", Json::string("none"));
            e.kwargs.emplace_back("ecolor", ecolor ? *ecolor : Json::string("k"));
            e.kwargs.emplace_back("capsize", capsize ? *capsize : rc_.value("errorbar.capsize"));
            for (const auto& [name, value] : rest)
                if (name == "zorder" && value->isNumber()) e.kwargs.emplace_back("zorder", Json::real(value->asNumber() + 0.01));
            e.kwargs.emplace_back("label", Json::string("_nolegend_"));
            if (!errorbar(ax, e, error)) return false;
        }
        ax.requestAutoscaleView(true, true);
        ax.containers.push_back(container);
        if (!call.as.empty()) barContainers_[call.as[0]] = {&ax, ax.containers.size() - 1};
        return true;
    }

    // LineCollection.get_segments: each path's vertices as iter_segments gives them (non-finite ones removed)
    static std::vector<std::vector<std::array<double, 2>>> segmentsOf(const Collection& c) {
        std::vector<std::vector<std::array<double, 2>>> out;
        for (const Path& p : c.paths) {
            mpl::PathIterator it(p.vertices, p.codes, false, p.simplifyThreshold);
            agg::trans_affine identity;
            std::vector<double> v;
            std::vector<unsigned char> codes;
            cleanup_path(it, identity, true, false, agg::rect_d(0.0, 0.0, 0.0, 0.0), SNAP_FALSE, 1.0, false, true,
                         SketchParams{0.0, 0.0, 0.0}, v, codes);
            std::vector<std::array<double, 2>> s;
            for (std::size_t k = 0; k < codes.size() && codes[k] != agg::path_cmd_stop; ++k) s.push_back({v[2 * k], v[2 * k + 1]});
            out.push_back(s);
        }
        return out;
    }

    // Axes.bar_label(container, labels=None, *, fmt='%g', label_type='edge', padding=0, **kwargs): one
    // annotation per bar, at the end of the bar (or of its error bar) or in its middle, offset by the
    // padding in points away from the bar's base; the text from `labels` or fmt of the bar's value
    bool barLabel(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".bar_label";
        for (const char* key : {"horizontalalignment", "ha", "verticalalignment", "va"})
            if (keyword(call, key)) return *error = where + ": Passing '" + key + "' to bar_label() is not supported.", false;
        const Json* ref = argument(call, 0, "container");
        const bool isRef = ref && ref->isObject() && ref->members()[0].first == "$ref";
        const auto found = isRef ? barContainers_.find(ref->members()[0].second.asString()) : barContainers_.end();
        if (found == barContainers_.end())
            return *error = where + ": the container must be a bar call's result (a {\"$ref\": name})", false;
        if (found->second.first != &ax) return *error = where + ": labels for another Axes' bars are not supported yet", false;
        // the view limits as they are now (a pending autoscale runs first)
        const auto yv = ax.yaxis->viewInterval();
        const bool yInverted = yv[0] > yv[1];
        const auto xv = ax.xaxis->viewInterval();
        const bool xInverted = xv[0] > xv[1];
        std::string labelType = "edge";
        if (const Json* l = keyword(call, "label_type")) {
            if (!l->isString() || (l->asString() != "edge" && l->asString() != "center"))
                return *error = where + ": " + (l->isString() ? "'" + l->asString() + "'" : std::string("that")) +
                                " is not a valid value for label_type; supported values are 'edge', 'center'",
                       false;
            labelType = l->asString();
        }
        const ErrorbarContainer container = ax.containers[found->second.second];
        std::vector<std::vector<std::array<double, 2>>> errs;
        if (container.errorbar) {
            const ErrorbarContainer& e = ax.containers[*container.errorbar];
            if (!e.barlinecols.empty()) errs = segmentsOf(*e.barlinecols[0]);
        }
        std::vector<std::string> labels;
        if (const Json* l = argument(call, 1, "labels"); l && !l->isNull()) {
            const Json* seq = sequence(*l);
            if (!seq) return *error = where + ": labels must be a list of strings", false;
            for (const Json& s : seq->items()) {
                if (!s.isString()) return *error = where + ": labels must be strings (others are not supported yet)", false;
                labels.push_back(s.asString());
            }
        }
        const std::size_t n = container.bars.size();
        std::vector<double> padding;
        double one = 0;
        if (const Json* p = keyword(call, "padding"); p && !p->isNull()) {
            if (readNumber(*p, &one)) {
                padding.assign(n, one);
            } else if (!numbers(*p, &padding)) {
                return *error = where + ": padding must be a number or a list of numbers", false;
            } else if (padding.size() != n) {
                return *error = where + ": padding must be of length " + std::to_string(n) + " when passed as a sequence", false;
            }
        } else {
            padding.assign(n, 0.0);
        }
        std::string fmt = "%g";
        if (const Json* f = keyword(call, "fmt")) {
            if (!f->isString()) return *error = where + ": fmt must be a str (a callable is not supported)", false;
            fmt = f->asString();
        }
        std::vector<std::pair<std::string, const Json*>> rest;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "container" || name == "labels" || name == "fmt" || name == "label_type" || name == "padding") continue;
            static const char* const annotateOwn[] = {"text", "xy", "xytext", "xycoords", "textcoords", "arrowprops", "annotation_clip"};
            if (std::find(std::begin(annotateOwn), std::end(annotateOwn), name) != std::end(annotateOwn))
                return *error = where + ": " + name + " is not supported yet", false;
            rest.emplace_back(name, &value);
        }
        if (labels.size() > n) return *error = where + ": 'NoneType' object has no attribute 'get_bbox'", false;
        const auto sign = [](double x) { return x >= 0 ? 1.0 : -1.0; };
        for (std::size_t i = 0; i < n; ++i) {
            Rectangle* bar = container.bars[i];
            // get_bbox().get_points(): (x0, y0), (x0 + width, y0 + height)
            const double x0 = bar->x0, y0 = bar->y0, x1 = bar->x0 + bar->width, y1 = bar->y0 + bar->height;
            const double xc = (x0 + x1) / 2, yc = (y0 + y1) / 2;
            const double dat = container.datavalues[i];
            const auto pmax = [](double a, double b) { return b > a ? b : a; };  // max(a, b)
            const auto pmin = [](double a, double b) { return b < a ? b : a; };  // min(a, b)
            const double extrema = !container.horizontal ? (dat >= 0 ? pmax(y0, y1) : pmin(y0, y1)) : (dat >= 0 ? pmax(x0, x1) : pmin(x0, x1));
            double endpt = extrema;
            if (i < errs.size() && !errs[i].empty()) {
                // err[:, 1].max() (or .min(); [:, 0] for horizontal bars): NaN wins
                const int d = container.horizontal ? 0 : 1;
                endpt = errs[i][0][std::size_t(d)];
                for (const auto& q : errs[i]) {
                    const double e = q[std::size_t(d)];
                    if (std::isnan(endpt)) break;
                    if (std::isnan(e) || (dat >= 0 ? e > endpt : e < endpt)) endpt = e;
                }
            }
            AnnotationCoords xycoords, textcoords;
            xycoords.names = {"data"};
            textcoords.names = {"offset points"};
            std::array<double, 2> xy{};
            if (labelType == "center") {
                xy = {0.5, 0.5};
                // Bbox.intersection(bar.get_window_extent(r), bar.get_clip_box()) or Bbox.null()
                xycoords.names.clear();
                xycoords.box = [bar](Renderer&) {
                    const auto a = bar->windowExtent().points();
                    const auto b = bar->clipBox->points();
                    const auto nanMax = [](double p, double q) { return std::isnan(p) || std::isnan(q) ? std::nan("") : std::max(p, q); };
                    const auto nanMin = [](double p, double q) { return std::isnan(p) || std::isnan(q) ? std::nan("") : std::min(p, q); };
                    const double ix0 = nanMax(nanMin(a[0], a[2]), nanMin(b[0], b[2])), ix1 = nanMin(nanMax(a[0], a[2]), nanMax(b[0], b[2]));
                    const double iy0 = nanMax(nanMin(a[1], a[3]), nanMin(b[1], b[3])), iy1 = nanMin(nanMax(a[1], a[3]), nanMax(b[1], b[3]));
                    if (ix0 <= ix1 && iy0 <= iy1) return transforms::Bbox({ix0, iy0, ix1, iy1});
                    const double inf = std::numeric_limits<double>::infinity();
                    return transforms::Bbox({inf, inf, -inf, -inf});  // Bbox.null()
                };
            } else {
                xy = !container.horizontal ? std::array<double, 2>{xc, endpt} : std::array<double, 2>{endpt, yc};
            }
            const double pad = padding[i];
            const std::array<double, 2> xytext = !container.horizontal ? std::array<double, 2>{0, (yInverted ? -1.0 : 1.0) * sign(dat) * pad}
                                                                        : std::array<double, 2>{(xInverted ? -1.0 : 1.0) * sign(dat) * pad, 0};
            std::string ha = "center", va = "center";
            if (labelType == "edge") {
                if (!container.horizontal) {
                    va = yInverted ? (dat > 0 ? "top" : "bottom") : (dat < 0 ? "top" : "bottom");  // (also for NaN)
                } else {
                    ha = xInverted ? (dat > 0 ? "right" : "left") : (dat < 0 ? "right" : "left");
                }
            }
            std::string text;
            if (std::isnan(dat)) {
                text = "";
            } else if (i < labels.size()) {
                text = labels[i];
            } else {
                const support::Formatted f = autoFormatStr(fmt, dat, container.datavaluesInt);
                if (f.kind == support::Formatted::Unsupported) return *error = where + ": this fmt is not supported yet (" + f.text + ")", false;
                if (f.kind == support::Formatted::Raised) return *error = where + ": " + f.text, false;
                text = f.text;
            }
            auto a = std::make_unique<Annotation>(rc_, ax, text, xy, xytext, xycoords, textcoords, std::nullopt);
            const Json haJson = Json::string(ha), vaJson = Json::string(va);
            std::vector<std::pair<std::string, const Json*>> given = {{"ha", &haJson}, {"va", &vaJson}};
            given.insert(given.end(), rest.begin(), rest.end());
            if (!textProperties(*a, given, TextKeywords::Construct, where, error)) return false;
            a->figure = &ax.figure;
            ax.children.push_back(std::move(a));
        }
        return true;
    }

    // A colour as a figure file writes it (for the calls made here)
    static Json colorJson(const colors::Spec& c) {
        if (const auto* s = std::get_if<std::string>(&c)) return Json::string(*s);
        std::vector<Json> items;
        for (double v : std::get<std::vector<double>>(c)) items.push_back(Json::real(v));
        return Json::array(items);
    }

    static Json numbersJson(const std::vector<double>& values) {
        std::vector<Json> items;
        for (double v : values) items.push_back(Json::real(v));
        return Json::array(items);
    }

    // Whether NumPy makes an int64 array of a number or a list of them: every value written as an
    // integer (as a Python int), and at least one
    static bool integers(const Json& v) {
        if (v.isNumber()) return v.isInteger() && fitsIn64Bits(v.numberText());
        const Json* seq = sequence(v);
        if (!seq || seq->items().empty()) return false;
        for (const Json& item : seq->items())
            if (!item.isNumber() || !item.isInteger() || !fitsIn64Bits(item.numberText())) return false;
        return true;
    }

    // cbook._reshape_2D for lists: a list of numbers is one data set, otherwise each item is one (a
    // number being a set of one value). `integer`: which data sets NumPy holds as int64.
    bool reshape2D(const Json& v, std::vector<std::vector<double>>* out, std::vector<bool>* integer = nullptr) {
        out->clear();
        if (integer) integer->clear();
        const Json* seq = sequence(v);
        if (!seq) return false;
        if (seq->items().empty()) {
            if (integer) integer->push_back(false);
            return out->emplace_back(), true;
        }
        bool flat = true;
        for (const Json& item : seq->items()) flat = flat && !sequence(item);
        std::vector<double> one;
        if (flat) {
            if (!numbers(v, &one)) return false;
            out->push_back(one);
            if (integer) integer->push_back(integers(v));
            return true;
        }
        for (const Json& item : seq->items()) {
            double n = 0;
            if (readNumber(item, &n)) out->push_back({n});
            else if (numbers(item, &one)) out->push_back(one);
            else return false;
            if (integer) integer->push_back(integers(item));
        }
        return true;
    }

    // Axes.hist(x, bins=None, range=None, density=False, weights=None, cumulative=False, bottom=None,
    // histtype='bar', align='mid', orientation='vertical', rwidth=None, log=False, color=None,
    // label=None, stacked=False, **kwargs): np.histogram of each data set, drawn by bar or barh, or for
    // histtype 'step' and 'stepfilled' by fill.
    bool hist(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".hist";
        static const Aliases patchAliases = {{"aa", "antialiased"}, {"ec", "edgecolor"}, {"fc", "facecolor"},
                                             {"ls", "linestyle"}, {"lw", "linewidth"}};
        if (!givenOnce(call, patchAliases, where, error)) return false;
        static const char* const names[15] = {"x",           "bins",   "range", "density", "weights",
                                              "cumulative",  "bottom", "histtype", "align", "orientation",
                                              "rwidth",      "log",    "color", "label",   "stacked"};
        const Json* raw[15];
        const Json* given[15];  // (None as not given)
        for (int i = 0; i < 15; ++i) {
            raw[i] = argument(call, std::size_t(i), names[i]);
            given[i] = raw[i] && !raw[i]->isNull() ? raw[i] : nullptr;
        }
        if (!given[0]) return *error = where + ": x is required", false;
        const auto truthy = [&](int i, bool* out) {
            *out = false;
            if (!given[i]) return true;
            if (given[i]->isBool()) return *out = given[i]->asBool(), true;
            double n = 0;
            if (readNumber(*given[i], &n)) return *out = n != 0, true;
            return *error = where + ": " + names[i] + " must be True or False", false;
        };
        // _api.check_in_list
        const auto choice = [&](int i, std::initializer_list<const char*> valid, std::string* out) {
            if (!raw[i]) return *out = *valid.begin(), true;
            for (const char* c : valid)
                if (raw[i]->isString() && raw[i]->asString() == c) return *out = c, true;
            std::string list;
            for (const char* c : valid) list += std::string(list.empty() ? "" : ", ") + "'" + c + "'";
            return *error = where + ": " + (raw[i]->isString() ? "'" + raw[i]->asString() + "'" : std::string("that")) +
                            " is not a valid value for " + names[i] + "; supported values are " + list,
                   false;
        };
        bool density = false, cumulative = false, log = false, stacked = false;
        if (!truthy(3, &density) || !truthy(5, &cumulative) || !truthy(11, &log) || !truthy(14, &stacked)) return false;
        std::string histtype, align, orientation;
        if (!choice(7, {"bar", "barstacked", "step", "stepfilled"}, &histtype) || !choice(8, {"mid", "left", "right"}, &align) ||
            !choice(9, {"vertical", "horizontal"}, &orientation))
            return false;
        if (histtype == "barstacked") stacked = true;
        // the data sets (np.isscalar(x): [x])
        std::vector<std::vector<double>> x, w;
        std::vector<bool> xIntegers;  // (the data sets NumPy holds as int64: the bin rules by name see it)
        double one = 0;
        if (readNumber(*given[0], &one)) x = {{one}}, xIntegers = {integers(*given[0])};
        else if (!reshape2D(*given[0], &x, &xIntegers)) return *error = where + ": x must be numbers or lists of numbers", false;
        const std::size_t nx = x.size();
        // bins: rcParams["hist.bins"] when None; a count or the edges
        support::Bins bins;
        const Json& binsValue = given[1] ? *given[1] : rc_.value("hist.bins");
        if (binsValue.isString()) {
            bins.name = binsValue.asString();
        } else if (binsValue.isBool()) {
            bins.count = binsValue.asBool() ? 1 : 0;  // (operator.index(True) is 1)
        } else if (binsValue.isNumber()) {
            if (!binsValue.isInteger()) return *error = where + ": `bins` must be an integer, a string, or an array", false;
            if (binsValue.asNumber() > 100000) return *error = where + ": more than 100000 bins are not supported", false;
            bins.count = static_cast<long long>(binsValue.asNumber());
        } else if (!numbers(binsValue, &bins.edges)) {
            return *error = where + ": `bins` must be an integer, a string, or an array", false;
        }
        std::optional<std::pair<double, double>> binRange;
        if (given[2]) {
            std::vector<double> r;
            if (!numbers(*given[2], &r) || r.size() != 2) return *error = where + ": range must be two numbers", false;
            binRange = std::make_pair(r[0], r[1]);
        }
        if (given[4]) {
            if (!reshape2D(*given[4], &w)) return *error = where + ": weights must be lists of numbers", false;
            if (w.size() != nx) return *error = where + ": weights should have the same shape as x", false;
            for (std::size_t i = 0; i < nx; ++i)
                if (w[i].size() != x[i].size()) return *error = where + ": weights should have the same shape as x", false;
        }
        bool inputEmpty = true;
        for (const auto& xi : x) inputEmpty = inputEmpty && xi.empty();
        // the colours: the lines' cycle, or one per data set (to_rgba_array)
        std::vector<colors::Spec> setColors;
        if (!given[12]) {
            for (std::size_t i = 0; i < nx; ++i) setColors.push_back(ax.nextLineColor());
        } else {
            colors::Spec c;
            std::vector<colors::Spec> list;
            if (color(*given[12], &c)) list.assign(nx, c);  // is_color_like: [color] * nx
            else if (!colorList(*given[12], &list)) return *error = where + ": color is not a colour or a list of colours", false;
            for (const auto& item : list) {
                const auto rgba = colors::toRgba(item, std::nullopt, rc_, nullptr);
                if (!rgba) return *error = where + ": color is not a colour or a list of colours", false;
                setColors.push_back(std::vector<double>(rgba->begin(), rgba->end()));
            }
            if (setColors.size() != nx)
                return *error = where + ": The 'color' keyword argument must have one color per dataset, but " + std::to_string(nx) +
                                " datasets and " + std::to_string(setColors.size()) + " colors were provided",
                       false;
        }
        // the labels (np.atleast_1d(np.asarray(label, str)))
        std::vector<std::string> labels;
        if (given[13]) {
            if (given[13]->isString()) {
                labels.push_back(given[13]->asString());
            } else if (const Json* seq = sequence(*given[13])) {
                for (const Json& l : seq->items()) {
                    if (!l.isString()) return *error = where + ": labels must be strings", false;
                    labels.push_back(l.asString());
                }
            } else {
                return *error = where + ": label must be a string or a list of strings", false;
            }
        }
        // the other keywords (_style_helpers.style_generator): the colours, hatches, line widths and line
        // styles cycle over the data sets; the rest go to every bar as they are
        struct Style {
            std::string name;
            std::vector<std::optional<colors::Spec>> colours;  // facecolor, edgecolor (None: default)
            std::vector<std::optional<std::string>> hatches;
            std::vector<std::optional<double>> linewidths;
            std::vector<std::pair<double, std::optional<std::vector<double>>>> dashes;
            std::size_t size() const { return std::max({colours.size(), hatches.size(), linewidths.size(), dashes.size()}); }
        };
        std::vector<std::pair<std::string, const Json*>> rest;
        std::vector<Style> styles;
        for (const auto& [rawName, value] : call.kwargs) {
            const std::string name = fullName(rawName, patchAliases);
            if (std::find(std::begin(names), std::end(names), name) != std::end(names)) continue;
            if (name != "facecolor" && name != "edgecolor" && name != "hatch" && name != "linewidth" && name != "linestyle") {
                rest.emplace_back(name, &value);
                continue;
            }
            Style s;
            s.name = name;
            const Json* seq = value.isString() ? nullptr : sequence(value);
            if (seq && seq->items().empty()) return *error = where + ": " + name + " must not be an empty sequence", false;
            if (name == "facecolor" || name == "edgecolor") {
                std::string lower = value.isString() ? value.asString() : "";
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                if (value.isNull()) {
                    s.colours = {std::nullopt};
                } else if (lower == "none") {
                    s.colours = {colors::Spec(value.asString())};
                } else {
                    std::vector<colors::Spec> list;
                    if (!colorList(value, &list)) return *error = where + ": " + name + " is not a colour", false;
                    for (const auto& item : list) {
                        const auto rgba = colors::toRgba(item, std::nullopt, rc_, nullptr);
                        if (!rgba) return *error = where + ": " + name + " is not a colour", false;
                        if (colors::isNone(item) && list.size() == 1) break;  // to_rgba_array('none'): no rows
                        s.colours.push_back(std::vector<double>(rgba->begin(), rgba->end()));
                    }
                    if (s.colours.empty()) return *error = where + ": " + name + " must not be empty", false;
                }
            } else if (name == "hatch") {
                if (value.isString() || value.isNull()) s.hatches = {value.isNull() ? std::nullopt : std::optional<std::string>(value.asString())};
                else if (!seq) return *error = where + ": hatch must be a string", false;
                else
                    for (const Json& h : seq->items()) {
                        if (!h.isNull() && !h.isString()) return *error = where + ": hatch must be strings", false;
                        s.hatches.push_back(h.isNull() ? std::nullopt : std::optional<std::string>(h.asString()));
                    }
            } else if (name == "linewidth") {
                std::vector<double> l;
                if (value.isNull()) s.linewidths = {std::nullopt};
                else if (readNumber(value, &one)) s.linewidths = {one};
                else if (numbers(value, &l)) s.linewidths.assign(l.begin(), l.end());
                else return *error = where + ": linewidth must be numbers", false;
            } else {
                // mlines._get_dash_patterns: one style or one per data set (named styles only)
                std::vector<std::string> list;
                if (value.isString()) list = {value.asString()};
                else if (seq)
                    for (const Json& l : seq->items()) {
                        if (!l.isString()) return *error = where + ": linestyle given as dashes is not supported yet", false;
                        list.push_back(l.asString());
                    }
                else return *error = where + ": linestyle given as dashes is not supported yet", false;
                for (std::string style : list) {
                    if (style == "-") style = "solid";
                    else if (style == "--") style = "dashed";
                    else if (style == "-.") style = "dashdot";
                    else if (style == ":") style = "dotted";
                    if (style == "solid" || style == "None" || style == "none" || style == "" || style == " ")
                        s.dashes.push_back({0.0, std::nullopt});
                    else if (style == "dashed" || style == "dashdot" || style == "dotted")
                        s.dashes.push_back(dashPattern(style, rc_));
                    else
                        return *error = where + ": Do not know how to convert that to dashes", false;
                }
            }
            styles.push_back(std::move(s));
        }

        // the histograms: shared edges for several data sets (np.histogram_bin_edges of them all)
        if (!binRange) {
            double xmin = std::numeric_limits<double>::infinity(), xmax = -xmin;
            for (const auto& xi : x) {
                if (xi.empty()) continue;
                double lo = std::numeric_limits<double>::quiet_NaN(), hi = lo;  // np.nanmin / np.nanmax
                for (double v : xi)
                    if (!std::isnan(v)) {
                        if (std::isnan(lo) || v < lo) lo = v;
                        if (std::isnan(hi) || v > hi) hi = v;
                    }
                if (lo < xmin) xmin = lo;  // (min(xmin, nan) keeps xmin)
                if (hi > xmax) xmax = hi;
            }
            if (xmin <= xmax) binRange = std::make_pair(xmin, xmax);
        }
        std::optional<std::pair<double, double>> histRange;
        std::vector<double> edges;
        std::string why;
        if (!inputEmpty && nx > 1) {
            std::vector<double> all, allWeights;
            for (std::size_t i = 0; i < nx; ++i) {
                all.insert(all.end(), x[i].begin(), x[i].end());
                if (!w.empty()) allWeights.insert(allWeights.end(), w[i].begin(), w[i].end());
            }
            // (np.concatenate: int64 only if every data set is)
            const bool allIntegers = std::all_of(xIntegers.begin(), xIntegers.end(), [](bool b) { return b; });
            if (!support::histogramBinEdges(all, bins, binRange, w.empty() ? nullptr : &allWeights, &edges, &why, allIntegers))
                return *error = where + ": " + why, false;
            bins = support::Bins{std::nullopt, edges, {}};
        } else {
            histRange = binRange;
        }
        std::vector<std::vector<double>> tops(nx);
        for (std::size_t i = 0; i < nx; ++i) {
            if (!support::histogram(x[i], bins, histRange, density && !stacked, w.empty() ? nullptr : &w[i], &tops[i], &edges, &why,
                                    xIntegers[i]))
                return *error = where + ": " + why, false;
            bins = support::Bins{std::nullopt, edges, {}};
        }
        const std::size_t nb = edges.empty() ? 0 : edges.size() - 1;
        std::vector<double> totwidth(nb);  // np.diff(bins)
        for (std::size_t b = 0; b < nb; ++b) totwidth[b] = edges[b + 1] - edges[b];
        if (stacked) {
            for (std::size_t i = 1; i < nx; ++i)
                for (std::size_t b = 0; b < nb; ++b) tops[i][b] = tops[i - 1][b] + tops[i][b];
            if (density) {
                const double total = support::npSum(tops.back());
                for (auto& top : tops)
                    for (std::size_t b = 0; b < nb; ++b) top[b] = (top[b] / totwidth[b]) / total;
            }
        }
        if (cumulative) {
            double c = 0;
            const bool reverse = given[5]->isNumber() && readNumber(*given[5], &c) && c < 0;
            for (auto& top : tops) {
                if (density)
                    for (std::size_t b = 0; b < nb; ++b) top[b] = top[b] * totwidth[b];
                if (reverse)
                    for (std::size_t b = nb - 1; b-- > 0;) top[b] = top[b + 1] + top[b];
                else
                    for (std::size_t b = 1; b < nb; ++b) top[b] = top[b - 1] + top[b];
            }
        }

        const bool horizontal = orientation == "horizontal";
        std::vector<std::vector<Patch*>> patches;  // one list per data set
        if (histtype == "step" || histtype == "stepfilled") {
            // the perimeter of each polygon: the steps along the tops, then back along the bottom
            const std::size_t L = edges.size(), n = 4 * L - 3;
            std::vector<double> px(n, 0.0), py(n, 0.0);
            for (std::size_t k = 0; k < L; ++k) px[2 * k] = edges[k];
            for (std::size_t k = 0; k + 1 < L; ++k) px[2 * k + 1] = edges[k];
            for (std::size_t j = 0; j + 2 * L - 1 < n; ++j) px[2 * L - 1 + j] = px[2 * L - 2 - j];  // x[1:2L-1][::-1]
            // bottom: None is 0; a number, or one per bin
            std::vector<double> bottomValues{0.0};
            if (given[6]) {
                if (readNumber(*given[6], &one)) bottomValues = {one};
                else if (!numbers(*given[6], &bottomValues)) return *error = where + ": bottom must be numbers", false;
                if (bottomValues.size() != 1 && bottomValues.size() != nb)
                    return *error = where + ": bottom cannot be broadcast to the bins", false;
            }
            const auto bottomAt = [&](std::size_t b) { return bottomValues[bottomValues.size() == 1 ? 0 : b]; };
            for (std::size_t b = 0; b < nb; ++b) py[2 * b + 1] = py[2 * b + 2] = bottomAt(b);
            for (std::size_t j = 0; j + 2 * L - 1 < n; ++j) py[2 * L - 1 + j] = py[2 * L - 2 - j];
            if (log) ax.setScale(horizontal, "log");  // set_xscale / set_yscale('log', nonpositive='clip')
            if (align == "left" || align == "right") {
                const double shift = 0.5 * (edges[1] - edges[0]);
                for (double& v : px) v = align == "left" ? v - shift : v + shift;
            }
            const bool filled = histtype == "stepfilled";
            std::vector<std::vector<double>> xvals, yvals;
            for (std::size_t i = 0; i < nx; ++i) {
                if (stacked)  // the top of the previous polygon becomes the bottom
                    for (std::size_t j = 0; j + 2 * L - 1 < n; ++j) py[2 * L - 1 + j] = py[2 * L - 2 - j];
                for (std::size_t b = 0; b < nb; ++b) py[2 * b + 1] = py[2 * b + 2] = tops[i][b] + bottomAt(b);
                py[0] = py[n - 1];  // (closes the polygon; the end vertex is dropped below)
                xvals.push_back(horizontal ? py : px);
                yvals.push_back(horizontal ? px : py);
            }
            // stepfilled is closed, step is not; the polygons are made in reverse order (lower ones of a
            // stack drawn over higher ones), each fill taking the next entry of the patches' cycle
            const std::size_t split = filled ? n - 1 : 2 * L;
            for (std::size_t i = nx; i-- > 0;) {
                model::Call c;
                c.on = call.on;
                c.method = "fill";
                c.args = {numbersJson({xvals[i].begin(), xvals[i].begin() + long(split)}),
                          numbersJson({yvals[i].begin(), yvals[i].begin() + long(split)})};
                c.kwargs.emplace_back("closed", filled ? Json::boolean(true) : Json());
                c.kwargs.emplace_back("facecolor", colorJson(setColors[i]));
                c.kwargs.emplace_back("edgecolor", filled ? Json() : colorJson(setColors[i]));
                c.kwargs.emplace_back("fill", filled ? Json::boolean(true) : Json());
                c.kwargs.emplace_back("zorder", filled ? Json() : Json::integer(2));  // (Line2D.zorder)
                std::vector<Polygon*> made;
                if (!fill(ax, c, error, &made)) return false;
                for (Polygon* p : made) (horizontal ? p->stickyX : p->stickyY).push_back(0);
                patches.emplace_back(made.begin(), made.end());
            }
            std::reverse(patches.begin(), patches.end());
            if (histtype == "step" && std::none_of(styles.begin(), styles.end(), [](const Style& s) { return s.name == "edgecolor"; })) {
                // kwargs.setdefault('edgecolor', colors): the data sets' colours as rows
                Style s;
                s.name = "edgecolor";
                for (const auto& sc : setColors) {
                    const auto rgba = colors::toRgba(sc, std::nullopt, rc_, nullptr);
                    s.colours.push_back(std::vector<double>(rgba->begin(), rgba->end()));
                }
                styles.push_back(std::move(s));
            }
        } else {
        // the bars: one bar() call per data set, side by side or stacked
        double dr = 1.0;
        if (given[10]) {
            if (!readNumber(*given[10], &dr)) return *error = where + ": rwidth must be a number", false;
            if (!std::isnan(dr)) dr = std::min(std::max(dr, 0.0), 1.0);  // np.clip(rwidth, 0, 1)
        } else if (nx > 1 && (!stacked || rc_.boolean("_internal.classic_mode"))) {
            dr = 0.8;
        }
        std::vector<double> width(nb), dw(nb, 0.0), boffset(nb, 0.0);
        if (histtype == "bar" && !stacked) {
            for (std::size_t b = 0; b < nb; ++b) {
                width[b] = dr * totwidth[b] / double(nx);
                boffset[b] = -0.5 * dr * totwidth[b] * (1 - 1 / double(nx));
            }
            dw = width;
        } else {
            for (std::size_t b = 0; b < nb; ++b) width[b] = dr * totwidth[b];
        }
        if (align == "mid")
            for (std::size_t b = 0; b < nb; ++b) boffset[b] += 0.5 * totwidth[b];
        else if (align == "right")
            for (std::size_t b = 0; b < nb; ++b) boffset[b] += totwidth[b];
        // bottom: None (zeros), a number or one per bin
        std::optional<Json> bottom;
        std::vector<double> bottomValues;
        if (given[6]) {
            if (readNumber(*given[6], &one)) bottomValues = {one};
            else if (!numbers(*given[6], &bottomValues)) return *error = where + ": bottom must be numbers", false;
            bottom = *given[6];
        }
        for (std::size_t i = 0; i < nx; ++i) {
            if (!bottom) {
                bottomValues.assign(nb, 0.0);
                bottom = numbersJson(bottomValues);
            }
            std::vector<double> height = tops[i], positions(nb);
            if (stacked) {
                if (bottomValues.size() != 1 && bottomValues.size() != nb)
                    return *error = where + ": bottom cannot be broadcast to the bins", false;
                for (std::size_t b = 0; b < nb; ++b) height[b] = tops[i][b] - bottomValues[bottomValues.size() == 1 ? 0 : b];
            }
            for (std::size_t b = 0; b < nb; ++b) positions[b] = edges[b] + boffset[b];
            model::Call c;
            c.on = call.on;
            c.method = horizontal ? "barh" : "bar";
            c.args = {numbersJson(positions), numbersJson(height), numbersJson(width)};
            c.kwargs.emplace_back("align", Json::string("center"));
            c.kwargs.emplace_back("log", Json::boolean(log));
            c.kwargs.emplace_back("color", colorJson(setColors[i]));
            c.kwargs.emplace_back(horizontal ? "left" : "bottom", *bottom);
            if (!bar(ax, c, error)) return false;
            patches.emplace_back(ax.containers.back().bars.begin(), ax.containers.back().bars.end());
            if (stacked) {
                bottomValues = tops[i];
                bottom = numbersJson(bottomValues);
            }
            for (std::size_t b = 0; b < nb; ++b) boffset[b] += dw[b];
        }
        for (std::size_t i = 1; i < patches.size(); ++i)
            for (Patch* r : patches[i]) r->stickyX.clear(), r->stickyY.clear();
        }
        // the keywords on every bar, the cycling ones last (kwargs.update(next(style_gen)))
        for (std::size_t i = 0; i < patches.size(); ++i) {
            for (std::size_t k = 0; k < patches[i].size(); ++k) {
                Patch& r = *patches[i][k];
                for (const auto& [name, value] : rest)
                    if (!patchProperty(r, name, *value, where, error)) return false;
                for (const Style& s : styles) {
                    const std::size_t at = i % s.size();
                    if (s.name == "facecolor") r.setFacecolor(s.colours[at]);
                    else if (s.name == "edgecolor") r.setEdgecolor(s.colours[at]);
                    else if (s.name == "hatch") r.setHatch(s.hatches[at], true);
                    else if (s.name == "linewidth") r.setLinewidth(s.linewidths[at]);
                    else r.setDashPattern(s.dashes[at].first, s.dashes[at].second);
                }
                if (k == 0 && i < labels.size()) r.label = labels[i];
                else if (k > 0) r.label = "_nolegend_";
            }
        }
        return true;
    }

    // Axes.vlines(x, ymin, ymax, colors=None, linestyles='solid', label='', **kwargs) and hlines
    bool lines(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        const bool vertical = call.method == "vlines";
        const char* names[3] = {vertical ? "x" : "y", vertical ? "ymin" : "xmin", vertical ? "ymax" : "xmax"};
        markDateUnits(ax, call);
        std::vector<double> v[3];
        for (int i = 0; i < 3; ++i) {
            const Json* a = argument(call, std::size_t(i), names[i]);
            double one = 0;
            if (!a) return *error = where + ": " + names[i] + " is required", false;
            if (readNumber(*a, &one)) v[i] = {one};
            else if (!numbers(*a, &v[i])) return *error = where + ": " + names[i] + " must be numbers", false;
        }
        const std::size_t n = v[0].size();
        // cbook._combine_masks(x, ymin, ymax): those as long as x share one mask, set where any of
        // them is not finite, and are NaN there (masked vertices); the others stay as they are
        std::vector<bool> bad(n, false);
        for (int i = 0; i < 3; ++i)
            if (v[i].size() == n)
                for (std::size_t k = 0; k < n; ++k) bad[k] = bad[k] || !std::isfinite(v[i][k]);
        for (int i = 0; i < 3; ++i)
            if (v[i].size() == n)
                for (std::size_t k = 0; k < n; ++k)
                    if (bad[k]) v[i][k] = std::numeric_limits<double>::quiet_NaN();
        for (int i = 1; i < 3; ++i) {
            if (v[i].size() == 1) {
                const double one = v[i][0];  // (assign must not read from the vector it replaces)
                v[i].assign(n, one);
            }
            if (v[i].size() != n) return *error = where + ": the lengths do not match", false;
        }
        std::vector<std::vector<std::array<double, 2>>> segments;
        for (std::size_t k = 0; k < n; ++k) {
            if (vertical) segments.push_back({{v[0][k], v[1][k]}, {v[0][k], v[2][k]}});
            else segments.push_back({{v[1][k], v[0][k]}, {v[2][k], v[0][k]}});
        }
        auto lc = std::make_unique<LineCollection>(rc_, segments);
        // LineCollection(verts, colors=colors, linestyles=linestyles, label=label), then the keywords
        // through their setters in the order given (_internal_update)
        std::vector<std::pair<std::string, const Json*>> props;
        const char* const own[3] = {"colors", "linestyles", "label"};
        for (std::size_t i = 0; i < 3; ++i)
            if (const Json* a = argument(call, 3 + i, own[i])) props.emplace_back(own[i], a);
        for (const auto& [name, value] : call.kwargs) {
            bool taken = false;
            for (const char* o : own) taken = taken || name == o;
            for (const char* o : names) taken = taken || name == o;
            if (!taken) props.emplace_back(name, &value);
        }
        for (const auto& [name, valuePointer] : props) {
            const Json& value = *valuePointer;
            double num = 0;
            if (name == "colors" || name == "color") {
                // (colors=None is the default; color=None, a keyword set after it, is the default colour again)
                if (value.isNull()) {
                    if (name == "color") lc->setEdgecolor(ColorArg{});
                    continue;
                }
                std::vector<colors::Spec> l;
                if (!colorList(value, &l)) return *error = where + ": bad colors", false;
                ColorArg a;
                if (l.size() == 1) a = ColorArg::of(l[0]);
                else a.kind = ColorArg::Colors, a.colors = l;
                lc->setEdgecolor(a);  // LineCollection.set_color
            } else if (name == "linestyles" || name == "linestyle" || name == "ls") {
                std::vector<std::string> styles;
                if (value.isString()) styles = {value.asString()};
                else if (const Json* seq = sequence(value)) {
                    for (const Json& s2 : seq->items())
                        if (s2.isString()) styles.push_back(s2.asString());
                }
                if (styles.empty() || !lc->setLinestyles(styles)) return *error = where + ": bad linestyles", false;
            } else if (name == "linewidths" || name == "linewidth" || name == "lw") {
                std::vector<double> w;
                if (readNumber(value, &num)) w = {num};
                else if (!numbers(value, &w)) return *error = where + ": bad linewidths", false;
                if (!finiteWidths(w)) return *error = where + ": bad linewidths", false;
                lc->setLinewidths(w);
            } else if (name == "alpha") {
                if (value.isNull()) lc->setAlpha(std::nullopt);  // set_alpha(None)
                else if (!readAlpha(value, &num)) return *error = where + ": alpha must be a number from 0 to 1", false;
                else lc->setAlpha(num);
            } else if (name == "zorder") {
                if (!readFinite(value, &num)) return *error = where + ": zorder must be a finite number", false;
                lc->zorder = num;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                lc->label = value.asString();
            } else if (name == "capstyle") {
                if (!value.isString() || (value.asString() != "butt" && value.asString() != "projecting" && value.asString() != "round"))
                    return *error = where + ": that is not a valid value for CapStyle; supported values are 'butt', 'projecting', 'round'", false;
                lc->capstyle = value.asString();
            } else {
                return *error = where + ": unsupported keyword " + name, false;
            }
        }
        ax.addLinesCollection(std::move(lc));
        return true;
    }

    bool lineProperty(Line2D& line, const std::string& name, const Json& v, const std::string& where, std::string* error) {
        double n = 0;
        if (name == "color") {
            if (v.isNull()) return true;
            if (!color(v, &line.color)) return *error = where + ": bad color", false;
        } else if (name == "linestyle") {
            if (v.isObject() && v.members()[0].first == "$tuple") {
                // _get_dash_pattern of (offset, on-off sequence or None): the line counts as '--'; the
                // offset is brought within one cycle of the dashes (Python's %)
                const Json& t = v.members()[0].second;
                if (t.size() != 2 || !t[0].isNumber()) return *error = where + ": bad linestyle", false;
                double offset = t[0].asNumber();
                std::optional<std::vector<double>> dashes;
                if (!t[1].isNull()) {
                    std::vector<double> d;
                    const Json* seq = sequence(t[1]);
                    if (!seq) return *error = where + ": bad linestyle", false;
                    for (const Json& x : seq->items()) {
                        if (!x.isNumber()) return *error = where + ": bad linestyle", false;
                        d.push_back(x.asNumber());
                    }
                    double sum = 0;
                    for (double x : d) sum += x;
                    if (sum != 0) {
                        double r = std::fmod(offset, sum);
                        if (r != 0 && ((r < 0) != (sum < 0))) r += sum;
                        offset = r;
                    }
                    dashes = d;
                }
                line.setDashPattern(offset, dashes);
            } else if (!v.isString() || !line.setLinestyle(v.asString(), error)) {
                return *error = where + ": bad linestyle", false;
            }
        } else if (name == "linewidth") {
            if (!readFinite(v, &n)) return *error = where + ": linewidth must be a finite number", false;
            line.setLinewidth(n);
        } else if (name == "marker") {
            std::string m;
            if (!markerFromJson(v, &m, error)) return *error = where + ": " + *error, false;
            if (!line.setMarker(m, error)) return false;
        } else if (name == "markersize") {
            if (!readNumber(v, &n)) return *error = where + ": markersize must be a number", false;
            line.markersize = n;
        } else if (name == "markeredgewidth") {
            if (!readNumber(v, &n)) return *error = where + ": markeredgewidth must be a number", false;
            line.markeredgewidth = n;
        } else if (name == "markevery") {
            if (!markevery(v, &line.markevery, where, error)) return false;
        } else if (name == "markerfacecolor" || name == "markeredgecolor" || name == "markerfacecoloralt") {
            colors::Spec c;
            if (!color(v, &c, {"auto"})) return *error = where + ": bad " + name, false;
            (name == "markerfacecolor" ? line.markerfacecolor : name == "markeredgecolor" ? line.markeredgecolor : line.markerfacecoloralt) = c;
        } else if (name == "alpha") {
            if (v.isNull()) line.alpha.reset();
            else if (!readAlpha(v, &n)) return *error = where + ": alpha must be a number from 0 to 1", false;
            else line.alpha = n;
        } else if (name == "solid_capstyle" || name == "dash_capstyle" || name == "solid_joinstyle" || name == "dash_joinstyle") {
            if (!v.isString()) return *error = where + ": " + name + " must be a string", false;
            const std::string s = v.asString();
            const bool cap = name.find("cap") != std::string::npos;
            if (cap ? (s != "butt" && s != "projecting" && s != "round") : (s != "miter" && s != "round" && s != "bevel"))
                return *error = where + ": bad " + name + " " + s, false;
            (name == "solid_capstyle" ? line.solidCapstyle : name == "dash_capstyle" ? line.dashCapstyle
             : name == "solid_joinstyle" ? line.solidJoinstyle : line.dashJoinstyle) = s;
        } else if (name == "antialiased") {
            if (!v.isBool()) return *error = where + ": antialiased must be True or False", false;
            line.antialiased = v.asBool();
        } else if (name == "zorder") {
            if (!readFinite(v, &n)) return *error = where + ": zorder must be a finite number", false;
            line.zorder = n;
        } else if (name == "label") {
            if (!v.isString()) return *error = where + ": label must be a string", false;
            line.label = v.asString();
        } else if (name == "visible") {
            if (!v.isBool()) return *error = where + ": visible must be True or False", false;
            line.visible = v.asBool();
        } else if (name == "drawstyle") {
            // set_drawstyle: None is 'default'
            static const char* const styles[] = {"default", "steps-mid", "steps-pre", "steps-post", "steps"};
            const std::string s = v.isNull() ? std::string("default") : v.isString() ? v.asString() : std::string();
            if (std::find(std::begin(styles), std::end(styles), s) == std::end(styles))
                return *error = where + ": drawstyle must be one of 'default', 'steps-mid', 'steps-pre', 'steps-post', 'steps'", false;
            line.setDrawstyle(s);
        } else if (name == "fillstyle") {
            if (!v.isString()) return *error = where + ": fillstyle must be a string", false;
            if (!line.markerStyle.set(line.markerStyle.marker(), v.asString(), rc_, error)) return false;
        } else {
            return *error = where + ": unsupported keyword " + name, false;
        }
        return true;
    }

    rc::Params rc_;
    DrawOptions options_;  // what measuring texts needs before the drawing (tight_layout)
    std::unique_ptr<Figure> figure_;
    std::map<std::string, Axes*> axes_;
    // The patches plt.Polygon made, by the names the figure file gave them, until add_patch takes them
    std::map<std::string, std::unique_ptr<Polygon>> patches_;
    std::map<std::string, std::shared_ptr<colors::Norm>> norms_;  // the norms colors.* calls made
    std::map<std::string, std::shared_ptr<tri::Triangulation>> triangulations_;  // the mtri.Triangulation calls'
    std::map<std::string, Mappable> mappables_;             // the images and collections named by "as"
    std::map<std::string, std::pair<ContourSet*, Axes*>> contourSets_;  // the contour sets named by "as", and their Axes
    // the quivers named by "as", and the PolyCollection keywords they were made with (polykw, for their keys)
    std::map<std::string, std::pair<Quiver*, std::vector<std::pair<std::string, Json>>>> quivers_;
    std::map<const Axes*, Mappable> currentImages_;         // pyplot's current image of each Axes (sci)
    std::map<std::string, std::vector<Axes*>> axesArrays_;  // the arrays of Axes plt.subplots returned
    std::set<std::string> addedPatches_;
    // The containers bar calls returned, by name: their Axes and where in its containers they are
    std::map<std::string, std::pair<Axes*, std::size_t>> barContainers_;
    std::set<std::string> figureNames_;  // what the figure file calls the figure
    Axes* current_ = nullptr;            // the figure's current Axes (gca), if any

public:
    // A name the figure file gives the Axes (the first in order), or "".
    std::string nameOf(const Axes* ax) const {
        for (const auto& [name, a] : axes_)
            if (a == ax) return name;
        return std::string();
    }
};

}  // namespace

// Runs the figure file's calls, then `draw` on the figure they made; returns its error or "".
template <typename Draw>
std::string runFigure(const model::Figure& model, const DrawOptions& options, Draw draw) {
    rc::Params rc = rc::Params::defaults();
    if (!model.style.isNull()) {
        std::string error;
        if (!rc::useStyle(rc, model.style, &error)) return "plt.style.use: " + error;
    }
    for (const auto& [key, value] : model.rc) {
        std::string error;
        if (!rc.set(key, value, &error)) return "rc " + key + ": " + error;
    }
    // A line made without a marker takes rcParams["lines.marker"], and matplotlib breaks there if it is
    // not one (or the engine, if it is one it does not draw yet): no figure is drawn with such a setting.
    {
        MarkerStyle probe;
        std::string error, marker;
        if (!markerFromJson(rc.value("lines.marker"), &marker, &error) || !probe.set(marker, std::string("full"), rc, &error))
            return "rc lines.marker: " + error;
    }
    // A transform that cannot be inverted (an Axes of no width, say) ends the drawing in matplotlib
    // where its inverse is first asked for ("Singular matrix").
    // The same for what else matplotlib raises while it draws (support::raised).
    transforms::singularMatrixSeen() = false;
    support::raised().clear();
    const char* const singular = "Singular matrix";
    Executor executor(std::move(rc), options);
    for (const auto& call : model.calls) {
        std::string error;
        if (!executor.run(call, &error)) return error;
        const std::string where = (call.on.empty() ? "plt" : call.on) + "." + call.method + ": ";
        if (transforms::singularMatrixSeen()) return where + singular;
        if (!support::raised().empty()) return where + support::raised();
    }
    Figure* figure = executor.currentFigure();
    if (!figure) return "the figure file makes no figure";
    std::string error;
    if constexpr (std::is_invocable_v<Draw, Figure&, const Executor&>) error = draw(*figure, executor);
    else error = draw(*figure);
    if (!error.empty()) return error;
    if (transforms::singularMatrixSeen()) return singular;
    return support::raised();
}

AggDrawing drawAgg(const model::Figure& model, const DrawOptions& options) {
    AggDrawing out;
    out.error = runFigure(model, options, [&](Figure& figure) {
        std::string error;
        out.trace = drawAggTrace(figure, options.locale, options.fontDirectory, &error);
        return error;
    });
    return out;
}

std::vector<std::string> axesNames(const model::Figure& model, const DrawOptions& options, std::string* error) {
    std::vector<std::string> names;
    *error = runFigure(model, options, [&](Figure& figure, const Executor& executor) {
        for (const auto& ax : figure.axes) names.push_back(executor.nameOf(ax.get()));
        return std::string();
    });
    return names;
}

std::string runCalls(const model::Figure& model, const DrawOptions& options) {
    return runFigure(model, options, [](Figure&) { return std::string(); });
}

AggDrawing drawScreen(const model::Figure& model, double ratio, const DrawOptions& options, std::optional<std::array<double, 2>> size) {
    AggDrawing out;
    out.error = runFigure(model, options, [&](Figure& figure) {
        std::string error;
        out.trace = drawScreenTrace(figure, ratio, options.locale, options.fontDirectory, &error, size);
        return error;
    });
    return out;
}

NavigationResult navigate(const model::Figure& model, double ratio, std::array<double, 2> size, const NavigationGesture& gesture,
                          const DrawOptions& options) {
    NavigationResult out;
    out.error = runFigure(model, options, [&](Figure& figure, const Executor& executor) {
        std::string error;
        drawScreenTrace(figure, ratio, options.locale, options.fontDirectory, &error, size);  // (the layout, the aspects)
        if (!error.empty()) return error;
        for (const LimitChange& change : navigate(figure, ratio, gesture)) {
            model::Call call;
            call.on = executor.nameOf(change.axes);
            if (call.on.empty()) return std::string("the Axes under the pointer has no name in the figure file");
            call.method = !change.method.empty() ? change.method : change.x ? "set_xlim" : "set_ylim";
            if (!change.method.empty()) call.args.push_back(support::Json::real(*change.lo));
            else
                for (const auto& v : {change.lo, change.hi}) call.args.push_back(v ? support::Json::real(*v) : support::Json());
            out.calls.push_back(std::move(call));
        }
        return std::string();
    });
    return out;
}

PointerTexts pointerTexts(const model::Figure& model, double ratio, std::array<double, 2> size,
                          const std::vector<std::array<double, 2>>& points, const DrawOptions& options) {
    PointerTexts out;
    out.error = runFigure(model, options, [&](Figure& figure) {
        std::string error;
        drawScreenTrace(figure, ratio, options.locale, options.fontDirectory, &error, size);
        if (!error.empty()) return error;
        for (const auto& p : points) out.texts.push_back(pointerText(figure, ratio, p[0], p[1]));
        return std::string();
    });
    return out;
}

SvgDrawing drawSvg(const model::Figure& model, const DrawOptions& options) {
    SvgDrawing out;
    out.error = runFigure(model, options, [&](Figure& figure) {
        const auto& salt = figure.rc().value("svg.hashsalt");
        std::string error;
        out.trace = drawSvgTrace(figure, options.locale, options.fontDirectory, kSvgMetadata, salt.isString() ? salt.asString() : kSvgHashSalt, &error);
        return error;
    });
    return out;
}

}  // namespace calculate_plot::engine
