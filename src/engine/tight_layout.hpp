// Derived from Matplotlib 3.11.2: lib/matplotlib/_tight_layout.py (_auto_adjust_subplotpars,
// get_subplotspec_list, get_tight_layout_figure), lib/matplotlib/layout_engine.py
// (TightLayoutEngine.execute) and lib/matplotlib/figure.py (Figure.tight_layout, subplots_adjust).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/renderer.hpp"

#include <array>
#include <optional>
#include <string>

namespace calculate_plot::engine {

class Figure;

// The keywords of Figure.subplots_adjust; unset values stay as they are.
struct SubplotsAdjustment {
    std::optional<double> left, bottom, right, top, wspace, hspace;
};

// Figure.subplots_adjust: the figure's subplot parameters change, then every Axes of a grid moves to
// its cell. False with *error where matplotlib raises.
bool subplotsAdjust(Figure& figure, const SubplotsAdjustment& adjustment, std::string* error);

struct TightLayoutOptions {
    double pad = 1.08;  // in units of the font size
    std::optional<double> hPad, wPad;
    std::optional<std::array<double, 4>> rect;  // (left, bottom, right, top) in figure coordinates
};

// get_tight_layout_figure: the subplot parameters that make room for the Axes' decorations, measured
// with `renderer`; nullopt where matplotlib leaves the layout alone (it warns).
std::optional<SubplotsAdjustment> tightLayoutParameters(Figure& figure, Renderer& renderer,
                                                        const TightLayoutOptions& options);

// Figure.tight_layout: measured with the figure's Agg renderer, applied once. False with *error
// where the measuring met something unsupported or matplotlib raises.
bool tightLayout(Figure& figure, const TightLayoutOptions& options, const std::string& locale,
                 const std::string& fontDirectory, std::string* error);

}  // namespace calculate_plot::engine
