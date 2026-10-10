#include "engine/engine.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

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
        *error = "unsupported call: " + (call.on.empty() ? "plt" : call.on) + "." + call.method;
        return false;
    }

    Figure* currentFigure() { return figure_.get(); }

private:
    // plt.figure(figsize=, dpi=, facecolor=, edgecolor=, frameon=)
    bool figure(const model::Call& call, std::string* error) {
        if (!call.args.empty()) {
            *error = "plt.figure: positional arguments are not supported";
            return false;
        }
        FigureOptions options;
        for (const auto& [name, value] : call.kwargs) {
            if (name == "figsize") {
                const Json* s = sequence(value);
                std::array<double, 2> size{};
                if (!s || s->size() != 2 || !readNumber((*s)[0], &size[0]) || !readNumber((*s)[1], &size[1])) {
                    *error = "plt.figure: figsize must be (width, height) in inches";
                    return false;
                }
                options.figsize = size;
            } else if (name == "dpi") {
                double dpi = 0;
                if (!readNumber(value, &dpi)) {
                    *error = "plt.figure: dpi must be a number";
                    return false;
                }
                options.dpi = dpi;
            } else if (name == "facecolor" || name == "edgecolor") {
                if (!value.isString()) {
                    *error = "plt.figure: " + name + " must be a colour string";
                    return false;
                }
                (name == "facecolor" ? options.facecolor : options.edgecolor) = value.asString();
            } else if (name == "frameon") {
                if (!value.isBool()) {
                    *error = "plt.figure: frameon must be True or False";
                    return false;
                }
                options.frameon = value.asBool();
            } else {
                *error = "plt.figure: unsupported keyword " + name;
                return false;
            }
        }
        figure_ = std::make_unique<Figure>(rc_, options);
        return true;
    }

    rc::Params rc_;
    std::unique_ptr<Figure> figure_;
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
