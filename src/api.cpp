// The public drawing API (include/calculate-plot/calculate-plot.hpp) over the engine and the renderers.
#include "calculate-plot/calculate-plot.hpp"

#include "engine/engine.hpp"
#include "model/figure_model.hpp"
#include "png/png.hpp"
#include "render/replay.hpp"
#include "render/svg_replay.hpp"
#include "text/text_render.h"

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

}  // namespace

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
