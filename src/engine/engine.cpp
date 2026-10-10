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
        std::optional<double> zorder;
        std::string label;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "x" || name == "y" || name == "s" || name == "c") continue;
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
        if (c && !c->isNull()) {
            std::vector<double> values;
            if (!c->isString() && numbers(*c, &values) && values.size() == xs.size() && !(values.size() == 3 || values.size() == 4))
                return *error = where + ": colour-mapped c values are not supported yet", false;
            if (!colorList(*c, &faces)) return *error = where + ": 'c' must be a color or a sequence of colors", false;
        } else if (facecolors) {
            faces = *facecolors;
        } else {
            faces = {ax.nextPatchColor()};
        }
        if (faces.size() != 1 && faces.size() != xs.size() && !faces.empty())
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
        ColorArg faceArg = argOf(faces), edgeArg;
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
        collection->transform = std::make_shared<transforms::IdentityTransform>();
        if (zorder) collection->zorder = *zorder;
        collection->label = label;
        ax.addCollection(std::move(collection));
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
