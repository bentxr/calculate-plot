// Derived from Matplotlib 3.11.2: lib/matplotlib/textpath.py (TextToPath.get_text_path, get_glyphs_mathtext,
// TextPath).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/text_path.hpp"

#include "support/histogram.hpp"
#include "text/font_manager.hpp"
#include "text/ft2font.h"
#include "text/mathtext.hpp"
#include "text/text_render.h"
#include "transforms/transforms.hpp"

#include <limits>

namespace calculate_plot::engine {

namespace {
constexpr double kFontScale = 100.0;  // TextToPath.FONT_SCALE
constexpr double kDpi = 72.0;         // TextToPath.DPI
}  // namespace

bool mathtextPath(text::FontCache& fonts, const std::string& s, const rc::Params& rc, Path* out, std::string* error) {
    // get_glyphs_mathtext: the layout of prop.copy() with its size set to FONT_SCALE
    text::FontProperties prop = text::FontProperties::defaults(rc);
    const double size = prop.size;  // (TextPath's size: prop.get_size_in_points())
    prop.size = kFontScale;
    const auto parse = text::parseMath(fonts, s, kDpi, prop, rc, error);
    if (!parse) return false;
    std::vector<double> verts;
    std::vector<unsigned char> codes;
    for (const auto& g : parse->glyphs) {
        // font.clear(); font.set_size(FONT_SCALE, DPI); font.load_glyph(glyph_index, NO_HINTING); font.get_path()
        g.font->clear();
        g.font->set_size(kFontScale, kDpi);
        g.font->load_glyph(g.index, FT_LOAD_NO_HINTING);
        std::vector<double> v;
        std::vector<unsigned char> c;
        g.font->get_path(v, c);
        const double scale = g.fontsize / kFontScale;
        // verts1 * scale + [xposition, yposition]
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            verts.push_back(v[i] * scale + g.ox);
            verts.push_back(v[i + 1] * scale + g.oy);
        }
        codes.insert(codes.end(), c.begin(), c.end());
    }
    for (const auto& r : parse->rects) {
        const double ox = r[0], oy = r[1], w = r[2], h = r[3];
        for (double v : {ox, oy, ox, oy + h, ox + w, oy + h, ox + w, oy, ox, oy, 0.0, 0.0}) verts.push_back(v);
        for (unsigned char c : {MOVETO, LINETO, LINETO, LINETO, LINETO, CLOSEPOLY}) codes.push_back(c);
    }
    // TextPath's vertices: Affine2D().scale(size / FONT_SCALE).translate(0, 0) of them
    transforms::Affine2D tr;
    tr.scale(size / kFontScale);
    tr.translate(0, 0);
    const auto m = tr.matrix();
    for (std::size_t i = 0; i + 1 < verts.size(); i += 2) {
        const auto p = transforms::apply(m, verts[i], verts[i + 1]);
        verts[i] = p[0], verts[i + 1] = p[1];
    }
    *out = Path::make(std::move(verts), std::move(codes), rc);
    out->shouldSimplify = false;  // (TextPath._should_simplify)
    return true;
}

std::array<double, 4> pathExtents(const Path& path) {
    std::vector<std::array<double, 2>> xys;
    const auto& v = path.vertices ? *path.vertices : std::vector<double>{};
    bool curved = false;
    if (path.codes)
        for (unsigned char c : *path.codes) curved = curved || c == CURVE3 || c == CURVE4;
    if (!path.codes) {
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) xys.push_back({v[i], v[i + 1]});
    } else if (!curved) {  // the MOVETO and LINETO vertices (STOP and CLOSEPOLY ones are not drawn)
        for (std::size_t i = 0; i + 1 < v.size(); i += 2)
            if ((*path.codes)[i / 2] == MOVETO || (*path.codes)[i / 2] == LINETO) xys.push_back({v[i], v[i + 1]});
    } else {
        xys = bezierEnds(path);
    }
    if (xys.empty()) {
        const double inf = std::numeric_limits<double>::infinity();
        return {inf, inf, -inf, -inf};
    }
    std::vector<double> xs, ys;
    for (const auto& p : xys) xs.push_back(p[0]), ys.push_back(p[1]);
    return {support::npMin(xs), support::npMin(ys), support::npMax(xs), support::npMax(ys)};
}

}  // namespace calculate_plot::engine
