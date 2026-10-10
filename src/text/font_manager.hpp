// Derived from Matplotlib 3.11.2: lib/matplotlib/font_manager.py (FontProperties' defaults, the
// font scoring and FontManager._find_fonts_by_props / _findfont_cached).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::text {

// A weight or stretch: a CSS number, or a name (weight_dict, stretch_dict).
using FontValue = std::variant<int, std::string>;

// FontProperties: what a text asks of its font.
struct FontProperties {
    std::vector<std::string> family;  // names or generic families (sans-serif, serif, ...)
    std::string style = "normal";     // normal, italic, oblique
    std::string variant = "normal";   // normal, small-caps
    FontValue weight = std::string("normal");
    FontValue stretch = std::string("normal");
    double size = 10.0;  // points

    // FontProperties() with every property from the rcParams (font.family, font.style, ...).
    static FontProperties defaults(const rc::Params& rc);
};

// One entry of the font manager's list (FontEntry), for a bundled font file.
struct FontEntry {
    std::string file;  // the file name in fonts/ttf
    std::string name, style, variant;
    FontValue weight, stretch;
    std::string size;  // "scalable" for every bundled font
};

// matplotlib's bundled fonts as its font manager lists them (data/fonts.json).
const std::vector<FontEntry>& bundledFonts();

// A named font size (xx-small ... xx-large, larger, smaller) in points: font_scalings times
// rcParams["font.size"]; nullopt for an unknown name.
std::optional<double> fontSize(const std::string& name, const rc::Params& rc);

// FontManager._find_fonts_by_props: for each family in turn, the best-scoring font (families with
// no font scoring under 10 are skipped); if none was found, the default family's. `fonts` replaces
// the bundled list (tests).
std::vector<std::string> findFontsByProps(const FontProperties& prop, const rc::Params& rc,
                                          const std::vector<FontEntry>* fonts = nullptr);

}  // namespace calculate_plot::text
