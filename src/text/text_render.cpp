// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_agg.py,
// lib/matplotlib/font_manager.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/text_render.h"

#include "agg/backend_agg.h"

#include <freetype/tttables.h>

#include <cmath>

namespace calculate_plot::text {

namespace {

constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;  // as math.radians

}  // namespace

long long pythonRound(double v) { return static_cast<long long>(std::nearbyint(v)); }

std::optional<FT_Int32> hintingFlags(const std::string& hinting) {
    if (hinting == "default" || hinting == "either") return FT_LOAD_DEFAULT;
    if (hinting == "no_autohint" || hinting == "native") return FT_LOAD_NO_AUTOHINT;
    if (hinting == "force_autohint" || hinting == "auto" || hinting == "true") return FT_LOAD_FORCE_AUTOHINT;
    if (hinting == "no_hinting" || hinting == "none" || hinting == "false") return FT_LOAD_NO_HINTING;
    return std::nullopt;
}

FontCache::FontCache(std::string directory) : directory_(std::move(directory)) {}
FontCache::~FontCache() = default;

FT2Font* FontCache::get(const std::vector<std::string>& names, bool enableLastResort) {
    if (names.empty()) {
        error = "no font given";
        return nullptr;
    }
    auto& slot = fonts_[names];
    if (slot) return slot->font.get();
    auto entry = std::make_unique<Entry>();
    auto open = [&](const std::string& name, bool warnIfUsed) -> std::unique_ptr<FT2Font> {
        std::vector<FT2Font*> none;
        auto font = std::make_unique<FT2Font>(none, warnIfUsed);
        if (!font->open(directory_ + "/" + name, 0)) {
            error = name + ": " + font->error;
            return nullptr;
        }
        return font;
    };
    const std::string lastResort = "LastResortHE-Regular.ttf";
    for (std::size_t i = 1; i < names.size(); ++i) {
        auto font = open(names[i], false);
        if (!font) return nullptr;
        entry->fallbacks.push_back(std::move(font));
    }
    long lastResortIndex = -1;
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == lastResort) lastResortIndex = long(i);
    if (lastResortIndex < 0 && enableLastResort) {
        // Add Last Resort font so we always have glyphs regardless of font, unless we're already in the list.
        auto font = open(lastResort, true);
        if (!font) return nullptr;
        entry->fallbacks.push_back(std::move(font));
        lastResortIndex = long(entry->fallbacks.size());
    }
    for (auto& f : entry->fallbacks) entry->fallbackPointers.push_back(f.get());
    entry->font = std::make_unique<FT2Font>(entry->fallbackPointers, false);
    if (!entry->font->open(directory_ + "/" + names[0], 0)) {
        error = names[0] + ": " + entry->font->error;
        return nullptr;
    }
    // Ensure we are using the right charmap for the Last Resort font; FreeType picks the Unicode one
    // by default, but this exists only for Windows, and is empty.
    if (lastResortIndex == 0) {
        entry->font->set_charmap(0);
    } else if (lastResortIndex > 0) {
        entry->fallbacks[std::size_t(lastResortIndex - 1)]->set_charmap(0);
    }
    slot = std::move(entry);
    return slot->font.get();
}

FT2Font* FontCache::byPath(const std::string& name) {
    const std::string path = directory_ + "/" + name;
    for (auto& [names, entry] : fonts_) {
        if (entry->font && entry->font->path() == path) return entry->font.get();
        for (auto& f : entry->fallbacks)
            if (f->path() == path) return f.get();
    }
    return get({name}, true);
}

std::optional<FontHeightMetrics> fontHeightMetrics(FontCache& fonts, const std::vector<std::string>& names,
                                                   double size, double dpi, std::string& error) {
    FT2Font* font = fonts.get(names);
    if (!font) {
        error = fonts.error;
        return std::nullopt;
    }
    const FT_Face face = font->get_face();
    // scale = 1 / units_per_em * fontsize * dpi / 72, in Python's order of operations
    const double scale = 1.0 / double(face->units_per_EM) * size * dpi / 72;
    if (const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2)))
        return FontHeightMetrics{os2->sTypoLineGap * scale, os2->sTypoAscender * scale,
                                 -double(os2->sTypoDescender) * scale};
    if (const auto* hhea = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA)))
        return FontHeightMetrics{hhea->Line_Gap * scale, hhea->Ascender * scale, -double(hhea->Descender) * scale};
    return std::nullopt;
}

