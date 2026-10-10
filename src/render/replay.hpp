#pragma once

#include "png/png.hpp"

#include <string>
#include <string_view>

namespace calculate_plot::render {

struct ReplayResult {
    png::Image image;   // straight RGBA, as matplotlib's Agg buffer
    std::string error;  // empty when the trace was drawn completely
};

// How the replay draws text.
enum class TextMode {
    Bitmaps,  // the glyph bitmaps recorded from matplotlib (tests the renderer alone)
    Glyphs,   // calculate-plot renders the recorded glyph lists with FreeType (tests rasterizing)
    Text,     // calculate-plot also lays out each plain string (tests layout); mathtext as Glyphs
};

struct ReplayOptions {
    TextMode text = TextMode::Bitmaps;
    std::string fontDirectory;  // where the fonts named in the trace are (needed unless Bitmaps)
};

// Draws a trace recorded from matplotlib's Agg renderer (tools/oracle/record.py, version 1)
// with calculate-plot's port of that renderer.
ReplayResult replayTrace(std::string_view traceJson, const ReplayOptions& options = {});

}  // namespace calculate_plot::render

namespace calculate_plot::support { class Json; }

namespace calculate_plot::render {

// Writes a parsed JSON value back as JSON text (numbers keep their original spelling).
std::string toJson(const support::Json& value);

}  // namespace calculate_plot::render
