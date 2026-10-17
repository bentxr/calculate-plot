// Derived from Matplotlib 3.11.2: lib/matplotlib/_constrained_layout.py, lib/matplotlib/_layoutgrid.py and
// lib/matplotlib/layout_engine.py (ConstrainedLayoutEngine.execute).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/renderer.hpp"

#include <array>
#include <string>

namespace calculate_plot::engine {

class Figure;

// ConstrainedLayoutEngine's parameters: the pads in inches, the spaces as fractions, the rectangle
// (left, bottom, width, height) in figure coordinates, and whether the layout is 'compressed'
struct ConstrainedLayoutParams {
    double hPad = 0, wPad = 0, hspace = 0, wspace = 0;
    std::array<double, 4> rect{0, 0, 1, 1};
    bool compress = false;
};

// ConstrainedLayoutEngine.execute(fig): do_constrained_layout with the pads relative to the figure's size,
// measuring with `renderer`. False and why where matplotlib raises an error Figure.draw does not catch
// (kiwisolver's); a ValueError (which Figure.draw ignores) stops the layout where it is raised.
bool executeConstrainedLayout(Figure& figure, Renderer& renderer, const ConstrainedLayoutParams& params, std::string* error);

}  // namespace calculate_plot::engine
