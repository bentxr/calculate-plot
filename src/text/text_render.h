// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_agg.py,
// lib/matplotlib/font_manager.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "text/ft2font.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GCAgg;
class RendererAgg;

namespace calculate_plot::text {

// Python's round() and NumPy's round() to an integer: halves go to the even neighbour.
long long pythonRound(double v);

// rcParams["text.hinting"] as FreeType load flags (backend_agg.get_hinting_flag).
std::optional<FT_Int32> hintingFlags(const std::string& hinting);

// Fonts by file name, opened once with their fallback chain, as font_manager._get_font builds it:
// the given fonts in order, then the Last Resort font (which never warns when it is used).
class FontCache {
public:
    explicit FontCache(std::string directory);
    ~FontCache();
    // nullptr if a font cannot be opened (`error` says why).
    FT2Font* get(const std::vector<std::string>& names, bool enableLastResort = true);
    FT2Font* byPath(const std::string& name);  // a font already opened under this file name
    std::string nameOf(const FT2Font* font) const;  // the file name a font was opened from
    std::string error;

private:
    struct Entry {
        std::vector<std::unique_ptr<FT2Font>> fallbacks;
        std::vector<FT2Font*> fallbackPointers;
        std::unique_ptr<FT2Font> font;
    };
    std::string directory_;
    std::map<std::vector<std::string>, std::unique_ptr<Entry>> fonts_;
};

// One glyph to draw: the font object (main or fallback), its size in points, the glyph, a slant and
// horizontal stretch, and its offset in pixels from the text's origin (dy upwards).
struct Glyph {
    FT2Font* font;
    double size;
    FT_UInt index;
    double slant;
    double extend;
    double dx;
    double dy;
};

// A filled rectangle of mathtext (fraction bars, roots): dx, dy (upwards), width, height in pixels.
struct Box {
    double dx, dy, w, h;
};

// The language matplotlib shapes with when a text sets none: HarfBuzz's default, which comes from
// the process locale (LC_CTYPE) and changes shaping (ligatures, for one). The references use C.UTF-8,
// which shapes like en_US.UTF-8, the locale of matplotlib's own image tests. calculate-plot always
// passes a language explicitly, so its output never depends on the user's locale.
inline const char* const kReferenceLocale = "C.UTF-8";

// RendererAgg.draw_text for plain text: lays `text` (UTF-8) out with the fonts and returns its glyphs.
// `language` is the text's own (Text.set_language); without one, `defaultLanguage` is used.
std::vector<Glyph> layoutText(FontCache& fonts, const std::vector<std::string>& names, const std::string& text,
                              double size, double dpi, FT_Int32 flags,
                              const std::optional<std::vector<std::string>>& features,
                              const std::optional<std::string>& language, std::string& error,
                              const std::string& defaultLanguage = kReferenceLocale);

// RendererAgg.get_text_width_height_descent for plain text, in pixels.
struct TextExtent {
    double width = 0, height = 0, descent = 0;
};
TextExtent textExtent(FontCache& fonts, const std::vector<std::string>& names, const std::string& text, double size,
                      double dpi, FT_Int32 flags, const std::optional<std::string>& language, std::string& error,
                      const std::string& defaultLanguage = kReferenceLocale);

// A glyph bitmap as _draw_text_glyphs_and_boxes hands it to draw_text_image: x is its left side,
// y its bottom side (downwards from the canvas' top).
struct GlyphImage {
    unsigned rows = 0, width = 0;
    std::vector<unsigned char> buffer;  // rows * width coverage bytes
    int x = 0, y = 0;
};

// The glyph half of _draw_text_glyphs_and_boxes: each glyph rendered with FreeType at its place, on
// a canvas `height` pixels high. x, y in pixels from the top-left, angle in degrees.
std::vector<GlyphImage> renderGlyphs(double height, double dpi, double x, double y, double angle,
                                     const std::vector<Glyph>& glyphs, bool antialiased, FT_Int32 flags,
                                     std::string& error);

// RendererAgg._draw_text_glyphs_and_boxes: renders each glyph with FreeType at its place and
// composites it, then fills the boxes. x, y in pixels from the top-left, angle in degrees.
std::string drawGlyphsAndBoxes(RendererAgg& renderer, GCAgg& gc, double x, double y, double angle,
                               const std::vector<Glyph>& glyphs, const std::vector<Box>& boxes, FT_Int32 flags);

// UTF-8 to code points (invalid bytes become U+FFFD).
std::u32string codepoints(const std::string& utf8);

}  // namespace calculate_plot::text
