#pragma once

#include "model/figure_model.hpp"
#include "text/text_render.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::engine {

struct AggDrawing {
    std::string trace;  // the draw calls, in the trace format of tools/oracle/record.py
    std::string error;  // empty when every call of the figure file was understood
};

struct DrawOptions {
    std::string fontDirectory;                    // where matplotlib's bundled fonts are
    std::string locale = text::kReferenceLocale;  // the shaping language of texts that set none
};

// Runs a figure file's calls and draws the figure as matplotlib's FigureCanvasAgg.draw would.
AggDrawing drawAgg(const model::Figure& figure, const DrawOptions& options);

// Runs a figure file's calls and draws the figure on a screen of `ratio` device pixels per logical
// pixel, as matplotlib's Qt canvas does (drawScreenTrace).
AggDrawing drawScreen(const model::Figure& figure, double ratio, const DrawOptions& options);

// Runs a figure file's calls without drawing the figure: what they refuse, or "".
std::string runCalls(const model::Figure& figure, const DrawOptions& options);

struct SvgDrawing {
    std::string trace;  // the draw calls, in the trace format of record.py's record_svg
    std::string error;
};

// The SVG metadata calculate-plot writes unless told otherwise: no date, so that the same figure
// gives the same bytes.
inline const std::vector<std::pair<std::string, std::optional<std::string>>> kSvgMetadata = {
    {"Creator", "calculate-plot"}, {"Date", std::nullopt}};
// The hash salt of the SVG ids when rcParams["svg.hashsalt"] is None (matplotlib then uses a
// random one, so its SVG bytes change from run to run).
inline const char* const kSvgHashSalt = "calculate-plot";

// Runs a figure file's calls and draws the figure as matplotlib's FigureCanvasSVG.print_svg would.
SvgDrawing drawSvg(const model::Figure& figure, const DrawOptions& options);

}  // namespace calculate_plot::engine
