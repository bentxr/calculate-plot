// Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (the box model, the parser's
// handlers, ship) and lib/matplotlib/mathtext.py (MathTextParser).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "text/font_manager.hpp"
#include "text/text_render.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::text {

// One glyph of a laid-out expression: the font, its size in points, the character and glyph,
// and where it goes (x right, y down from the top of the box, offset included).
struct MathGlyph {
    FT2Font* font;
    std::string file;  // the font's file name
    double fontsize;
    unsigned num;  // the code point
    FT_UInt index;
    double ox, oy;
};

// MathTextParser('path').parse: the box's size (width, height + depth, depth; each rounded up),
// the glyphs and the filled rectangles (x, y, width, height; y down from the top).
struct MathParse {
    double width = 0, height = 0, depth = 0;
    std::vector<MathGlyph> glyphs;
    std::vector<std::array<double, 4>> rects;
};

// Lays out a string with mathtext ($...$ parts), as matplotlib's vector output does: glyphs loaded
// unhinted. The fontset is the font properties' math font family ('dejavusans', 'dejavuserif', 'cm',
// 'stix', 'stixsans' or 'custom'; rcParams["mathtext.fontset"] unless the text asks for another). What matplotlib refuses gives nullopt with *error; the messages of the
// grammar are ours, those of the handlers ("Double superscript", "Unknown symbol: ...") matplotlib's.
std::optional<MathParse> parseMath(FontCache& fonts, const std::string& s, double dpi, const FontProperties& prop,
                                   const rc::Params& rc, std::string* error);

}  // namespace calculate_plot::text
