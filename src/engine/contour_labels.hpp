// Derived from Matplotlib 3.11.2: lib/matplotlib/contour.py (ContourLabeler) and lib/matplotlib/path.py
// (Path._iter_connected_components, Path.make_compound_path).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colormap.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;
class ContourSet;
class Renderer;

// ContourSet.clabel's keywords (manual placement and use_clabeltext are not taken)
struct ClabelOptions {
    std::optional<std::vector<double>> levels;  // the levels to label (a subset of the set's), all for None
    std::string levelsText;                     // repr(list(levels)), for matplotlib's message when they do not match
    std::optional<double> fontsize;             // points; None: rcParams["font.size"]
    bool inlineLabels = true;                   // inline
    double inlineSpacing = 5;
    std::optional<std::string> fmt;             // a %-format; None: ScalarFormatter(useOffset=False)
    std::optional<std::vector<colors::Rgba>> colors;
    bool rightsideUp = true;
    std::optional<double> zorder;               // None: 2 + the set's zorder
};

// ContourSet.clabel(levels, ...) with automatic placement (labels()): the labels added to `ax` (the set's Axes) as
// Texts in data coordinates, rotated along the contour, the contour broken under them when inline. `renderer` is
// the figure's (fig._get_renderer()), which measures the labels. False and matplotlib's message when it raises, or
// a "not supported yet" message.
bool clabel(ContourSet& cs, Axes& ax, Renderer& renderer, const ClabelOptions& options, std::string* error);

}  // namespace calculate_plot::engine
