// Derived from Matplotlib 3.11.2: lib/matplotlib/textpath.py (TextToPath.get_text_path, get_glyphs_mathtext,
// TextPath).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"
#include "rc/rc_params.hpp"

#include <string>

namespace calculate_plot::text {
class FontCache;
}

namespace calculate_plot::engine {

// TextPath(xy=(0, 0), s, size=None, prop=None) of a mathtext string ($...$): the glyphs' outlines (unhinted, at
// TextToPath.FONT_SCALE and 72 dpi, each scaled to its size and moved to its place) and the filled rectangles of the
// layout, as one path, then scaled by size / FONT_SCALE (size: FontProperties()'s, rcParams["font.size"]). It is
// not simplified. False with *error where the mathtext parser raises.
bool mathtextPath(text::FontCache& fonts, const std::string& s, const rc::Params& rc, Path* out, std::string* error);

// Path.get_extents(): (x0, y0, x1, y1) of the vertices that are drawn (the MOVETO and LINETO ones of a path of
// straight segments; the ends and extrema of each Bezier segment of a path with curves); Bbox.null() when empty
std::array<double, 4> pathExtents(const Path& path);

}  // namespace calculate_plot::engine
