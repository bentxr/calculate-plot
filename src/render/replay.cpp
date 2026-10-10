// Derived from Matplotlib 3.11.2: src/_backend_agg_wrapper.cpp, src/_backend_agg_basic_types.h,
// src/py_converters.h (how the renderer's arguments are converted, now from a trace).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "render/replay.hpp"

#include "agg/agg_hooks.h"
#include "agg/backend_agg.h"
#include "support/base64.hpp"
#include "render/trace_reader.hpp"
#include "support/json.hpp"
#include "text/text_render.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <optional>

namespace calculate_plot::render {

namespace {

using support::Json;
using render::Reader;

// A colour tuple; `size3` remembers whether it had three entries (the alpha rules depend on it).
struct Colour {
    agg::rgba rgba;
    bool size3 = false;
};

std::optional<Colour> colour(Reader& in, const Json& j) {
    if (j.isNull()) return std::nullopt;
    if (!j.isArray() || (j.size() != 3 && j.size() != 4)) {
        in.fail("a colour must have 3 or 4 components");
        return std::nullopt;
    }
    const auto v = in.numbers(j);
    Colour c;
    c.rgba = agg::rgba(v[0], v[1], v[2], v.size() == 4 ? v[3] : 1.0);
    c.size3 = v.size() == 3;
    return c;
}

// None is black with zero alpha, as matplotlib's rgba converter makes it.
agg::rgba rgbaOrTransparent(Reader& in, const Json& j) {
    const auto c = colour(in, j);
    return c ? c->rgba : agg::rgba(0.0, 0.0, 0.0, 0.0);
}

agg::trans_affine affine(Reader& in, const Json& j) {
    agg::trans_affine t;  // None is the identity
    if (j.isNull()) return t;
    const auto v = in.numbers(j, 6);
    t.sx = v[0];
    t.shx = v[1];
    t.tx = v[2];
    t.shy = v[3];
    t.sy = v[4];
    t.ty = v[5];
    return t;
}

// Clip-path identities recorded in the trace, turned into stable addresses for the renderer's cache.
class Identities {
public:
    const void* of(long long id) {
        auto& slot = slots_[id];
        if (!slot) slot = std::make_unique<char>();
        return slot.get();
    }

private:
    std::map<long long, std::unique_ptr<char>> slots_;
};

mpl::PathIterator path(Reader& in, const Json& j, const void* id = nullptr) {
    if (j.isNull()) return mpl::PathIterator();
    auto vertices = std::make_shared<std::vector<double>>(in.numbers(in.field(j, "vertices")));
    if (vertices->size() % 2) in.fail("a path needs pairs of coordinates");
    std::shared_ptr<std::vector<uint8_t>> codes;
    const Json& c = in.field(j, "codes");
    if (!c.isNull()) {
        if (!c.isArray() || c.size() != vertices->size() / 2) in.fail("a path needs one code per vertex");
        codes = std::make_shared<std::vector<uint8_t>>();
        for (const Json& item : c.items()) {
            const long long code = in.integer(item);
            if (code < 0 || code > 255) in.fail("a path code is out of range");
            codes->push_back(static_cast<uint8_t>(code));
        }
    }
    const bool simplify = in.boolean(in.field(j, "simplify"));
    const double threshold = in.number(in.field(j, "threshold"));
    if (!in.error.empty()) return mpl::PathIterator();
    return mpl::PathIterator(vertices, codes, simplify, threshold, id);
}

agg::line_cap_e cap(Reader& in, const std::string& name) {
    if (name == "butt") return agg::butt_cap;
    if (name == "round") return agg::round_cap;
    if (name == "projecting") return agg::square_cap;
    in.fail("unknown cap style \"" + name + "\"");
    return agg::butt_cap;
}

agg::line_join_e join(Reader& in, const std::string& name) {
    if (name == "miter") return agg::miter_join_revert;
    if (name == "round") return agg::round_join;
    if (name == "bevel") return agg::bevel_join;
    in.fail("unknown join style \"" + name + "\"");
    return agg::round_join;
}

Dashes dashes(Reader& in, const Json& j) {
    Dashes d;
    d.set_dash_offset(0.0);
    if (!j.isArray() || j.size() != 2) {
        in.fail("dashes must be [offset, pattern]");
        return d;
    }
    const double offset = in.number(j[0]);
    if (j[1].isNull()) return d;
    const auto seq = in.numbers(j[1]);
    const std::size_t n = seq.size();
    if (n == 0) return d;
    // If the dash pattern has odd length, iterate through it twice (as the PDF/PS/SVG specs say).
    const std::size_t length = (n % 2) ? 2 * n : n;
    for (std::size_t i = 0; i < length; i += 2) d.add_dash_pair(seq[i % n], seq[(i + 1) % n]);
    d.set_dash_offset(offset);
    return d;
}

void graphicsContext(Reader& in, GCAgg& gc, const Json& g, Identities& ids) {
    gc.linewidth = in.number(in.field(g, "linewidth"));
    gc.alpha = in.number(in.field(g, "alpha"));
    gc.forced_alpha = in.boolean(in.field(g, "forced_alpha"));
    gc.color = rgbaOrTransparent(in, in.field(g, "rgb"));
    gc.isaa = in.boolean(in.field(g, "antialiased"));
    gc.cap = cap(in, in.text(in.field(g, "capstyle")));
    gc.join = join(in, in.text(in.field(g, "joinstyle")));
    gc.dashes = dashes(in, in.field(g, "dashes"));
    const Json& cliprect = in.field(g, "cliprect");
    gc.cliprect = agg::rect_d(0.0, 0.0, 0.0, 0.0);
    if (!cliprect.isNull()) {
        const auto r = in.numbers(cliprect, 4);
        gc.cliprect = agg::rect_d(r[0], r[1], r[2], r[3]);
    }
    const Json& clip = in.field(g, "clippath");
    gc.clippath = ClipPath();
    if (!clip.isNull()) {
        const Json* id = clip.isObject() ? clip.find("id") : nullptr;
        const void* identity = (id && id->isNumber()) ? ids.of(in.integer(*id)) : nullptr;
        gc.clippath.path = path(in, in.field(clip, "path"), identity);
        gc.clippath.trans = affine(in, in.field(clip, "transform"));
    }
    const Json& snap = in.field(g, "snap");
    gc.snap_mode = snap.isNull() ? SNAP_AUTO : (in.boolean(snap) ? SNAP_TRUE : SNAP_FALSE);
    gc.hatchpath = path(in, in.field(g, "hatch"));
    gc.hatch_color = rgbaOrTransparent(in, in.field(g, "hatch_color"));
    gc.hatch_linewidth = in.number(in.field(g, "hatch_linewidth"));
    const Json& sketch = in.field(g, "sketch");
    gc.sketch = SketchParams{0.0, 0.0, 0.0};
    if (!sketch.isNull()) {
        const auto v = in.numbers(sketch, 3);
        gc.sketch = SketchParams{v[0], v[1], v[2]};
    }
}

template <typename T, int ND>
mpl::NdArray<T, ND> array(Reader& in, const Json& j) {
    std::array<std::size_t, ND> shape{};
    const Json& s = in.field(j, "shape");
    if (!s.isArray() || s.size() < 1 || s.size() > std::size_t(ND)) {
        in.fail("an array has a bad shape");
        return mpl::NdArray<T, ND>();
    }
    std::size_t count = 1;
    for (int i = 0; i < ND; ++i) {
        shape[i] = i < int(s.size()) ? in.dimension(s[i]) : 1;
        count *= shape[i];
    }
    const auto data = in.numbers(in.field(j, "data"));
    if (data.size() != count) {
        in.fail("an array's data does not match its shape");
        return mpl::NdArray<T, ND>();
    }
    if (int(s.size()) < ND) shape[s.size()] = 0;  // e.g. (0,) for an empty (N, 4) array is (0, 0)
    std::vector<T> values(data.begin(), data.end());
    return mpl::NdArray<T, ND>(shape, std::move(values));
}

template <int ND>
mpl::NdArray<agg::int8u, ND> image(Reader& in, const Json& j) {
    std::array<std::size_t, ND> shape{};
    const Json& s = in.field(j, "shape");
    if (!s.isArray() || s.size() != std::size_t(ND)) {
        in.fail("an image has a bad shape");
        return mpl::NdArray<agg::int8u, ND>();
    }
    std::size_t count = 1;
    for (int i = 0; i < ND; ++i) count *= (shape[i] = in.dimension(s[i]));
    const auto bytes = support::decodeBase64(in.text(in.field(j, "base64")));
    if (!bytes || bytes->size() != count) {
        in.fail("an image's data does not match its shape");
        return mpl::NdArray<agg::int8u, ND>();
    }
    if (ND == 3 && shape[2] != 4) in.fail("an image must be RGBA");
    return mpl::NdArray<agg::int8u, ND>(shape, std::vector<agg::int8u>(bytes->begin(), bytes->end()));
}

// The face colour of draw_path and draw_markers: a 3-tuple, or any tuple when the alpha is forced,
// takes the graphics context's alpha.
agg::rgba face(Reader& in, const Json& j, const GCAgg& gc) {
    const auto c = colour(in, j);
    if (!c) return agg::rgba(0.0, 0.0, 0.0, 0.0);
    agg::rgba result = c->rgba;
    if (gc.forced_alpha || c->size3) result.a = gc.alpha;
    return result;
}

DashesVector dashesVector(Reader& in, const Json& j) {
    DashesVector out;
    if (!j.isArray()) {
        in.fail("expected a list of dashes");
        return out;
    }
    for (const Json& item : j.items()) out.push_back(dashes(in, item));
    return out;
}

// What the replay must still skip: the recorded output of text it drew itself.
struct Skip {
    std::size_t textImages = 0;
    std::size_t paths = 0;
    bool glyphsEvent = false;  // the "glyphs" annotation after a "text" event drawn from its string
};

std::vector<std::string> strings(Reader& in, const Json& j) {
    std::vector<std::string> out;
    if (!j.isArray()) {
        in.fail("expected a list of strings");
        return out;
    }
    for (const Json& item : j.items()) out.push_back(in.text(item));
    return out;
}

// A "glyphs" event: draw its glyphs and boxes with FreeType and the renderer, as matplotlib did.
std::string drawGlyphs(RendererAgg& r, const Json& e, Identities& ids, text::FontCache& fonts, Skip& skip) {
    Reader in;
    GCAgg gc;
    graphicsContext(in, gc, in.field(e, "gc"), ids);
    const double x = in.number(in.field(e, "x")), y = in.number(in.field(e, "y"));
    const double angle = in.number(in.field(e, "angle"));
    const auto flags = text::hintingFlags(in.text(in.field(e, "hinting")));
    if (!flags) in.fail("unknown text.hinting");
    std::vector<text::Glyph> glyphs;
    const Json& list = in.field(e, "glyphs");
    if (!list.isArray()) in.fail("expected a list of glyphs");
    for (const Json& g : list.items()) {
        FT2Font* font = fonts.byPath(in.text(in.field(g, "font")));
        if (!font) in.fail(fonts.error);
        glyphs.push_back(text::Glyph{font, in.number(in.field(g, "size")), FT_UInt(in.integer(in.field(g, "index"))),
                                     in.number(in.field(g, "slant")), in.number(in.field(g, "extend")),
                                     in.number(in.field(g, "dx")), in.number(in.field(g, "dy"))});
    }
    std::vector<text::Box> boxes;
    const Json& boxList = in.field(e, "boxes");
    if (!boxList.isArray()) in.fail("expected a list of boxes");
    for (const Json& b : boxList.items()) {
        const auto v = in.numbers(b, 4);
        boxes.push_back(text::Box{v[0], v[1], v[2], v[3]});
    }
    if (!in.error.empty()) return "glyphs: " + in.error;
    skip.textImages += glyphs.size();
    skip.paths += boxes.size();
    return text::drawGlyphsAndBoxes(r, gc, x, y, angle, glyphs, boxes, *flags);
}

// A plain "text" event: lay the string out and draw it, as RendererAgg.draw_text does.
std::string drawText(RendererAgg& r, const Json& e, Identities& ids, text::FontCache& fonts, Skip& skip,
                     const std::string& locale) {
    Reader in;
    GCAgg gc;
    graphicsContext(in, gc, in.field(e, "gc"), ids);
    const std::string s = in.text(in.field(e, "s"));
    const double x = in.number(in.field(e, "x")), y = in.number(in.field(e, "y"));
    const double angle = in.number(in.field(e, "angle"));
    const auto names = strings(in, in.field(e, "fonts"));
    const double size = in.number(in.field(e, "size"));
    const auto flags = text::hintingFlags(in.text(in.field(e, "hinting")));
    if (!flags) in.fail("unknown text.hinting");
    std::optional<std::vector<std::string>> features;
    if (!in.field(e, "features").isNull()) features = strings(in, in.field(e, "features"));
    std::optional<std::string> language;
    if (!in.field(e, "language").isNull()) language = in.text(in.field(e, "language"));
    if (!in.error.empty()) return "text: " + in.error;
    std::string error;
    const auto glyphs = text::layoutText(fonts, names, s, size, r.dpi, *flags, features, language, error, locale);
    if (!error.empty()) return "text: " + error;
    skip.glyphsEvent = true;
    skip.textImages += glyphs.size();
    return text::drawGlyphsAndBoxes(r, gc, x, y, angle, glyphs, {}, *flags);
}

std::string draw(RendererAgg& r, const Json& e, Identities& ids, const ReplayOptions& options,
                 text::FontCache& fonts, Skip& skip, const std::string& locale) {
    Reader in;
    const std::string op = in.text(in.field(e, "op"));
    if (!in.error.empty()) return in.error;
    if (op == "clear") {
        r.clear();
        return {};
    }
    if (op == "text") {
        const Json& ismath = in.field(e, "ismath");
        if (options.text == TextMode::Text && ismath.isBool() && !ismath.asBool())
            return drawText(r, e, ids, fonts, skip, locale);
        return {};
    }
    if (op == "glyphs") {
        if (skip.glyphsEvent) {
            skip.glyphsEvent = false;
            return {};
        }
        if (options.text != TextMode::Bitmaps) return drawGlyphs(r, e, ids, fonts, skip);
        return {};  // annotations: their pixels arrive as draw_text_image
    }
    if (op == "draw_text_image" && skip.textImages) {
        --skip.textImages;
        return {};
    }
    if (op == "draw_path" && skip.paths) {
        --skip.paths;
        return {};
    }
    static const char* const known[] = {"draw_path", "draw_markers", "draw_text_image", "draw_image",
                                        "draw_path_collection", "draw_quad_mesh", "draw_gouraud_triangles"};
    if (std::find(std::begin(known), std::end(known), op) == std::end(known))
        return "unknown operation \"" + op + "\"";
    GCAgg gc;
    graphicsContext(in, gc, in.field(e, "gc"), ids);
    if (op == "draw_path") {
        auto p = path(in, in.field(e, "path"));
        auto t = affine(in, in.field(e, "transform"));
        auto f = face(in, in.field(e, "face"), gc);
        if (in.error.empty()) r.draw_path(gc, p, t, f);
    } else if (op == "draw_markers") {
        auto mp = path(in, in.field(e, "marker_path"));
        auto mt = affine(in, in.field(e, "marker_transform"));
        auto p = path(in, in.field(e, "path"));
        auto t = affine(in, in.field(e, "transform"));
        auto f = face(in, in.field(e, "face"), gc);
        if (in.error.empty()) r.draw_markers(gc, mp, mt, p, t, f);
    } else if (op == "draw_text_image") {
        auto img = image<2>(in, in.field(e, "image"));
        const long long x = in.integer(in.field(e, "x")), y = in.integer(in.field(e, "y"));
        const double angle = in.number(in.field(e, "angle"));
        if (std::llabs(x) > (1LL << 30) || std::llabs(y) > (1LL << 30)) in.fail("text position out of range");
        if (in.error.empty()) r.draw_text_image(gc, img, int(x), int(y), angle);
    } else if (op == "draw_image") {
        auto img = image<3>(in, in.field(e, "image"));
        const double x = mpl_round(in.number(in.field(e, "x")));
        const double y = mpl_round(in.number(in.field(e, "y")));
        gc.alpha = 1.0;
        if (in.error.empty()) r.draw_image(gc, x, y, img);
    } else if (op == "draw_path_collection") {
        std::vector<mpl::PathIterator> paths;
        const Json& list = in.field(e, "paths");
        if (!list.isArray()) in.fail("expected a list of paths");
        for (const Json& p : list.items()) paths.push_back(path(in, p));
        mpl::PathGenerator generator(std::move(paths));
        auto master = affine(in, in.field(e, "master_transform"));
        auto transforms = array<double, 3>(in, in.field(e, "transforms"));
        auto offsets = array<double, 2>(in, in.field(e, "offsets"));
        auto offsetTrans = affine(in, in.field(e, "offset_transform"));
        auto facecolors = array<double, 2>(in, in.field(e, "facecolors"));
        auto edgecolors = array<double, 2>(in, in.field(e, "edgecolors"));
        auto linewidths = array<double, 1>(in, in.field(e, "linewidths"));
        auto linestyles = dashesVector(in, in.field(e, "dashes"));
        auto antialiaseds = array<uint8_t, 1>(in, in.field(e, "antialiaseds"));
        auto hatchcolors = array<double, 2>(in, in.field(e, "hatchcolors"));
        if (!check_trailing_shape(transforms, 3, 3) || !check_trailing_shape(offsets, 2) ||
            !check_trailing_shape(facecolors, 4) || !check_trailing_shape(edgecolors, 4) ||
            !check_trailing_shape(hatchcolors, 4))
            in.fail("draw_path_collection: an array has the wrong shape");
        if (in.error.empty())
            r.draw_path_collection(gc, master, generator, transforms, offsets, offsetTrans, facecolors, edgecolors,
                                   linewidths, linestyles, antialiaseds, hatchcolors);
    } else if (op == "draw_quad_mesh") {
        auto master = affine(in, in.field(e, "master_transform"));
        const std::size_t meshWidth = in.dimension(in.field(e, "mesh_width"));
        const std::size_t meshHeight = in.dimension(in.field(e, "mesh_height"));
        auto coordinates = array<double, 3>(in, in.field(e, "coordinates"));
        auto offsets = array<double, 2>(in, in.field(e, "offsets"));
        auto offsetTrans = affine(in, in.field(e, "offset_transform"));
        auto facecolors = array<double, 2>(in, in.field(e, "facecolors"));
        const bool antialiased = in.boolean(in.field(e, "antialiased"));
        auto edgecolors = array<double, 2>(in, in.field(e, "edgecolors"));
        if (coordinates.shape(0) != meshHeight + 1 || coordinates.shape(1) != meshWidth + 1 ||
            coordinates.shape(2) != 2 || !check_trailing_shape(offsets, 2) ||
            !check_trailing_shape(facecolors, 4) || !check_trailing_shape(edgecolors, 4))
            in.fail("draw_quad_mesh: an array has the wrong shape");
        if (in.error.empty())
            r.draw_quad_mesh(gc, master, unsigned(meshWidth), unsigned(meshHeight), coordinates, offsets, offsetTrans,
                             facecolors, antialiased, edgecolors);
    } else {  // draw_gouraud_triangles
        auto points = array<double, 3>(in, in.field(e, "points"));
        auto colors = array<double, 3>(in, in.field(e, "colors"));
        auto t = affine(in, in.field(e, "transform"));
        if (in.error.empty()) r.draw_gouraud_triangles(gc, points, colors, t);
    }
    return in.error.empty() ? std::string() : op + ": " + in.error;
}

}  // namespace

ReplayResult replayTrace(std::string_view traceJson, const ReplayOptions& options) {
    ReplayResult result;
    support::JsonError jsonError;
    const auto trace = support::parseJson(traceJson, &jsonError);
    if (!trace || !trace->isObject()) {
        result.error = "the trace is not valid JSON: " + jsonError.message;
        return result;
    }
    Reader in;
    if (in.integer(in.field(*trace, "version")) != 1) {
        result.error = "unsupported trace version";
        return result;
    }
    const std::size_t width = in.dimension(in.field(*trace, "width"));
    const std::size_t height = in.dimension(in.field(*trace, "height"));
    const double dpi = in.number(in.field(*trace, "dpi"));
    const Json& events = in.field(*trace, "events");
    if (!events.isArray()) in.fail("\"events\" must be a list");
    if (!in.error.empty()) {
        result.error = in.error;
        return result;
    }
    RendererAgg renderer(unsigned(width), unsigned(height), dpi);
    if (!renderer.error.empty()) {
        result.error = renderer.error;
        return result;
    }
    // The locale matplotlib ran in decides how text is shaped when it sets no language.
    const Json* recordedLocale = trace->find("locale");
    const std::string locale =
        recordedLocale && recordedLocale->isString() ? recordedLocale->asString() : text::kReferenceLocale;
    Identities ids;
    text::FontCache fonts(options.fontDirectory);
    Skip skip;
    for (const Json& event : events.items()) {
        agg_hooks::cellBlockLimitExceeded() = false;
        const std::string error = draw(renderer, event, ids, options, fonts, skip, locale);
        if (agg_hooks::cellBlockLimitExceeded()) renderer.error = "Exceeded cell block limit";
        if (!error.empty() || !renderer.error.empty()) {
            result.error = error.empty() ? renderer.error : error;
            return result;
        }
    }
    result.image.width = int(width);
    result.image.height = int(height);
    const agg::int8u* pixels = renderer.pixBuffer;
    result.image.rgba.assign(pixels, pixels + std::size_t(width) * height * 4);
    return result;
}

}  // namespace calculate_plot::render

namespace calculate_plot::render {

std::string toJson(const support::Json& value) {
    switch (value.type()) {
    case support::Json::Type::Null: return "null";
    case support::Json::Type::Bool: return value.asBool() ? "true" : "false";
    case support::Json::Type::Number: return value.numberText();
    case support::Json::Type::String: {
        std::string out = "\"";
        for (const char c : value.asString()) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out + "\"";
    }
    case support::Json::Type::Array: {
        std::string out = "[";
        for (std::size_t i = 0; i < value.size(); ++i) out += (i ? "," : "") + toJson(value[i]);
        return out + "]";
    }
    case support::Json::Type::Object: {
        std::string out = "{";
        bool first = true;
        for (const auto& [k, v] : value.members()) {
            out += (first ? "\"" : ",\"") + k + "\":" + toJson(v);
            first = false;
        }
        return out + "}";
    }
    }
    return "null";
}

}  // namespace calculate_plot::render