std::string FontCache::nameOf(const FT2Font* font) const {
    const std::string& path = font->path();
    const std::string prefix = directory_ + "/";
    return path.compare(0, prefix.size(), prefix) == 0 ? path.substr(prefix.size()) : path;
}

std::u32string codepoints(const std::string& utf8) {
    std::u32string out;
    for (std::size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        const int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        // A bad lead byte, or a sequence cut by the end of the text.
        if (extra < 0 || std::size_t(extra) >= utf8.size() - i) {
            out.push_back(0xfffd);
            ++i;
            continue;
        }
        char32_t cp = extra == 0 ? c : (c & (0x3f >> extra));
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            const auto b = static_cast<unsigned char>(utf8[i + std::size_t(k)]);
            if ((b & 0xc0) != 0x80) ok = false;
            cp = (cp << 6) | (b & 0x3f);
        }
        out.push_back(ok ? cp : char32_t(0xfffd));
        i += ok ? std::size_t(extra) + 1 : 1;
    }
    return out;
}

std::vector<Glyph> layoutText(FontCache& fonts, const std::vector<std::string>& names, const std::string& text,
                              double size, double dpi, FT_Int32 flags,
                              const std::optional<std::vector<std::string>>& features,
                              const std::optional<std::string>& language, std::string& error,
                              const std::string& defaultLanguage) {
    // _prepare_font: the font, cleared and set to the size.
    FT2Font* font = fonts.get(names);
    if (!font) {
        error = fonts.error;
        return {};
    }
    font->clear();
    font->set_size(size, dpi);
    const std::u32string cps = codepoints(text);
    // Never HarfBuzz's process-wide default (the user's locale): always an explicit language. An
    // empty one (to C: one that starts with a NUL) is none to HarfBuzz, which then takes its default.
    const bool given = language && language->c_str()[0] != '\0';
    const FT2Font::LanguageType languages =
        std::vector<FT2Font::LanguageRange>{{given ? *language : defaultLanguage, 0, int(cps.size())}};
    const auto items = font->layout_items(cps, flags, features, languages);
    if (!font->error.empty()) {
        error = font->error;
        return {};
    }
    std::vector<Glyph> glyphs;
    for (const auto& item : items)
        glyphs.push_back(Glyph{item.ft_object, size, FT_UInt(item.glyph_index), 0.0, 1.0, item.x, item.y});
    return glyphs;
}

TextExtent textExtent(FontCache& fonts, const std::vector<std::string>& names, const std::string& text, double size,
                      double dpi, FT_Int32 flags, std::string& error, const std::string& defaultLanguage) {
    FT2Font* font = fonts.get(names);
    if (!font) {
        error = fonts.error;
        return {};
    }
    font->clear();
    font->set_size(size, dpi);
    const std::u32string cps = codepoints(text);
    const FT2Font::LanguageType languages = std::vector<FT2Font::LanguageRange>{{defaultLanguage, 0, int(cps.size())}};
    std::vector<double> xys;
    font->set_text(cps, 0.0, flags, std::nullopt, languages, xys);
    if (!font->error.empty()) {
        error = font->error;
        return {};
    }
    const auto [w, h] = font->get_width_height();  // of the unrotated string
    const long d = font->get_descent();
    return TextExtent{double(w) / 64.0, double(h) / 64.0, double(d) / 64.0};  // convert from subpixels
}

