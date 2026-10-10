#pragma once

#include "png/png.hpp"

#include <string>
#include <string_view>

namespace calculate_plot::render {

struct ReplayResult {
    png::Image image;   // straight RGBA, as matplotlib's Agg buffer
    std::string error;  // empty when the trace was drawn completely
};

// Draws a trace recorded from matplotlib's Agg renderer (tools/oracle/record.py, version 1)
// with calculate-plot's port of that renderer.
ReplayResult replayTrace(std::string_view traceJson);

}  // namespace calculate_plot::render

namespace calculate_plot::support { class Json; }

namespace calculate_plot::render {

// Writes a parsed JSON value back as JSON text (numbers keep their original spelling).
std::string toJson(const support::Json& value);

}  // namespace calculate_plot::render
