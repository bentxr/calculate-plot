#include "engine/engine.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
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

// The figure file's calls, run against the objects they make.
class Executor {
public:
    explicit Executor(rc::Params rc) : rc_(std::move(rc)) {}

    bool run(const model::Call& call, std::string* error) {
        if (call.on.empty() && call.method == "figure") return figure(call, error);
        if (call.on.empty() && call.method == "subplots") return subplots(call, error);
        const auto ax = axes_.find(call.on);
        if (ax != axes_.end()) return axesCall(*ax->second, call, error);
        *error = "unsupported call: " + (call.on.empty() ? "plt" : call.on) + "." + call.method;
        return false;
    }

    Figure* currentFigure() { return figure_.get(); }

private:
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
            options.figsize = size;
        } else if (name == "dpi") {
            double dpi = 0;
            if (!readNumber(value, &dpi)) {
                *error = where + ": dpi must be a number";
                return false;
            }
            options.dpi = dpi;
        } else if (name == "facecolor" || name == "edgecolor") {
            if (!value.isString()) {
                *error = where + ": " + name + " must be a colour string";
                return false;
            }
            (name == "facecolor" ? options.facecolor : options.edgecolor) = value.asString();
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
        return true;
    }

    // plt.subplots(nrows=1, ncols=1, **fig_kw) -> fig, ax (one Axes; grids of Axes come later)
    bool subplots(const model::Call& call, std::string* error) {
        double grid[2] = {1, 1};
        if (call.args.size() > 2) {
            *error = "plt.subplots: too many positional arguments";
            return false;
        }
        for (std::size_t i = 0; i < call.args.size(); ++i)
            if (!readNumber(call.args[i], &grid[i]) || call.args[i].numberText().find_first_of(".eE") != std::string::npos) {
                *error = "plt.subplots: nrows and ncols must be integers";
                return false;
            }
        FigureOptions options;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "nrows" || name == "ncols") {
                if (!readNumber(value, &grid[name == "nrows" ? 0 : 1])) {
                    *error = "plt.subplots: " + name + " must be an integer";
                    return false;
                }
            } else if (!figureKeyword(name, value, options, "plt.subplots", error)) {
                return false;
            }
        }
        if (grid[0] != 1 || grid[1] != 1) {
            *error = "plt.subplots: grids of several Axes are not supported yet";
            return false;
        }
        figure_ = std::make_unique<Figure>(rc_, options);
        Axes* ax = figure_->subplots(1, 1).front();
        axes_.clear();
        if (call.as.size() == 2) axes_[call.as[1]] = ax;
        else if (!call.as.empty()) {
            *error = "plt.subplots returns (fig, ax): name both";
            return false;
        }
        return true;
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
            if (!keywords(call, {"label", "loc", "pad", "y"}, error)) return false;
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
            ax.setTitle(label, loc, pad, y);
            return true;
        }
        if (call.method == "set_xlabel" || call.method == "set_ylabel") {
            const bool isX = call.method == "set_xlabel";
            if (!keywords(call, {isX ? "xlabel" : "ylabel", "labelpad", "loc"}, error)) return false;
            std::string label, loc = rc_.text(isX ? "xaxis.labellocation" : "yaxis.labellocation");
            if (!(call.args.size() == 1 ? text(&call.args[0], &label) : text(keyword(call, isX ? "xlabel" : "ylabel"), &label))) {
                *error = where + ": the label must be a string";
                return false;
            }
            Axis& axis = isX ? *ax.xaxis : *ax.yaxis;
            if (const Json* v = keyword(call, "labelpad")) {
                double n = 0;
                if (!readNumber(*v, &n)) return *error = where + ": labelpad must be a number", false;
                axis.labelpad = n;
            }
            if (const Json* l = keyword(call, "loc"); l && !text(l, &loc)) return *error = where + ": bad loc", false;
            const bool okLoc = isX ? (loc == "left" || loc == "center" || loc == "right")
                                   : (loc == "bottom" || loc == "center" || loc == "top");
            if (!okLoc) return *error = where + ": bad loc " + loc, false;
            axis.setLabelText(label, loc);
            return true;
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
                if (!given[i] || given[i]->isNull()) missing = true;
                else if (!readNumber(*given[i], &v[i])) return *error = where + ": limits must be numbers", false;
            }
            if (missing) {  // the old limits fill in, read through viewLim (a pending autoscale runs first)
                const auto view = axis.viewInterval();
                for (int i = 0; i < 2; ++i)
                    if (!given[i] || given[i]->isNull()) v[i] = view[i];
            }
            axis.setLim(v[0], v[1], false);
            return true;
        }
        if (call.method == "plot") return plot(ax, call, error);
        if (call.method == "scatter") return scatter(ax, call, error);
        if (call.method == "fill_between") return fillBetween(ax, call, error);
        if (call.method == "errorbar") return errorbar(ax, call, error);
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

    bool color(const Json& v, colors::Spec* out) {
        if (v.isString()) {
            *out = v.asString();
            return true;
        }
        std::vector<double> t;
        if (!numbers(v, &t) || (t.size() != 3 && t.size() != 4)) return false;
        *out = t;
        return true;
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
                return *error = "'" + fmt + "' is not a valid format string (unrecognized character '" + std::string(1, c) + "')", false;
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
        std::vector<std::pair<std::string, const Json*>> props;
        for (const auto& [name, value] : call.kwargs) {
            static const std::pair<const char*, const char*> aliases[] = {
                {"aa", "antialiased"}, {"c", "color"}, {"ds", "drawstyle"}, {"ls", "linestyle"}, {"lw", "linewidth"},
                {"mec", "markeredgecolor"}, {"mew", "markeredgewidth"}, {"mfc", "markerfacecolor"},
                {"mfcalt", "markerfacecoloralt"}, {"ms", "markersize"}};
            std::string full = name;
            for (const auto& [a, f] : aliases)
                if (name == a) full = f;
            props.emplace_back(full, &value);
        }
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
            // kw from the format, then the keyword arguments over it
            bool hasColor = fmtColor.has_value();
            if (linestyle && !line->setLinestyle(*linestyle, error)) return false;
            if (marker && !line->setMarker(*marker, error)) return false;
            if (fmtColor) line->color = *fmtColor;
            for (const auto& [name, value] : props) {
                if (!lineProperty(*line, name, *value, where, error)) return false;
                if (name == "color" && !value->isNull()) hasColor = true;
            }
            if (!hasColor) line->color = ax.nextColor();  // _PropCycle.getdefaults
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
        if (!s || s->isNull()) sizes = {std::pow(rc_.number("lines.markersize"), 2.0)};
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
        for (const auto& [name, value] : call.kwargs) {
            if (name == "x" || name == "y" || name == "s" || name == "c") continue;
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
                    if (!readNumber(value, &a)) return *error = where + ": alpha must be a number", false;
                    alpha = a;
                }
            } else if (name == "linewidths" || name == "linewidth" || name == "lw") {
                double one = 0;
                if (readNumber(value, &one)) linewidths = {one};
                else if (!numbers(value, &linewidths)) return *error = where + ": bad linewidths", false;
                linewidthsGiven = true;
            } else if (name == "zorder") {
                double z = 0;
                if (!readNumber(value, &z)) return *error = where + ": zorder must be a number", false;
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
        if (!edgecolors && !edgeFace && !edgeNone) {
            const std::string e = rc_.text("scatter.edgecolors");
            if (e == "face") edgeFace = true;
            else if (e == "none") edgeNone = true;
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
        else edgeArg = argOf(*edgecolors);
        if (!style.filled()) {
            if (style.fillstyle() == "none") {
                edgeArg = faceArg;
                faceArg = ColorArg{};
                faceArg.kind = ColorArg::None;
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
        if (mappedValues) {  // set_cmap, set_norm(Normalize()), set_array(c), _scale_norm
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
        ColorArg face, edge;
        bool faceGiven = false;
        std::optional<double> alpha, zorder;
        std::vector<double> linewidths;
        std::string label;
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
            } else if (name == "interpolate" || name == "step") {
                if (!value.isNull() && !(value.isBool() && !value.asBool()))
                    return *error = where + ": " + name + " is not supported yet", false;
            } else if (name == "color" || name == "facecolor" || name == "facecolors" || name == "fc") {
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad " + name, false;
                face = ColorArg::of(c);
                faceGiven = true;
                if (name == "color") edge = ColorArg::of(c);  // set_color: face and edge
            } else if (name == "edgecolor" || name == "edgecolors" || name == "ec") {
                colors::Spec c;
                if (!color(value, &c)) return *error = where + ": bad " + name, false;
                edge = ColorArg::of(c);
            } else if (name == "alpha") {
                double a = 0;
                if (!value.isNull()) {
                    if (!readNumber(value, &a)) return *error = where + ": alpha must be a number", false;
                    alpha = a;
                }
            } else if (name == "linewidth" || name == "linewidths" || name == "lw") {
                double w = 0;
                if (!readNumber(value, &w)) return *error = where + ": linewidth must be a number", false;
                linewidths = {w};
            } else if (name == "zorder") {
                double z = 0;
                if (!readNumber(value, &z)) return *error = where + ": zorder must be a number", false;
                zorder = z;
            } else if (name == "label") {
                if (!value.isString()) return *error = where + ": label must be a string", false;
                label = value.asString();
            } else {
                return *error = where + ": unsupported keyword " + name, false;
            }
        }
        if (!faceGiven) face = ColorArg::of(ax.nextPatchColor());
        auto collection = std::make_unique<FillBetweenPolyCollection>(rc_, t, f1, f2, mask);
        // the keyword arguments are set after the defaults (_internal_update)
        collection->setFacecolor(face);
        if (edge.kind != ColorArg::Default) collection->setEdgecolor(edge);
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
        if (fmtJson && !fmtJson->isNull()) {
            if (!fmtJson->isString()) return *error = where + ": fmt must be a string", false;
            fmt = fmtJson->asString();
        }
        std::optional<colors::Spec> ecolor;
        std::optional<double> elinewidth, capsize, capthick;
        bool barsabove = false;
        // the Line2D keyword arguments (aliases normalised, None dropped)
        std::vector<std::pair<std::string, const Json*>> props;
        static const std::pair<const char*, const char*> aliases[] = {
            {"aa", "antialiased"}, {"c", "color"}, {"ds", "drawstyle"}, {"ls", "linestyle"}, {"lw", "linewidth"},
            {"mec", "markeredgecolor"}, {"mew", "markeredgewidth"}, {"mfc", "markerfacecolor"},
            {"mfcalt", "markerfacecoloralt"}, {"ms", "markersize"}};
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
                if (!readNumber(value, &n)) return *error = where + ": " + name + " must be a number", false;
                (name == "elinewidth" ? elinewidth : name == "capsize" ? capsize : capthick) = n;
            } else if (name == "barsabove") {
                if (!value.isBool()) return *error = where + ": barsabove must be True or False", false;
                barsabove = value.asBool();
            } else if (name == "lolims" || name == "uplims" || name == "xlolims" || name == "xuplims" ||
                       name == "errorevery" || name == "elinestyle") {
                return *error = where + ": " + name + " is not supported yet", false;
            } else {
                if (value.isNull()) continue;
                std::string full = name;
                for (const auto& [a, f] : aliases)
                    if (name == a) full = f;
                if (full == "zorder") {
                    double z = 0;
                    if (!readNumber(value, &z)) return *error = where + ": zorder must be a number", false;
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
        if (lowerFmt != "none") {
            auto line = std::make_unique<Line2D>(rc_, x, y);
            if (linestyle && !line->setLinestyle(*linestyle, error)) return false;
            if (marker && !line->setMarker(*marker, error)) return false;
            if (fmtColor) line->color = *fmtColor;
            for (const auto& [name, value] : props)
                if (!lineProperty(*line, name, *value, where, error)) return false;
            if (!baseColorSet) {  // _PropCycle.getdefaults
                baseColor = ax.nextColor();
                baseColorSet = true;
            }
            line->color = baseColor;
            line->label = "_nolegend_";
            line->zorder = barsabove ? zorder - .1 : zorder + .1;
            ax.addLine(std::move(line));
        } else {
            // no data line; base_style keeps the kwargs' colour only
            if (!prop("color")) baseColorSet = false;
        }
        if (!baseColorSet) baseColor = std::string("C0");
        const colors::Spec eColor = ecolor ? *ecolor : baseColor;
        // base_style keeps what is not about markers or line styles
        std::optional<double> linewidth, alpha;
        double n = 0;
        if (const Json* v = prop("linewidth"); v && readNumber(*v, &n)) linewidth = n;
        if (const Json* v = prop("alpha"); v && readNumber(*v, &n)) alpha = n;
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
            ax.addLinesCollection(std::move(bars));
            if (caps > 0) {
                for (const std::vector<double>* lh : {&low, &high}) {
                    if (axisIndex == 0) capLines.push_back(capLine(*lh, indep, "|"));
                    else capLines.push_back(capLine(indep, *lh, "_"));
                }
            }
        }
        for (auto& l : capLines) ax.addLine(std::move(l));
        ax.requestAutoscaleView(true, true);
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
            if (!readNumber(v, &n)) return *error = where + ": linewidth must be a number", false;
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
            if (!color(v, &c)) return *error = where + ": bad " + name, false;
            (name == "markerfacecolor" ? line.markerfacecolor : name == "markeredgecolor" ? line.markeredgecolor : line.markerfacecoloralt) = c;
        } else if (name == "alpha") {
            if (v.isNull()) line.alpha.reset();
            else if (!readNumber(v, &n)) return *error = where + ": alpha must be a number", false;
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
            if (!readNumber(v, &n)) return *error = where + ": zorder must be a number", false;
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
    std::unique_ptr<Figure> figure_;
    std::map<std::string, Axes*> axes_;
};

}  // namespace

// Runs the figure file's calls, then `draw` on the figure they made; returns its error or "".
template <typename Draw>
std::string runFigure(const model::Figure& model, Draw draw) {
    rc::Params rc = rc::Params::defaults();
    for (const auto& [key, value] : model.rc) {
        std::string error;
        if (!rc.set(key, value, &error)) return "rc " + key + ": " + error;
    }
    Executor executor(std::move(rc));
    for (const auto& call : model.calls) {
        std::string error;
        if (!executor.run(call, &error)) return error;
    }
    Figure* figure = executor.currentFigure();
    if (!figure) return "the figure file makes no figure";
    return draw(*figure);
}

AggDrawing drawAgg(const model::Figure& model, const DrawOptions& options) {
    AggDrawing out;
    out.error = runFigure(model, [&](Figure& figure) {
        std::string error;
        out.trace = drawAggTrace(figure, options.locale, options.fontDirectory, &error);
        return error;
    });
    return out;
}

SvgDrawing drawSvg(const model::Figure& model, const DrawOptions& options) {
    SvgDrawing out;
    out.error = runFigure(model, [&](Figure& figure) {
        const auto& salt = figure.rc().value("svg.hashsalt");
        std::string error;
        out.trace = drawSvgTrace(figure, options.locale, options.fontDirectory, kSvgMetadata, salt.isString() ? salt.asString() : kSvgHashSalt, &error);
        return error;
    });
    return out;
}

}  // namespace calculate_plot::engine
