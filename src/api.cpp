// The public drawing API (include/calculate-plot/calculate-plot.hpp) over the engine and the renderers.
#include "calculate-plot/calculate-plot.hpp"

#include "engine/engine.hpp"
#include "model/figure_model.hpp"
#include "png/png.hpp"
#include "render/replay.hpp"
#include "render/svg_replay.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"
#include "support/json_writer.hpp"
#include "text/text_render.h"

#include <functional>
#include <map>

namespace calculate_plot {

namespace {

std::optional<model::Figure> parsed(std::string_view figureFile, std::string* error) {
    std::string why;
    auto figure = model::parseFigure(figureFile, &why);
    if (!figure && error) *error = why;
    return figure;
}

// The pixels of an Agg trace (the renderer's port draws it)
std::optional<png::Image> pixels(const engine::AggDrawing& drawing, const std::string& fontDirectory, std::string* error) {
    if (!drawing.error.empty()) return *error = drawing.error, std::nullopt;
    render::ReplayOptions replay;
    replay.fontDirectory = fontDirectory;
    auto result = render::replayTrace(drawing.trace, replay);
    if (!result.error.empty()) return *error = result.error, std::nullopt;
    return std::move(result.image);
}

using support::Json;

void write(support::JsonWriter& w, const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: w.null(); break;
    case Json::Type::Bool: w.value(v.asBool()); break;
    case Json::Type::Number: w.raw(v.numberText()); break;
    case Json::Type::String: w.value(v.asString()); break;
    case Json::Type::Array:
        w.beginArray();
        for (const Json& x : v.items()) write(w, x);
        w.endArray();
        break;
    case Json::Type::Object:
        w.beginObject();
        for (const auto& [k, x] : v.members()) {
            w.key(k);
            write(w, x);
        }
        w.endObject();
        break;
    }
}

// The arguments a call needs at the least, as a figure file writes them
const std::map<std::string, std::string>& leastArguments() {
    static const std::map<std::string, std::string> args = {
        {"plot", "[[0, 1], [0, 1]]"},     {"scatter", "[[0, 1], [0, 1]]"},  {"fill_between", "[[0, 1], [0, 1]]"}, {"fill_betweenx", "[[0, 1], [0, 1]]"},
        {"errorbar", "[[0, 1], [0, 1]]"}, {"loglog", "[[1, 2], [1, 2]]"},   {"semilogx", "[[1, 2], [1, 2]]"},
        {"semilogy", "[[1, 2], [1, 2]]"}, {"axhspan", "[0, 1]"},  {"bar", "[[1, 2], [1, 2]]"}, {"barh", "[[1, 2], [1, 2]]"}, {"hist", "[[1, 2, 3]]"}, {"fill", "[[0, 1, 1], [0, 0, 1]]"}, {"annotate", "[\"a\", [0.5, 0.5]]"}, {"bar_label", "[{\"$ref\": \"bars\"}]"}, {"step", "[[0, 1], [0, 1]]"}, {"stairs", "[[1, 2]]"}, {"stem", "[[1, 2]]"}, {"boxplot", "[[1, 2, 3]]"}, {"violinplot", "[[1, 2, 3]]"}, {"pie", "[[1, 2, 3]]"},            {"axvspan", "[0, 1]"},
        {"hlines", "[[1], 0, 1]"},        {"vlines", "[[1], 0, 1]"},        {"set_title", "[\"a\"]"},
        {"set_xlabel", "[\"a\"]"},      {"set_ylabel", "[\"a\"]"},      {"set_xlim", "[]"},
        {"set_ylim", "[]"},           {"set_xscale", "[\"log\"]"},    {"set_yscale", "[\"log\"]"},
        {"add_axes", "[[0.1, 0.1, 0.8, 0.8]]"}, {"suptitle", "[\"a\"]"},  {"inset_axes", "[[0.5, 0.5, 0.4, 0.4]]"}, {"indicate_inset", "[[0, 0, 0.5, 0.5]]"}, {"indicate_inset_zoom", "[{\"$ref\": \"ins\"}]"},
        {"set_major_formatter", "[{\"$new\": \"NullFormatter\"}]"},
        {"set_minor_formatter", "[{\"$new\": \"NullFormatter\"}]"}};
    return args;
}

// A figure file that makes the call, with `keyword` set to `value` (JSON) when there is one
std::string probe(const std::string& on, const std::string& call, const std::string& keyword, const std::string& value,
                  const std::string& givenArgs = std::string()) {
    std::string args = "[]";
    const auto it = leastArguments().find(call);
    if (it != leastArguments().end()) args = it->second;
    if (call == "text") args = on == "figure" ? "[0.5, 0.5, \"a\"]" : "[0, 0, \"a\"]";
    if (!givenArgs.empty()) args = givenArgs;
    std::string kwargs = keyword.empty() ? "{}" : "{\"" + keyword + "\": " + value + "}";
    std::string setup, receiver;
    if (on != "plt") {
        setup = R"({"call": "subplots", "as": ["fig", "ax"]}, )";
        // (bar_label labels the bars of a bar call the figure file names)
        if (call == "bar_label") setup += R"({"on": "ax", "call": "bar", "args": [[1, 2], [1, 2]], "as": "bars"}, )";
        // (indicate_inset_zoom shows an inset Axes the figure file names)
        if (call == "indicate_inset_zoom") setup += R"({"on": "ax", "call": "inset_axes", "args": [[0.5, 0.5, 0.4, 0.4]], "as": "ins"}, )";
        receiver = on == "figure" ? R"("on": "fig", )" : on == "axes" ? R"("on": "ax", )" : R"("on": "ax.xaxis", )";
    }
    return R"({"version": 1, "calls": [)" + setup + "{" + receiver + R"("call": ")" + call + R"(", "args": )" + args +
           R"(, "kwargs": )" + kwargs + "}]}";
}

// The probe's arguments with a date for every number: whether the call's data take dates. (The calls
// without numbers in their least arguments are given some.)
std::string withDates(const std::string& call) {
    static const std::map<std::string, std::string> given = {
        {"axhline", "[0]"}, {"axvline", "[0]"}, {"set_xlim", "[0, 1]"}, {"set_ylim", "[0, 1]"}};
    std::string args = "[]";
    if (const auto it = given.find(call); it != given.end()) args = it->second;
    else if (const auto least = leastArguments().find(call); least != leastArguments().end()) args = least->second;
    const auto parsed = support::parseJson(args, nullptr);
    if (!parsed) return {};
    bool any = false;
    std::function<void(const Json&, support::JsonWriter&)> copy = [&](const Json& v, support::JsonWriter& w) {
        if (v.isNumber()) {
            any = true;
            w.beginObject().key("$datetime").value("2020-01-01T00:00:00").endObject();
        } else if (v.isArray()) {
            w.beginArray();
            for (const Json& item : v.items()) copy(item, w);
            w.endArray();
        } else {
            write(w, v);
        }
    };
    support::JsonWriter w;
    copy(*parsed, w);
    return any ? w.text() : std::string();
}

// A value to try a property with: by its name for some, else by its kind
std::string sample(const Json& entry) {
    const std::string name = entry.find("name")->asString();
    static const std::map<std::string, std::string> byName = {
        {"shadow", "true"},           {"loc", "\"upper left\""},      {"numpoints", "2"},
        {"scatterpoints", "2"},       {"mode", "\"expand\""},          {"linespacing", "1.5"},
        {"figsize", "[4, 3]"},        {"rect", "[0, 0, 1, 1]"},          {"where", "[true, true]"},
        {"elinestyle", "\"dashed\""}, {"c", "\"C1\""},               {"labelcolor", "\"C1\""},
        {"edgecolor", "\"C1\""},    {"facecolor", "\"C1\""},         {"title", "\"a\""},
        {"hatch_linewidth", "2.0"},   {"num", "1"},                      {"width_ratios", "[1]"},
        {"height_ratios", "[1]"},     {"language", "\"en\""},          {"left", "0.25"},
        {"right", "0.75"},            {"bottom", "0.25"},                {"top", "0.75"},
        {"bins", "4"},                {"range", "[0, 3]"},               {"weights", "[1, 2, 1]"},
        {"label", "\"a\""},          {"xytext", "[1, 1]"},              {"textcoords", "\"offset points\""}};
    const Json* choices = entry.find("choices");
    if (choices && choices->size()) return "\"" + (*choices)[0].asString() + "\"";
    if (const auto it = byName.find(name); it != byName.end()) return it->second;
    const Json* kind = entry.find("kind");
    const std::string k = kind ? kind->asString() : "";
    if (k == "number") return "0.25";
    if (k == "integer") return "2";
    if (k == "bool") return "true";
    if (k == "colour") return "\"C1\"";
    if (k == "text") return "\"a\"";
    if (name == "marker") return "\"o\"";
    return {};
}

}  // namespace

