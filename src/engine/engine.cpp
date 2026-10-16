#include "engine/engine.hpp"

#include "agg/path_text.h"
#include "engine/annotation.hpp"
#include "engine/axes.hpp"
#include "engine/figure.hpp"
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

bool readNumber(const Json& v, double* out) {
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
        if (call.on.empty() && call.method == "figure") return figure(call, error);
        if (call.on.empty() && call.method == "subplots") return subplots(call, error);
        if (call.on.empty()) {
            if (call.method == "gcf") {  // the current figure, made if there is none
                gcf();
                return nameResult(call, nullptr, error);
            }
            if (call.method == "gca") return noArguments(call, error) && nameResult(call, &gca(), error);
            if (call.method == "subplot") return subplot(call, error);
            if (call.method == "Polygon") return polygon(call, error);
            if (call.method == "axes") {
                // plt.axes(arg=None, **kwargs): add_subplot() without a rect, else add_axes(rect)
                if (call.args.size() > 1) return *error = "plt.axes: too many positional arguments", false;
                const Json* rect = call.args.empty() ? keyword(call, "arg") : &call.args[0];
                if (!rect || rect->isNull()) rect = keyword(call, "position");
                for (const auto& kw : call.kwargs)
                    if (kw.first != "arg" && kw.first != "position") return *error = "plt.axes: keyword arguments are not supported yet", false;
                if (!rect || rect->isNull()) {
                    gcf();
                    current_ = figure_->addSubplot(1, 1, 0, 0);
                    return nameResult(call, current_, error);
                }
                gcf();
                return addAxes(call, rect, error);
            }
            // the pyplot functions of the current Axes and of the current figure
            static const std::map<std::string, std::string> ofAxes = {
                {"plot", "plot"}, {"scatter", "scatter"}, {"fill_between", "fill_between"}, {"fill_betweenx", "fill_betweenx"}, {"errorbar", "errorbar"},
                {"axhline", "axhline"}, {"axvline", "axvline"}, {"axhspan", "axhspan"}, {"axvspan", "axvspan"},
                {"hlines", "hlines"}, {"vlines", "vlines"}, {"grid", "grid"}, {"legend", "legend"}, {"text", "text"},
                {"title", "set_title"}, {"xlabel", "set_xlabel"}, {"ylabel", "set_ylabel"}, {"xlim", "set_xlim"},
                {"ylim", "set_ylim"}, {"xscale", "set_xscale"}, {"yscale", "set_yscale"}, {"bar", "bar"}, {"barh", "barh"},
                {"hist", "hist"}, {"fill", "fill"}, {"step", "step"}, {"stairs", "stairs"}, {"stem", "stem"}, {"boxplot", "boxplot"}, {"violinplot", "violinplot"}, {"pie", "pie"}, {"annotate", "annotate"}, {"bar_label", "bar_label"}, {"loglog", "loglog"}, {"semilogx", "semilogx"}, {"semilogy", "semilogy"}};
            static const std::map<std::string, std::string> ofFigure = {
                {"suptitle", "suptitle"}, {"figtext", "text"}, {"subplots_adjust", "subplots_adjust"}, {"tight_layout", "tight_layout"}};
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

    // Axis.set_major_formatter / set_minor_formatter, with NullFormatter() so far. The formatter is the
    // one of the Ticker the Axes sharing this axis share.
    bool axisCall(Axis& axis, const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
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
        } else {
            *error = where + ": unsupported keyword " + name;
            return false;
        }
        return true;
    }

    // plt.figure(figsize=, dpi=, facecolor=, edgecolor=, frameon=)
    bool figure(const model::Call& call, std::string* error) {
        if (!call.args.empty()) {
            *error = "plt.figure: positional arguments are not supported";
            return false;
        }
        FigureOptions options;
        for (const auto& [name, value] : call.kwargs)
            if (!figureKeyword(name, value, options, "plt.figure", error)) return false;
        figure_ = std::make_unique<Figure>(rc_, options);
        if (!figure_->subplotpars.valid(error)) return *error = "plt.figure: " + *error, false;
        axes_.clear();
        figureNames_.clear();
        current_ = nullptr;
        if (call.as.size() == 1) figureNames_.insert(call.as[0]);
        return true;
    }

    // plt.subplots(nrows=1, ncols=1, **fig_kw) -> fig, ax (one Axes; grids of Axes come later)
    bool subplots(const model::Call& call, std::string* error) {
        double grid[2] = {1, 1};
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (!readNumber(call.args[i], &grid[i]) || call.args[i].numberText().find_first_of(".eE") != std::string::npos) {
                *error = "plt.subplots: nrows and ncols must be integers";
                return false;
            }
        FigureOptions options;
        bool squeeze = true;
        std::string sharex = "none", sharey = "none";
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
            } else if (!figureKeyword(name, value, options, "plt.subplots", error)) {
                return false;
            }
        }
        const int nrows = int(grid[0]), ncols = int(grid[1]);
        if (nrows < 1 || ncols < 1) return *error = "plt.subplots: nrows and ncols must be positive", false;
        figure_ = std::make_unique<Figure>(rc_, options);
        if (!figure_->subplotpars.valid(error)) return *error = "plt.subplots: " + *error, false;
        const std::vector<Axes*> made = figure_->subplots(nrows, ncols, sharex, sharey);
        axes_.clear();
        figureNames_.clear();
        current_ = made.back();  // (each add_subplot makes its Axes the current one)
        if (call.as.empty()) return true;
        if (call.as.size() != 2) return *error = "plt.subplots returns (fig, axs): name both", false;
        figureNames_.insert(call.as[0]);
        // the names the returned array answers to: squeezed to a single Axes or a 1-d array when it can be
        const std::string& n = call.as[1];
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
        if (call.method == "add_axes" && (call.args.size() != 1 || !call.kwargs.empty()))
            return *error = where + "a rect (left, bottom, width, height) and no keywords are supported so far", false;
        const Json* seq = rect ? sequence(*rect) : nullptr;
        double r[4];
        if (!seq || seq->size() != 4) return *error = where + "rect must be (left, bottom, width, height)", false;
        for (int i = 0; i < 4; ++i)
            if (!readNumber((*seq)[std::size_t(i)], &r[i])) return *error = where + "rect must be (left, bottom, width, height)", false;
        for (double v : r)
            if (!std::isfinite(v)) return *error = where + "all entries in rect must be finite not " + repr(*rect), false;
        if (r[2] < 0 || r[3] < 0) return *error = where + "Width and height specified must be non-negative", false;
        current_ = gcf().addAxes(r[0], r[1], r[2], r[3]);
        return nameResult(call, current_, error);
    }

    // plt.subplot(*args): the figure's Axes of that grid and cells, else a new one; either is current
    bool subplot(const model::Call& call, std::string* error) {
        if (!call.kwargs.empty()) return *error = "plt.subplot: keyword arguments are not supported yet", false;
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
                if (a->subplotSpec->gridspec == first) {
                    found = a.get();
                    break;
                }
            }
        current_ = found ? found : f.addSubplot(spec[0], spec[1], spec[2], spec[3]);
        return nameResult(call, current_, error);
    }

    bool figureCall(const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
        if (call.method == "add_subplot") {
            if (!call.kwargs.empty()) return *error = where + ": keyword arguments are not supported yet", false;
            int spec[4];
            if (!subplotSpecOf(call, spec, error)) return false;
            current_ = figure_->addSubplot(spec[0], spec[1], spec[2], spec[3]);
            return nameResult(call, current_, error);
        }
        if (call.method == "gca") return noArguments(call, error) && nameResult(call, &gca(), error);
        if (call.method == "add_axes") return addAxes(call, call.args.empty() ? nullptr : &call.args[0], error);
        if (call.method == "text") return figureText(call, error);
        if (call.method == "suptitle") {
            // FigureBase._suplabels with suptitle's info: (0.5, 0.98), centred, top, figure.titlesize/titleweight
            const Json* t = argument(call, 0, "t");
            if (!t || !t->isString()) return *error = where + ": the title must be a string", false;
            double x = 0.5, y = 0.98;
            for (const char* name : {"x", "y"})
                if (const Json* v = keyword(call, name); v && !v->isNull() && !readNumber(*v, name[0] == 'x' ? &x : &y))
                    return *error = where + ": " + name + " must be a number", false;
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
            figure_->texts.push_back(std::move(label));
            return true;
        }
        if (call.method == "subplots_adjust") {
            // subplots_adjust(left=None, bottom=None, right=None, top=None, wspace=None, hspace=None)
            const char* names[6] = {"left", "bottom", "right", "top", "wspace", "hspace"};
            if (!keywords(call, {"left", "bottom", "right", "top", "wspace", "hspace"}, error)) return false;
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
            return true;
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
        if (call.method == "loglog" || call.method == "semilogx" || call.method == "semilogy") {
            // set_xscale('log') and / or set_yscale('log') with the scale's keywords, then plot with the rest
            static const char* const scaleKeywords[] = {"base", "subs", "nonpositive", "basex", "subsx", "nonposx",
                                                        "basey", "subsy", "nonposy"};
            for (const auto& kw : call.kwargs)
                if (std::find(std::begin(scaleKeywords), std::end(scaleKeywords), kw.first) != std::end(scaleKeywords))
                    return *error = where + ": the keywords of the log scale are not supported yet", false;
            for (const char* scale : {"set_xscale", "set_yscale"}) {
                if ((call.method == "semilogx" && scale[4] == 'y') || (call.method == "semilogy" && scale[4] == 'x')) continue;
                model::Call c;
                c.on = call.on;
                c.method = scale;
                c.args = {Json::string("log")};
                if (!axesCall(ax, c, error)) return false;
            }
            model::Call c = call;
            c.method = "plot";
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
            return true;
        }
        if (call.method == "set_aspect") {
            // set_aspect(aspect, adjustable=None, anchor=None, share=False)
            const Json* a = argument(call, 0, "aspect");
            double value = 0;
            if (!a) return *error = where + ": aspect is required", false;
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
        if (call.method == "scatter") return scatter(ax, call, error);
        if (call.method == "fill_between" || call.method == "fill_betweenx") return fillBetween(ax, call, error);
        if (call.method == "errorbar") return errorbar(ax, call, error);
        if (call.method == "legend") return legend(ax, call, error);
        if (call.method == "grid") return grid(ax, call, error);
        if (call.method == "axhline" || call.method == "axvline") return axline(ax, call, error);
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
        if (call.method == "vlines" || call.method == "hlines") return lines(ax, call, error);
        if (call.method == "set_xscale" || call.method == "set_yscale") {
            if (!call.kwargs.empty()) return *error = where + ": scale options are not supported yet", false;
            if (call.args.size() != 1 || !call.args[0].isString()) return *error = where + ": give the scale's name", false;
            const std::string name = call.args[0].asString();
            if (name != "linear" && name != "log") return *error = where + ": the " + name + " scale is not supported yet", false;
            ax.setScale(call.method == "set_xscale", name);
            return true;
        }
        *error = "unsupported call: " + where;
        return false;
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
            collection->cmap = colors::ListedColormap::named(cmapName);
            if (!collection->cmap) return *error = where + ": the colormap " + cmapName + " is not supported yet", false;
            // the norm scales to the values that are not masked, and masked values map to 'bad'
            std::vector<double> values = *mappedValues, unmasked;
            for (std::size_t i = 0; i < n; ++i) {
                if (bad[i]) values[i] = nan;
                else unmasked.push_back(values[i]);
            }
            collection->array = values;
            collection->norm.vmin = vmin;
            collection->norm.vmax = vmax;
            if (!unmasked.empty()) collection->norm.autoscaleNone(unmasked);
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
                if (!value.isNull() && !(value.isBool() && !value.asBool())) return *error = where + ": interpolate is not supported yet", false;
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
        auto collection = std::make_unique<FillBetweenPolyCollection>(rc_, t, f1, f2, mask, stepKind, alongY);
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
            container.barlinecols.push_back(&ax.addLinesCollection(std::move(bars)));
            if (caps > 0) {
                for (const std::vector<double>* lh : {&low, &high}) {
                    if (axisIndex == 0) capLines.push_back(capLine(*lh, indep, "|"));
                    else capLines.push_back(capLine(indep, *lh, "_"));
                }
            }
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
        for (const auto& [name, value] : given) {
            if (isOwn(name)) continue;
            const std::string property = canonical(name);
            if (mode != TextKeywords::Internal && std::find(seen.begin(), seen.end(), property) != seen.end()) {
                *error = where + ": " + name + " is given twice (it is an alias of " + property + ")";
                return false;
            }
            seen.push_back(property);
            if (!set(name, *value, false)) return false;
        }
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
        if (v[5] && !v[5]->isNull()) return *error = where + ": arrowprops is not supported yet", false;
        std::optional<bool> clip;
        if (v[6] && !v[6]->isNull()) {
            if (!v[6]->isBool()) return *error = where + ": annotation_clip must be True, False or None", false;
            clip = v[6]->asBool();
        }
        auto a = std::make_unique<Annotation>(rc_, ax, v[0]->asString(), xy, xytext, xycoords, textcoords, clip);
        std::vector<std::pair<std::string, const Json*>> given;
        for (const auto& [name, value] : call.kwargs)
            if (std::find(std::begin(names), std::end(names), name) == std::end(names)) given.emplace_back(name, &value);
        if (!textProperties(*a, given, TextKeywords::Construct, where, error)) return false;
        // clip_on=True: clipped by the Axes' patch (a box); _add_text: the figure
        const Json* clipOn = keyword(call, "clip_on");
        if (clipOn && clipOn->isBool() && clipOn->asBool())
            a->clipBox = std::make_shared<transforms::TransformedBbox>(
                std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), ax.patch->fullTransform());
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
                if (!value.isString()) return *error = where + ": grid colours must be strings so far", false;
                if (!color(value, &checked)) return *error = where + ": bad " + name, false;
                style.gridColor = colors::Spec(value.asString()), styled = true;
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
        ax.addLine(std::move(line), true);
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
        // the whiskers: the farthest values within reach, but never inside the box
        std::optional<double> hi, lo;
        for (double v : x) {
            if (v <= hival && (!hi || v > *hi)) hi = v;
            if (v >= loval && (!lo || v < *lo)) lo = v;
        }
        s.whishi = !hi || *hi < s.q3 ? s.q3 : *hi;
        s.whislo = !lo || *lo > s.q1 ? s.q1 : *lo;
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
            if (axis.scale() == "log") return *error = where + ": boxplot ticks on a log axis are not supported yet", false;
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
                if (!readAlpha(value, &num)) return *error = where + ": alpha must be a number from 0 to 1", false;
                lc->setAlpha(num);
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
    std::set<std::string> addedPatches_;
    // The containers bar calls returned, by name: their Axes and where in its containers they are
    std::map<std::string, std::pair<Axes*, std::size_t>> barContainers_;
    std::set<std::string> figureNames_;  // what the figure file calls the figure
    Axes* current_ = nullptr;            // the figure's current Axes (gca), if any
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
    const std::string error = draw(*figure);
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

std::string runCalls(const model::Figure& model, const DrawOptions& options) {
    return runFigure(model, options, [](Figure&) { return std::string(); });
}

AggDrawing drawScreen(const model::Figure& model, double ratio, const DrawOptions& options) {
    AggDrawing out;
    out.error = runFigure(model, options, [&](Figure& figure) {
        std::string error;
        out.trace = drawScreenTrace(figure, ratio, options.locale, options.fontDirectory, &error);
        return error;
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