std::vector<GlyphImage> renderGlyphs(double height, double dpi, double x, double y, double angle,
                                     const std::vector<Glyph>& glyphs, bool antialiased, FT_Int32 flags,
                                     std::string& error) {
    // y is downwards.
    std::vector<GlyphImage> images;
    const double cos = std::cos(angle * kDegreesToRadians);
    const double sin = std::sin(angle * kDegreesToRadians);
    for (const Glyph& g : glyphs) {  // dy is upwards.
        g.font->set_size(g.size, dpi);
        // (0x10000 * [[cos, -sin], [sin, cos]]) @ [[extend, extend * slant], [0, 1]], rounded.
        const double a00 = 0x10000 * cos, a01 = 0x10000 * -sin, a10 = 0x10000 * sin, a11 = 0x10000 * cos;
        const double b00 = g.extend, b01 = g.extend * g.slant, b10 = 0.0, b11 = 1.0;
        const std::array<std::array<FT_Fixed, 2>, 2> matrix{{
            {{FT_Fixed(pythonRound(a00 * b00 + a01 * b10)), FT_Fixed(pythonRound(a00 * b01 + a01 * b11))}},
            {{FT_Fixed(pythonRound(a10 * b00 + a11 * b10)), FT_Fixed(pythonRound(a10 * b01 + a11 * b11))}}}};
        const std::array<FT_Fixed, 2> delta{
            FT_Fixed(pythonRound(0x40 * (x + g.dx * cos - g.dy * sin))),
            // FreeType's y is upwards.
            FT_Fixed(pythonRound(0x40 * (height - y + g.dx * sin + g.dy * cos)))};
        g.font->_set_transform(matrix, delta);
        PositionedBitmap bitmap =
            g.font->render_glyph(g.index, flags, antialiased ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
        if (!g.font->error.empty()) {
            error = g.font->error;
            return images;
        }
        if (!antialiased)
            for (auto& v : bitmap.buffer) v = static_cast<unsigned char>(v * 0xff);
        // draw_text_image's y is downwards & the bitmap bottom side.
        images.push_back(GlyphImage{bitmap.rows, bitmap.width, std::move(bitmap.buffer), int(bitmap.left),
                                    int(height) - int(bitmap.top) + int(bitmap.rows)});
    }
    return images;
}

std::string drawGlyphsAndBoxes(RendererAgg& renderer, GCAgg& gc, double x, double y, double angle,
                               const std::vector<Glyph>& glyphs, const std::vector<Box>& boxes, FT_Int32 flags) {
    const double height = renderer.get_height();
    std::string error;
    auto images = renderGlyphs(height, renderer.dpi, x, y, angle, glyphs, gc.isaa, flags, error);
    if (!error.empty()) return error;
    for (auto& g : images) {
        mpl::NdArray<agg::int8u, 2> image({g.rows, g.width}, g.buffer);
        renderer.draw_text_image(gc, image, g.x, g.y, 0.0);
    }

    if (boxes.empty()) return {};
    agg::rgba rgba = gc.color;
    // A 3-tuple colour or a forced alpha takes the context's alpha (the recorded colour is always RGBA).
    if (gc.forced_alpha) rgba.a = gc.alpha;
    GCAgg gc1;
    gc1.linewidth = 0.0;
    gc1.alpha = 1.0;
    gc1.forced_alpha = false;
    gc1.color = agg::rgba(0.0, 0.0, 0.0, 1.0);
    gc1.isaa = true;
    gc1.cap = agg::butt_cap;
    gc1.join = agg::round_join;
    gc1.dashes.set_dash_offset(0.0);
    gc1.cliprect = agg::rect_d(0.0, 0.0, 0.0, 0.0);
    gc1.snap_mode = gc.snap_mode;
    gc1.hatch_color = agg::rgba(0.0, 0.0, 0.0, 1.0);
    gc1.hatch_linewidth = 1.0;
    gc1.sketch = SketchParams{0.0, 0.0, 0.0};
    for (Box b : boxes) {  // dy is upwards.
        if (gc1.snap_mode == SNAP_AUTO || gc1.snap_mode == SNAP_TRUE) {
            // Prevent thin bars from disappearing by growing symmetrically.
            if (b.w < 1) {
                b.dx -= (1 - b.w) / 2;
                b.w = 1;
            }
            if (b.h < 1) {
                b.dy -= (1 - b.h) / 2;
                b.h = 1;
            }
        }
        // Path._create_closed: the first vertex repeated with CLOSEPOLY; too short to be simplified;
        // the threshold is rcParams["path.simplify_threshold"].
        auto vertices = std::make_shared<std::vector<double>>(std::vector<double>{
            b.dx, b.dy, b.dx + b.w, b.dy, b.dx + b.w, b.dy + b.h, b.dx, b.dy + b.h, b.dx, b.dy});
        auto codes = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>{1, 2, 2, 2, 79});
        mpl::PathIterator path(vertices, codes, false, 0.111111111111);
        // Affine2D().rotate_deg(angle).translate(x, height - y)
        agg::trans_affine t;
        const double c = std::cos(angle * kDegreesToRadians), s = std::sin(angle * kDegreesToRadians);
        t.sx = c, t.shx = -s, t.tx = x;
        t.shy = s, t.sy = c, t.ty = height - y;
        renderer.draw_path(gc1, path, t, rgba);
        if (!renderer.error.empty()) return renderer.error;
    }
    return {};
}

}  // namespace calculate_plot::text
