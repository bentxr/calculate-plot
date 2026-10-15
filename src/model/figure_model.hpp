#pragma once

#include "support/json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculate_plot::model {

// A figure as the ordered list of matplotlib calls that draws it (the figure file, JSON):
//   {"version": 1, "style": "ggplot" | [names...], "rc": {...rcParams...},
//    "calls": [{"on": "ax", "call": "plot", "args": [...], "kwargs": {...}, "as": "line"}, ...]}
// A call without "on" is a pyplot function (plt.figure, plt.subplots). "as" names the result (a list
// of names unpacks it). Values are JSON values; {"$tuple": [...]} is a Python tuple and
// {"$float": "nan" | "inf" | "-inf"} a float JSON cannot write.
struct Call {
    std::string on;      // the object called ("fig", "ax", "axs[1]"), or empty for pyplot
    std::string method;
    std::vector<support::Json> args;
    std::vector<std::pair<std::string, support::Json>> kwargs;
    std::vector<std::string> as;
};

struct Figure {
    support::Json style;  // plt.style.use's argument, before the rcParams: a name or a list of names; null: none
    std::vector<std::pair<std::string, support::Json>> rc;
    std::vector<Call> calls;
};

std::optional<Figure> parseFigure(std::string_view json, std::string* error);

// A plain Python script that draws the same figure with matplotlib.
std::string exportPython(const Figure& figure, std::string_view version);

}  // namespace calculate_plot::model
