// Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (Fonts, TruetypeFonts, BakomaFonts,
// UnicodeFonts, DejaVuFonts, StixFonts, the font constants) and lib/matplotlib/_mathtext_data.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "text/font_manager.hpp"
#include "text/text_render.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::text::mathtext {

// --- tables (data/tex2uni.json, data/mathtext_data.json, data/unicode_letters.json) ---------------

// get_unicode_index: a single character, or a TeX name (its backslashes stripped) in tex2uni.
std::optional<unsigned> unicodeIndex(const std::u32string& sym);
bool isTexName(const std::string& name);  // a key of tex2uni
// A set of the Parser class by its name in the data file: "binary_operators", "left_delims", ...
const std::set<std::u32string>& symbolSet(const std::string& name);
const std::map<std::string, double>& spaceWidths();        // Parser._space_widths (keys as written: "\\,", "~")
const std::map<std::string, std::string>& accentMap();     // Parser._accent_map (hat -> \circumflexaccent)
// unicodedata.category(c)[0] == "L" and unicodedata.name(c).startswith("GREEK CAPITAL"), for the
// Basic Multilingual Plane; str.isdigit() of one character.
bool isLetter(unsigned c);
bool isGreekCapital(unsigned c);
bool isDigit(unsigned c);
std::string utf8(char32_t c);

// --- fonts ---------------------------------------------------------------------------------------

struct FontMetrics {
    double advance, height, width, xmin, xmax, ymin, ymax, iceberg;
    bool slanted;
};

struct FontInfo {
    FT2Font* font;
    std::string file;
    double fontsize;
    unsigned num;
    FT_UInt glyphIndex;
    FontMetrics metrics;
    double offset;
};

// FontConstantsBase and its subclasses
struct FontConstants {
    double scriptSpace = 0.05, supdrop = 0.4, subdrop = 0.4, sup1 = 0.7, sub1 = 0.3, sub2 = 0.5;
    double delta = 0.025, deltaSlanted = 0.2, deltaIntegral = 0.1;
    double num1 = 1.4, num2 = 1.5, num3 = 1.3, denom1 = 1.3, denom2 = 1.1;
    std::optional<double> axisHeight, quad, xHeight;
};

// TruetypeFonts: a set of fonts mathtext takes its glyphs from.
class Fonts {
public:
    // (font, num, slanted) of _get_glyph: the font by its file
    struct Glyph {
        std::string file;
        unsigned num = 0;
        bool slanted = false;
    };

    Fonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags);
    virtual ~Fonts() = default;

    // _get_info (cached): nullopt with *error where matplotlib raises (a symbol with no code point).
    std::optional<FontInfo> info(const std::string& fontname, const std::string& fontClass, const std::u32string& sym,
                                 double fontsize, double dpi, std::string* error);
    std::optional<double> axisHeight(const std::string& fontname, double fontsize, double dpi, std::string* error);
    std::optional<double> quad(const std::string& fontname, double fontsize, double dpi, std::string* error);
    std::optional<double> xheight(const std::string& fontname, double fontsize, double dpi, std::string* error);
    double underlineThickness(double fontsize, double dpi) const { return ((0.75 / 12) * fontsize * dpi) / 72; }
    std::optional<double> kern(const std::string& font1, const std::string& class1, const std::u32string& sym1, double size1,
                               const std::string& font2, const std::string& class2, const std::u32string& sym2,
                               double size2, double dpi, std::string* error);
    // get_sized_alternatives_for_symbol: (font name, symbol) from the smallest to the largest
    virtual std::vector<std::pair<std::string, std::u32string>> sizedAlternatives(const std::string& fontname,
                                                                                 const std::u32string& sym);
    virtual const FontConstants& constants() const;
    // _get_glyph: false with *error for a symbol with no code point, and once the fontset has
    // failed: a font file could not be opened, or a font was asked for with a pattern that
    // FontProperties refuses (failure() then says which).
    bool glyph(const std::string& fontname, const std::string& fontClass, const std::u32string& sym, Glyph* out,
               std::string* error);
    const std::string& failure() const { return failure_; }
    virtual bool isStix() const { return false; }
    // _get_font: the file of a font name through the font map; empty when there is none
    std::string fontFile(const std::string& font) const;
    bool hasGlyph(const std::string& file, unsigned num);

protected:
    virtual bool lookup(const std::string& fontname, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                        std::string* error) = 0;
    // The glyph as another fontset has it (a fallback); its failure to open a font becomes ours.
    bool lookupIn(Fonts& other, const std::string& fontname, const std::string& fontClass, const std::u32string& sym,
                  Glyph* out, std::string* error);
    FT2Font* open(const std::string& file);  // nullptr, and failure() set, if the file cannot be opened
    std::string findPattern(const std::string& pattern);               // findfont(pattern): "" and failure() set if it cannot be
    void mapFont(const std::string& key, const std::string& pattern);  // fontmap[key] = fontmap[name] = findfont(name)
    FontCache& cache_;
    const rc::Params& rc_;
    FT_Int32 flags_;
    FontProperties defaultProp_;
    std::string defaultFile_;
    std::map<std::string, std::string> fontmap_;

private:
    std::map<std::tuple<std::string, std::string, std::u32string, double, double>, FontInfo> infos_;
    std::string failure_;
};

// MathTextParser._font_type_mapping: 'cm', 'dejavuserif', 'dejavusans', 'stix', 'stixsans', 'custom'
// (the names rcParams["mathtext.fontset"] takes).
std::unique_ptr<Fonts> makeFontset(const std::string& name, FontCache& cache, const FontProperties& defaultProp,
                                   const rc::Params& rc, FT_Int32 flags);

}  // namespace calculate_plot::text::mathtext