std::string schema(const std::string& fontDirectory) {
    const auto doc = support::parseJson(embedded::file("data/schema.json"), nullptr);
    if (!doc) return {};
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    const auto outcome = [&](const std::string& figureFile) -> Json {
        std::string why;
        const auto figure = model::parseFigure(figureFile, &why);
        if (figure) why = engine::runCalls(*figure, options);
        if (why.empty()) return Json::boolean(true);
        if (why.find("unsupported keyword") != std::string::npos || why.find("not supported yet") != std::string::npos ||
            why.find("unsupported call") != std::string::npos)
            return Json::boolean(false);
        return Json();
    };
    support::JsonWriter w;
    w.beginObject().key("calls").beginArray();
    for (const Json& call : doc->find("calls")->items()) {
        const std::string on = call.find("on")->asString(), name = call.find("call")->asString();
        w.beginObject();
        const std::string dated = withDates(name);
        const Json takes = dated.empty() ? Json::boolean(false) : outcome(probe(on, name, "", "", dated));
        w.key("dates");
        write(w, Json::boolean(takes.isBool() && takes.asBool()));
        for (const auto& [k, v] : call.members()) {
            if (k != "parameters" && k != "properties") {
                w.key(k);
                write(w, v);
                continue;
            }
            w.key(k).beginArray();
            for (const Json& entry : v.items()) {
                const std::string keyword = entry.find("name")->asString();
                std::string value;
                if (k == "properties" || entry.find("kind")) value = sample(entry);
                if (!value.empty()) {
                } else if (const Json* d = entry.find("default"); d && !d->isNull() && !(d->isString() && d->asString().find('<') != std::string::npos)) {
                    support::JsonWriter one;
                    write(one, *d);
                    value = one.text();
                }
                Json supported;
                if (k == "parameters" && entry.find("required")) supported = outcome(probe(on, name, "", ""));
                else if (!value.empty()) supported = outcome(probe(on, name, keyword, value));
                w.beginObject();
                for (const auto& [ek, ev] : entry.members()) {
                    w.key(ek);
                    write(w, ev);
                }
                w.key("supported");
                write(w, supported);
                w.endObject();
            }
            w.endArray();
        }
        w.endObject();
    }
    w.endArray().endObject();
    return w.text();
}

