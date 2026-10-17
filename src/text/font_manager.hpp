// Derived from Matplotlib 3.11.2: lib/matplotlib/font_manager.py (FontProperties' defaults and
// setters, the font scoring and FontManager._find_fonts_by_props / _findfont_cached).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "support/json.hpp"

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
    std::string mathFontfamily = "dejavusans";  // the fontset of the text's mathtext (rcParams["mathtext.fontset"])

    // FontProperties() with every property from the rcParams (font.family, font.style, ...).
    static FontProperties defaults(const rc::Params& rc);
};

// FontProperties(pattern): the properties a fontconfig pattern asks for, over the rcParams' defaults
// (set_fontconfig_pattern: the first value of each property, through its setter). False with *error
// for what is not a pattern, for a property FontProperties does not have and for a value its setter
// refuses. *file is the pattern's file=, which names a font file outright.
bool fontPropertiesFromPattern(const std::string& pattern, const rc::Params& rc, FontProperties* out,
                               std::optional<std::string>* file, std::string* error);

// One entry of the font manager's list (FontEntry), for a bundled font file.
struct FontEntry {
    std::string file;  // the file name in fonts/ttf
    std::string name, style, variant;
    FontValue weight, stretch;
    std::string size;  // "scalable" for every bundled font
};

// matplotlib's bundled fonts as its font manager lists them (data/fonts.json).
const std::vector<FontEntry>& bundledFonts();

// Whether a name is one of weight_dict (ultralight ... black) or of stretch_dict (ultra-condensed ...).
bool isFontWeightName(const std::string& name);
bool isFontStretchName(const std::string& name);

// A named font size (xx-small ... xx-large, larger, smaller) in points: font_scalings times
// rcParams["font.size"]; nullopt for an unknown name.
std::optional<double> fontSize(const std::string& name, const rc::Params& rc);

// FontProperties.set_size for a value of the rcParams: a number, the name of a size, or None for
// rcParams["font.size"]. FreeType does not take a size below one point: it is one point then.
double fontSizeOf(const support::Json& value, const rc::Params& rc);
// The same for a size in points.
inline double atLeastOnePoint(double size) { return size < 1.0 ? 1.0 : size; }

// FontManager._find_fonts_by_props: for each family in turn, the best-scoring font (families with
// no font scoring under 10 are skipped); if none was found, the default family's. `fonts` replaces
// the bundled list (tests).
// FontManager.findfont: the best-scoring font over all of the properties' families, falling back to
// the default family.
std::string findFont(const FontProperties& prop, const rc::Params& rc, const std::vector<FontEntry>* fonts = nullptr);

std::vector<std::string> findFontsByProps(const FontProperties& prop, const rc::Params& rc,
                                          const std::vector<FontEntry>* fonts = nullptr);

}  // namespace calculate_plot::text
