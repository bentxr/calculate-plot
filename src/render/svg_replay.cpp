#include "render/svg_replay.hpp"

#include "render/trace_reader.hpp"
#include "support/base64.hpp"
#include "svg/svg_renderer.h"
#include "text/text_render.h"

#include <memory>

namespace calculate_plot::render {

namespace {

svg::PathData path(Reader& in, const Json& j) {
    svg::PathData p;
    if (j.isNull()) return p;
    auto vertices = std::make_shared<std::vector<double>>(in.numbers(in.field(j, "vertices")));
    if (vertices->size() % 2) in.fail("a path needs pairs of coordinates");
    p.vertices = vertices;
    const Json& c = in.field(j, "codes");
    if (!c.isNull()) {
        auto codes = std::make_shared<std::vector<unsigned char>>();
        if (!c.isArray() || c.size() != vertices->size() / 2) in.fail("a path needs one code per vertex");
        for (const Json& item : c.items()) codes->push_back(static_cast<unsigned char>(in.integer(item)));
        p.codes = codes;
    }
    p.simplify = in.boolean(in.field(j, "simplify"));
    p.threshold = in.number(in.field(j, "threshold"));
    return p;
}

svg::Affine affine(Reader& in, const Json& j) {
    svg::Affine a;
    if (j.isNull()) return a;
    const auto v = in.numbers(j, 6);
    a.sx = v[0], a.shx = v[1], a.tx = v[2], a.shy = v[3], a.sy = v[4], a.ty = v[5];
    return a;
}

std::optional<std::string> optionalText(Reader& in, const Json& j) {
    if (j.isNull()) return std::nullopt;
    return in.text(j);
}

std::optional<std::vector<double>> optionalNumbers(Reader& in, const Json& j) {
    if (j.isNull()) return std::nullopt;
    return in.numbers(j);
}

svg::Gc graphicsContext(Reader& in, const Json& g, const Json& clipTransformText) {
    svg::Gc gc;
    gc.linewidth = in.number(in.field(g, "linewidth"));
    gc.alpha = in.number(in.field(g, "alpha"));
    gc.forcedAlpha = in.boolean(in.field(g, "forced_alpha"));
    if (auto rgb = optionalNumbers(in, in.field(g, "rgb"))) gc.rgb = *rgb;
    gc.antialiased = in.boolean(in.field(g, "antialiased"));
    gc.capstyle = in.text(in.field(g, "capstyle"));
    gc.joinstyle = in.text(in.field(g, "joinstyle"));
    const Json& dashes = in.field(g, "dashes");
    if (!dashes.isArray() || dashes.size() != 2) {
        in.fail("dashes must be [offset, pattern]");
    } else {
        gc.dashOffset = in.number(dashes[0]);
        gc.dashes = optionalNumbers(in, dashes[1]);
    }
    if (auto r = optionalNumbers(in, in.field(g, "cliprect"))) {
        if (r->size() != 4) in.fail("a clip rectangle needs 4 numbers");
        else gc.cliprect = std::array<double, 4>{(*r)[0], (*r)[1], (*r)[2], (*r)[3]};
    }
    const Json& clip = in.field(g, "clippath");
    if (!clip.isNull()) {
        svg::Gc::ClipPath c;
        c.path = path(in, in.field(clip, "path"));
        c.transform = affine(in, in.field(clip, "transform"));
        c.index = int(in.integer(in.field(g, "clip_index")));
        c.transformText = in.text(clipTransformText);
        gc.clippath = c;
    }
    const Json& snap = in.field(g, "snap");
    if (!snap.isNull()) gc.snap = in.boolean(snap);
    gc.hatch = optionalText(in, in.field(g, "hatch_string"));
    gc.hatchNumpy = in.boolean(in.field(g, "hatch_numpy"));
    gc.hatchPath = path(in, in.field(g, "hatch"));
    gc.hatchColor = optionalNumbers(in, in.field(g, "hatch_color"));
    gc.hatchLinewidth = in.number(in.field(g, "hatch_linewidth"));
    if (auto s = optionalNumbers(in, in.field(g, "sketch"))) {
        if (s->size() != 3) in.fail("sketch parameters need 3 numbers");
        else gc.sketch = std::array<double, 3>{(*s)[0], (*s)[1], (*s)[2]};
    }
    gc.url = optionalText(in, in.field(g, "url"));
    gc.gid = optionalText(in, in.field(g, "gid"));
    return gc;
}

std::optional<svg::Face> face(Reader& in, const Json& j) {
    auto v = optionalNumbers(in, j);
    if (!v) return std::nullopt;
    if (v->size() != 3 && v->size() != 4) in.fail("a colour must have 3 or 4 components");
    return svg::Face{*v, false};
}

// An array recorded as {"shape", "data"}, as rows of `width` numbers.
std::vector<std::vector<double>> rows(Reader& in, const Json& j, std::size_t width) {
    const auto data = in.numbers(in.field(j, "data"));
    std::vector<std::vector<double>> out;
    if (data.size() % width) {
        in.fail("an array does not have rows of " + std::to_string(width));
        return out;
    }
    for (std::size_t i = 0; i < data.size(); i += width) out.emplace_back(data.begin() + long(i), data.begin() + long(i + width));
    return out;
}

std::vector<std::string> strings(Reader& in, const Json& j) {
    std::vector<std::string> out;
    if (!j.isArray()) in.fail("expected a list of strings");
    for (const Json& item : j.items()) out.push_back(in.text(item));
    return out;
}

svg::Metadata metadata(Reader& in, const Json& j) {
    svg::Metadata m;
    if (j.isNull()) return m;
    if (!j.isObject()) in.fail("metadata must be an object");
    for (const auto& [key, value] : j.members()) {
        if (value.isNull()) m.entries[key] = std::nullopt;
        else if (value.isString()) m.entries[key] = std::vector<std::string>{value.asString()};
        else m.entries[key] = strings(in, value);
    }
    return m;
}

}  // namespace

SvgReplayResult replaySvgTrace(std::string_view traceJson, const std::string& fontDirectory) {
    SvgReplayResult result;
    support::JsonError jsonError;
    const auto trace = support::parseJson(traceJson, &jsonError);
    if (!trace || !trace->isObject()) {
        result.error = "the trace is not valid JSON: " + jsonError.message;
        return result;
    }
    Reader in;
    if (in.integer(in.field(*trace, "version")) != 1) in.fail("unsupported trace version");
    if (in.text(in.field(*trace, "fonttype")) != "path") in.fail("only svg.fonttype 'path' is supported");
    const Json& events = in.field(*trace, "events");
    if (!events.isArray() || events.size() == 0 || in.text(in.field(events[0], "op")) != "begin")
        in.fail("a trace starts with its 'begin' event");
    if (!in.error.empty()) {
        result.error = in.error;
        return result;
    }
    const Json& begin = events[0];
    const Json* recordedLocale = trace->find("locale");
    const std::string locale =
        recordedLocale && recordedLocale->isString() ? recordedLocale->asString() : text::kReferenceLocale;
    text::FontCache fonts(fontDirectory);
    const Json& salt = in.field(*trace, "hashsalt");
    if (salt.isNull()) in.fail("svg.hashsalt must be fixed (matplotlib would use a random one)");
    svg::RendererSVG r(in.number(in.field(begin, "width")), in.number(in.field(begin, "height")),
                       in.number(in.field(begin, "image_dpi")), metadata(in, in.field(*trace, "metadata")),
                       salt.isString() ? salt.asString() : "", optionalText(in, in.field(*trace, "svg_id")), fonts,
                       locale);
    for (std::size_t i = 1; i < events.size() && in.error.empty() && r.error.empty(); ++i) {
        const Json& e = events[i];
        const std::string op = in.text(in.field(e, "op"));
        if (op == "open_group") {
            r.openGroup(in.text(in.field(e, "s")), optionalText(in, in.field(e, "gid")));
            continue;
        }
        if (op == "close_group") {
            r.closeGroup();
            continue;
        }
        const svg::Gc gc = graphicsContext(in, in.field(e, "gc"), in.field(e, "clip_transform"));
        if (op == "draw_path") {
            const auto p = path(in, in.field(e, "path"));
            const auto t = affine(in, in.field(e, "transform"));
            const auto f = face(in, in.field(e, "face"));
            if (in.error.empty()) r.drawPath(gc, p, t, f);
        } else if (op == "draw_markers") {
            const auto mp = path(in, in.field(e, "marker_path"));
            const auto mt = affine(in, in.field(e, "marker_transform"));
            const auto p = path(in, in.field(e, "path"));
            const auto t = affine(in, in.field(e, "transform"));
            const auto f = face(in, in.field(e, "face"));
            if (in.error.empty()) r.drawMarkers(gc, mp, mt, p, t, f);
        } else if (op == "draw_path_collection") {
            svg::RendererSVG::Collection c;
            c.masterTransform = affine(in, in.field(e, "master_transform"));
            const Json& paths = in.field(e, "paths");
            if (!paths.isArray()) in.fail("expected a list of paths");
            for (const Json& p : paths.items()) c.paths.push_back(path(in, p));
            for (const auto& m : rows(in, in.field(e, "transforms"), 9)) {
                svg::Affine a;
                a.sx = m[0], a.shx = m[1], a.tx = m[2], a.shy = m[3], a.sy = m[4], a.ty = m[5];
                c.transforms.push_back(a);
            }
            for (const auto& o : rows(in, in.field(e, "offsets"), 2)) c.offsets.push_back({o[0], o[1]});
            c.offsetTransform = affine(in, in.field(e, "offset_transform"));
            c.facecolors = rows(in, in.field(e, "facecolors"), 4);
            c.edgecolors = rows(in, in.field(e, "edgecolors"), 4);
            c.hatchcolors = rows(in, in.field(e, "hatchcolors"), 4);
            c.linewidths = in.numbers(in.field(in.field(e, "linewidths"), "data"));
            const Json& dashes = in.field(e, "dashes");
            if (!dashes.isArray()) in.fail("expected a list of dashes");
            for (const Json& d : dashes.items()) {
                if (!d.isArray() || d.size() != 2) {
                    in.fail("dashes must be [offset, pattern]");
                    break;
                }
                c.linestyles.emplace_back(in.number(d[0]), optionalNumbers(in, d[1]));
            }
            for (double a : in.numbers(in.field(in.field(e, "antialiaseds"), "data"))) c.antialiaseds.push_back(a != 0);
            const Json& urls = in.field(e, "urls");
            if (!urls.isArray()) in.fail("expected a list of urls");
            for (const Json& u : urls.items()) c.urls.push_back(optionalText(in, u));
            if (in.error.empty()) r.drawPathCollection(gc, c);
        } else if (op == "draw_gouraud_triangles") {
            const auto points = rows(in, in.field(e, "points"), 6);
            const auto colors = rows(in, in.field(e, "colors"), 12);
            const auto t = affine(in, in.field(e, "transform"));
            std::vector<std::array<std::array<double, 2>, 3>> tris;
            std::vector<std::array<std::array<double, 4>, 3>> cols;
            for (const auto& p : points) tris.push_back({{{p[0], p[1]}, {p[2], p[3]}, {p[4], p[5]}}});
            for (const auto& c : colors)
                cols.push_back({{{c[0], c[1], c[2], c[3]}, {c[4], c[5], c[6], c[7]}, {c[8], c[9], c[10], c[11]}}});
            if (tris.size() != cols.size()) in.fail("points and colors differ in length");
            if (in.error.empty()) r.drawGouraudTriangles(gc, tris, cols, t);
        } else if (op == "draw_image") {
            const Json& image = in.field(e, "image");
            const Json& shape = in.field(image, "shape");
            const auto bytes = support::decodeBase64(in.text(in.field(image, "base64")));
            if (!shape.isArray() || shape.size() != 3 || !bytes) {
                in.fail("bad image");
            } else {
                const int h = int(in.dimension(shape[0])), w = int(in.dimension(shape[1]));
                if (in.dimension(shape[2]) != 4 || bytes->size() != std::size_t(w) * h * 4) in.fail("bad image size");
                std::optional<svg::Affine> t;
                if (!in.field(e, "transform").isNull()) t = affine(in, in.field(e, "transform"));
                if (in.error.empty())
                    r.drawImage(gc, in.number(in.field(e, "x")), in.number(in.field(e, "y")), w, h,
                                std::vector<unsigned char>(bytes->begin(), bytes->end()), t);
            }
        } else if (op == "draw_text") {
            const std::string s = in.text(in.field(e, "s"));
            const double x = in.number(in.field(e, "x")), y = in.number(in.field(e, "y"));
            const double size = in.number(in.field(e, "size")), angle = in.number(in.field(e, "angle"));
            const Json& ismath = in.field(e, "ismath");
            if (ismath.isBool() && ismath.asBool()) {
                const Json& math = in.field(e, "math");
                std::vector<svg::Glyph> glyphs;
                const Json& list = in.field(math, "glyphs");
                if (!list.isArray()) in.fail("expected a list of glyphs");
                for (const Json& g : list.items())
                    glyphs.push_back(svg::Glyph{in.text(in.field(g, "font")), in.number(in.field(g, "size")),
                                                unsigned(in.integer(in.field(g, "index"))), in.number(in.field(g, "ox")),
                                                in.number(in.field(g, "oy"))});
                std::vector<std::array<double, 4>> rects;
                const Json& rectList = in.field(math, "rects");
                if (!rectList.isArray()) in.fail("expected a list of rectangles");
                for (const Json& rect : rectList.items()) {
                    const auto v = in.numbers(rect, 4);
                    rects.push_back({v[0], v[1], v[2], v[3]});
                }
                if (in.error.empty()) r.drawMathtextAsPath(gc, x, y, s, size, angle, glyphs, rects);
            } else if (ismath.isBool()) {
                const auto names = strings(in, in.field(e, "fonts"));
                std::optional<std::vector<std::string>> features;
                if (!in.field(e, "features").isNull()) features = strings(in, in.field(e, "features"));
                const auto language = optionalText(in, in.field(e, "language"));
                if (in.error.empty()) r.drawTextAsPath(gc, x, y, s, names, size, angle, features, language);
            } else {
                in.fail("TeX text is not supported");
            }
        } else {
            in.fail("unknown operation \"" + op + "\"");
        }
        if (!in.error.empty()) in.error = op + ": " + in.error;
    }
    result.svg = r.finalize();
    result.hashedContents = r.hashedContents;
    result.error = !in.error.empty() ? in.error : r.error;
    return result;
}

}  // namespace calculate_plot::render
