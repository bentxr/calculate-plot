#include "engine/engine.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "engine/legend.hpp"
#include "engine/tight_layout.hpp"
#include "rc/rc_params.hpp"
#include "rc/styles.hpp"
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
        const auto ax = axes_.find(call.on);
        if (ax != axes_.end()) return axesCall(*ax->second, call, error);
        if (!figureName_.empty() && call.on == figureName_) return figureCall(call, error);
        *error = "unsupported call: " + (call.on.empty() ? "plt" : call.on) + "." + call.method;
        return false;
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
        figureName_ = call.as.size() == 1 ? call.as[0] : std::string();
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
        figureName_.clear();
        if (call.as.empty()) return true;
        if (call.as.size() != 2) return *error = "plt.subplots returns (fig, axs): name both", false;
        figureName_ = call.as[0];
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

    bool figureCall(const model::Call& call, std::string* error) {
        const std::string where = call.on + "." + call.method;
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
        if (call.method == "plot") return plot(ax, call, error);
        if (call.method == "scatter") return scatter(ax, call, error);
        if (call.method == "fill_between") return fillBetween(ax, call, error);
        if (call.method == "errorbar") return errorbar(ax, call, error);
        if (call.method == "legend") return legend(ax, call, error);
        if (call.method == "grid") return grid(ax, call, error);
        if (call.method == "axhline" || call.method == "axvline") return axline(ax, call, error);
        if (call.method == "axhspan" || call.method == "axvspan") return axspan(ax, call, error);
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
            *colorOut = colors::Spec(fmt);
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
        for (std::size_t i = 0; i < xs.size(); ++i)
            if (!std::isfinite(xs[i]) || !std::isfinite(ys[i]))
                return *error = where + ": non-finite points are not supported yet", false;
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
        std::string marker = rc_.text("scatter.marker");
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
                if (!value.isString()) return *error = where + ": marker must be a string", false;
                marker = value.asString();
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
        for (std::size_t i = 0; i < xs.size(); ++i) collection->offsets.push_back({xs[i], ys[i]});
        collection->offsetTransform = ax.transData;
        if (alpha) collection->setAlpha(alpha);
        if (mapped) {  // set_cmap, set_norm(Normalize()), set_array(c), _scale_norm
            collection->cmap = colors::ListedColormap::named(cmapName);
            if (!collection->cmap) return *error = where + ": the colormap " + cmapName + " is not supported yet", false;
            collection->array = mappedValues;
            collection->norm.vmin = vmin;
            collection->norm.vmax = vmax;
            collection->norm.autoscaleNone(*mappedValues);
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
    bool fillBetween(Axes& ax, const model::Call& call, std::string* error) {
        const std::string where = call.on + ".fill_between";
        const Json* xa = call.args.size() > 0 ? &call.args[0] : keyword(call, "x");
        const Json* y1a = call.args.size() > 1 ? &call.args[1] : keyword(call, "y1");
        const Json* y2a = call.args.size() > 2 ? &call.args[2] : keyword(call, "y2");
        if (call.args.size() > 3) return *error = where + ": too many positional arguments", false;
        std::vector<double> t, f1, f2;
        if (!xa || !numbers(*xa, &t)) return *error = where + ": x must be a list of numbers", false;
        // y1 and y2: lists, or numbers broadcast over x
        const auto values = [&](const Json* j, std::vector<double>* out, double fallback) {
            double one = fallback;
            if (!j || j->isNull() || readNumber(*j, &one)) {
                out->assign(t.size(), one);
                return true;
            }
            return numbers(*j, out) && out->size() == t.size();
        };
        if (!y1a || !values(y1a, &f1, 0.0)) return *error = where + ": y1 must be a number or a list as long as x", false;
        if (!values(y2a, &f2, 0.0)) return *error = where + ": y2 must be a number or a list as long as x", false;
        std::vector<bool> mask(t.size(), true);
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
            if (name == "x" || name == "y1" || name == "y2") continue;
            if (name == "where") {
                const Json* seq = sequence(value);
                if (value.isNull()) continue;
                if (!seq || seq->size() != t.size()) return *error = where + ": where must be booleans, one per x", false;
                for (std::size_t i = 0; i < t.size(); ++i) {
                    if (!(*seq)[i].isBool()) return *error = where + ": where must be booleans", false;
                    mask[i] = (*seq)[i].asBool();
                }
            } else if (name == "interpolate") {
                if (!value.isNull() && !(value.isBool() && !value.asBool())) return *error = where + ": interpolate is not supported yet", false;
            } else if (name == "step") {
                // (None is no steps; False is not: matplotlib breaks on it)
                if (!value.isNull()) return *error = where + ": step is not supported yet", false;
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
        auto collection = std::make_unique<FillBetweenPolyCollection>(rc_, t, f1, f2, mask);
        // the keyword arguments are set after the defaults (_internal_update)
        collection->setFacecolor(face);
        if (edge.kind != ColorArg::Default) collection->setEdgecolor(edge);
        collection->setHatchcolor(hatch);
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
                if (!value.isString()) return *error = where + ": loc must be a location name", false;
                o.loc = value.asString();
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
            } else {
                static const char* const numeric[] = {"framealpha", "borderpad", "labelspacing", "handlelength",
                                                      "handletextpad", "borderaxespad", "columnspacing"};
                std::optional<double>* targets[] = {&o.framealpha, &o.borderpad, &o.labelspacing, &o.handlelength,
                                                    &o.handletextpad, &o.borderaxespad, &o.columnspacing};
                bool found = false;
                for (int i = 0; i < 7; ++i)
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

    bool patchProperty(Rectangle& p, const std::string& name, const Json& v, const std::string& where, std::string* error) {
        double n = 0;
        const auto text = [&](std::string* out) {
            if (!v.isString()) return false;
            *out = v.asString();
            return true;
        };
        std::string str;
        if (name == "color" || name == "facecolor" || name == "fc" || name == "edgecolor" || name == "ec") {
            // set_color / set_facecolor / set_edgecolor: a colour that to_rgba takes
            colors::Spec spec;
            std::string why;
            if (!color(v, &spec, {}, &why))
                return *error = where + ": " + name + " is not a colour" + (why.empty() ? "" : ": " + why), false;
            if (name == "color") p.setColor(spec);
            else if (name[0] == 'f') p.setFacecolor(spec);
            else p.setEdgecolor(spec);
        } else if (name == "alpha") {
            if (!readAlpha(v, &n)) return *error = where + ": alpha must be a number from 0 to 1", false;
            p.setAlpha(n);
        } else if (name == "linewidth" || name == "lw") {
            if (!readFinite(v, &n)) return *error = where + ": linewidth must be a finite number", false;
            p.setLinewidth(n);
        } else if (name == "linestyle" || name == "ls") {
            // Patch.set_linestyle: a name of mlines._get_dash_pattern ('None', ' ' and '' draw no edge)
            static const char* const styles[] = {"-", "--", "-.", ":", "solid", "dashed", "dashdot", "dotted", "None", "none", " ", ""};
            if (!text(&str) || std::find_if(std::begin(styles), std::end(styles), [&](const char* s) { return str == s; }) == std::end(styles))
                return *error = where + ": bad linestyle", false;
            p.setLinestyle(str == "-" ? "solid" : str == "--" ? "dashed" : str == "-." ? "dashdot" : str == ":" ? "dotted" : str);
        } else if (name == "fill") {
            if (!v.isBool()) return *error = where + ": fill must be True or False", false;
            p.setFill(v.asBool());
        } else if (name == "hatch") {
            if (!v.isNull()) return *error = where + ": hatches are not supported yet", false;
        } else if (name == "zorder") {
            if (!readFinite(v, &n)) return *error = where + ": zorder must be a finite number", false;
            p.zorder = n;
        } else if (name == "label") {
            if (!text(&str)) return *error = where + ": label must be a string", false;
            p.label = str;
        } else {
            return *error = where + ": unsupported keyword " + name, false;
        }
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
            if (!v.isString() || !line.setLinestyle(v.asString(), error)) return *error = where + ": bad linestyle", false;
        } else if (name == "linewidth") {
            if (!readFinite(v, &n)) return *error = where + ": linewidth must be a finite number", false;
            line.setLinewidth(n);
        } else if (name == "marker") {
            if (!v.isString()) return *error = where + ": marker must be a string", false;
            if (!line.setMarker(v.asString(), error)) return false;
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
    std::string figureName_;
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
        std::string error;
        if (!probe.set(rc.text("lines.marker"), std::string("full"), rc, &error)) return "rc lines.marker: " + error;
    }
    // A transform that cannot be inverted (an Axes of no width, say) ends the drawing in matplotlib
    // where its inverse is first asked for ("Singular matrix").
    transforms::singularMatrixSeen() = false;
    const char* const singular = "Singular matrix";
    Executor executor(std::move(rc), options);
    for (const auto& call : model.calls) {
        std::string error;
        if (!executor.run(call, &error)) return error;
        if (transforms::singularMatrixSeen()) return (call.on.empty() ? "plt" : call.on) + "." + call.method + ": " + singular;
    }
    Figure* figure = executor.currentFigure();
    if (!figure) return "the figure file makes no figure";
    const std::string error = draw(*figure);
    return error.empty() && transforms::singularMatrixSeen() ? singular : error;
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
