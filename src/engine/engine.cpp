#include "engine/engine.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <cctype>
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
            const auto view = axis.viewInterval();
            double v[2] = {view[0], view[1]};
            const Json* given[2] = {call.args.size() > 0 ? &call.args[0] : keyword(call, lo),
                                    call.args.size() > 1 ? &call.args[1] : keyword(call, hi)};
            if (call.args.size() == 1 && sequence(call.args[0]) && sequence(call.args[0])->size() == 2) {
                given[0] = &(*sequence(call.args[0]))[0];
                given[1] = &(*sequence(call.args[0]))[1];
            }
            for (int i = 0; i < 2; ++i)
                if (given[i] && !given[i]->isNull() && !readNumber(*given[i], &v[i]))
                    return *error = where + ": limits must be numbers", false;
            axis.setLim(v[0], v[1], false);
            return true;
        }
        *error = "unsupported call: " + where;
        return false;
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