void useFontBytes(std::function<std::string_view(std::string_view fileName)> bytes) { text::setFontBytes(std::move(bytes)); }

std::string drawPng(std::string_view figureFile, const std::string& fontDirectory, std::string* error) {
    std::string why;
    const auto figure = parsed(figureFile, &why);
    if (!figure) return *error = why, std::string();
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    const auto image = pixels(engine::drawAgg(*figure, options), fontDirectory, &why);
    if (!image) return *error = why, std::string();
    // (matplotlib: "cannot write empty image as PNG")
    if (image->width == 0 || image->height == 0) return *error = "the figure has no pixels: it cannot be written as a PNG", std::string();
    error->clear();
    return png::encodePng(*image, {{"Software", "calculate-plot " + std::string(versionString())}});
}

std::string drawSvg(std::string_view figureFile, const std::string& fontDirectory, std::string* error) {
    std::string why;
    const auto figure = parsed(figureFile, &why);
    if (!figure) return *error = why, std::string();
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    const auto drawing = engine::drawSvg(*figure, options);
    if (!drawing.error.empty()) return *error = drawing.error, std::string();
    auto result = render::replaySvgTrace(drawing.trace, fontDirectory);
    if (!result.error.empty()) return *error = result.error, std::string();
    error->clear();
    return std::move(result.svg);
}

std::string exportPython(std::string_view figureFile, std::string* error) {
    std::string why;
    const auto figure = parsed(figureFile, &why);
    if (!figure) return *error = why, std::string();
    error->clear();
    return model::exportPython(*figure, versionString());
}

Image drawScreen(std::string_view figureFile, double devicePixelRatio, const std::string& fontDirectory, std::string* error) {
    std::string why;
    const auto figure = parsed(figureFile, &why);
    if (!figure) return *error = why, Image{};
    if (!(devicePixelRatio > 0) || devicePixelRatio > 16) return *error = "the device pixel ratio must be above 0 and at most 16", Image{};
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    const auto image = pixels(engine::drawScreen(*figure, devicePixelRatio, options), fontDirectory, &why);
    if (!image) return *error = why, Image{};
    error->clear();
    return Image{image->width, image->height, std::vector<std::uint8_t>(image->rgba.begin(), image->rgba.end())};
}

}  // namespace calculate_plot
