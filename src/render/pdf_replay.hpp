#pragma once

#include <string>
#include <string_view>

namespace calculate_plot::render {

struct PdfReplayResult {
    std::string pdf;  // the file's bytes
    std::string error;
};

// Writes the PDF of a trace recorded from matplotlib's PDF renderer (record.py's record_pdf) with
// calculate-plot's port of that renderer. `fontDirectory` holds the fonts the trace names.
PdfReplayResult replayPdfTrace(std::string_view traceJson, const std::string& fontDirectory);

}  // namespace calculate_plot::render
