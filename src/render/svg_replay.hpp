#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace calculate_plot::render {

struct SvgReplayResult {
    std::string svg;
    std::vector<std::string> hashedContents;  // what each element id hashed, in order
    std::string error;
};

// Writes the SVG of a trace recorded from matplotlib's SVG renderer (record.py's record_svg)
// with calculate-plot's port of that renderer. `fontDirectory` holds the fonts the trace names.
SvgReplayResult replaySvgTrace(std::string_view traceJson, const std::string& fontDirectory);

}  // namespace calculate_plot::render
