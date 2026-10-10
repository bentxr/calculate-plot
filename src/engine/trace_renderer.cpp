#include "engine/trace_renderer.hpp"

#include "rc/rc_params.hpp"
#include "support/base64.hpp"
#include "text/mathtext.hpp"

namespace calculate_plot::engine {

TraceRenderer::TraceRenderer(double width, double height, double dpi, const rc::Params& rc, std::string locale,
                             std::string fontDirectory)
    : width_(width), height_(height), dpi_(dpi), rc_(rc), locale_(std::move(locale)), fonts_(std::move(fontDirectory)) {
    events_.beginArray();
}

void TraceRenderer::writeNumbers(const std::vector<double>& values) {
    events_.beginArray();
    for (double v : values) events_.value(v);
    events_.endArray();
}

void TraceRenderer::writeMatrix(const transforms::Matrix& m) {
    writeNumbers({m.sx, m.shx, m.tx, m.shy, m.sy, m.ty});
}

void TraceRenderer::writePath(const Path& path) {
    events_.beginObject();
    events_.key("vertices");
    writeNumbers(path.vertices ? *path.vertices : std::vector<double>{});
    events_.key("codes");
    if (path.codes) {
        events_.beginArray();
        for (unsigned char c : *path.codes) events_.value(int(c));
        events_.endArray();
    } else {
        events_.null();
    }
    events_.key("simplify").value(path.shouldSimplify);
    events_.key("threshold").value(path.simplifyThreshold);
    events_.endObject();
}

void TraceRenderer::writeGc(const GC& gc) {
    events_.beginObject();
    writeGcFields(gc);
    events_.endObject();
}

void TraceRenderer::writeGcFields(const GC& gc) {
    events_.key("linewidth").value(gc.linewidth);
    events_.key("alpha").value(gc.alpha);
    events_.key("forced_alpha").value(gc.forcedAlpha);
    events_.key("rgb");
    writeNumbers(gc.rgb);
    events_.key("antialiased").value(gc.antialiased);
    events_.key("capstyle").value(gc.capstyle);
    events_.key("joinstyle").value(gc.joinstyle);
    events_.key("dashes").beginArray().value(gc.dashOffset);
    if (gc.dashes) writeNumbers(*gc.dashes);
    else events_.null();
    events_.endArray();
    events_.key("cliprect");
    if (gc.cliprect) writeNumbers({(*gc.cliprect)[0], (*gc.cliprect)[1], (*gc.cliprect)[2], (*gc.cliprect)[3]});
    else events_.null();
    events_.key("clippath");
    if (gc.clippath) {
        events_.beginObject();
        events_.key("path");
        writePath(gc.clippath->path);
        events_.key("id").value(clipIndex(*gc.clippath));
        events_.key("transform");
        writeMatrix(gc.clippath->affine->matrix());
        events_.endObject();
    } else {
        events_.null();
    }
    events_.key("snap");
    if (gc.snap) events_.value(*gc.snap);
    else events_.null();
    events_.key("hatch").null();  // hatch paths come with Plan 8's hatched artists
    events_.key("hatch_color");
    if (gc.hatchColor) writeNumbers(*gc.hatchColor);
    else events_.null();
    events_.key("hatch_linewidth").value(gc.hatchLinewidth);
    events_.key("sketch");
    if (gc.sketch) writeNumbers({(*gc.sketch)[0], (*gc.sketch)[1], (*gc.sketch)[2]});
    else events_.null();
}

void TraceRenderer::writeOptionalText(const std::optional<std::string>& text) {
    if (text) events_.value(*text);
    else events_.null();
}

void TraceRenderer::writeArray(const std::vector<std::size_t>& shape, const std::vector<double>& data) {
    events_.beginObject().key("shape").beginArray();
    for (std::size_t n : shape) events_.value(static_cast<long long>(n));
    events_.endArray().key("data");
    writeNumbers(data);
    events_.endObject();
}

void TraceRenderer::writeCollectionFields(const PathCollectionArgs& a) {
    events_.key("master_transform");
    writeMatrix(a.master);
    events_.key("paths").beginArray();
    for (const Path& p : a.paths) writePath(p);
    events_.endArray();
    std::vector<double> t;
    for (const auto& m : a.transforms) {
        for (double v : {m.sx, m.shx, m.tx, m.shy, m.sy, m.ty, 0.0, 0.0, 1.0}) t.push_back(v);
    }
    events_.key("transforms");
    writeArray({a.transforms.size(), 3, 3}, t);
    std::vector<double> o;
    for (const auto& p : a.offsets) o.push_back(p[0]), o.push_back(p[1]);
    events_.key("offsets");
    writeArray({a.offsets.size(), 2}, o);
    events_.key("offset_transform");
    writeMatrix(a.offsetTrans);
    const auto colors = [&](const char* key, const std::vector<std::array<double, 4>>& c) {
        std::vector<double> d;
        for (const auto& x : c) d.insert(d.end(), x.begin(), x.end());
        events_.key(key);
        writeArray({c.size(), 4}, d);
    };
    colors("facecolors", a.facecolors);
    colors("edgecolors", a.edgecolors);
    events_.key("linewidths");
    writeArray({a.linewidths.size()}, a.linewidths);
    events_.key("dashes").beginArray();
    for (const auto& [offset, pattern] : a.dashes) {
        events_.beginArray().value(offset);
        if (pattern) writeNumbers(*pattern);
        else events_.null();
        events_.endArray();
    }
    events_.endArray();
    std::vector<double> aa;
    for (bool b : a.antialiaseds) aa.push_back(b ? 1.0 : 0.0);
    events_.key("antialiaseds");
    writeArray({a.antialiaseds.size()}, aa);
}

int TraceRenderer::clipIndex(const ClipPath& clip) {
    const void* identity = clip.path.vertices.get();
    auto it = clipIds_.find(identity);
    if (it == clipIds_.end()) it = clipIds_.emplace(identity, int(clipIds_.size())).first;
    return it->second;
}

std::string TraceRenderer::events() {
    support::JsonWriter copy = events_;
    copy.endArray();
    return copy.text();
}

void AggTraceRenderer::clear() {
    events_.beginObject().key("op").value("clear").endObject();
}

void AggTraceRenderer::drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                                const std::optional<std::vector<double>>& face) {
    events_.beginObject();
    events_.key("op").value("draw_path");
    events_.key("gc");
    writeGc(gc);
    events_.key("path");
    writePath(path);
    events_.key("transform");
    writeMatrix(affine);
    events_.key("face");
    writeFace(face);
    events_.endObject();
}

void AggTraceRenderer::writeFace(const std::optional<std::vector<double>>& face) {
    if (face) writeNumbers(*face);
    else events_.null();
}

void AggTraceRenderer::drawMarkers(const GC& gc, const Path& markerPath, const transforms::Matrix& markerTrans,
                                   const Path& path, const transforms::Matrix& trans,
                                   const std::optional<std::vector<double>>& face) {
    events_.beginObject();
    events_.key("op").value("draw_markers");
    events_.key("gc");
    writeGc(gc);
    events_.key("marker_path");
    writePath(markerPath);
    events_.key("marker_transform");
    writeMatrix(markerTrans);
    events_.key("path");
    writePath(path);
    events_.key("transform");
    writeMatrix(trans);
    events_.key("face");
    writeFace(face);
    events_.endObject();
}

void AggTraceRenderer::drawPathCollection(const GC& gc, const PathCollectionArgs& a) {
    events_.beginObject();
    events_.key("op").value("draw_path_collection");
    events_.key("gc");
    writeGc(gc);
    writeCollectionFields(a);
    std::vector<double> h;
    for (const auto& x : a.hatchcolors) h.insert(h.end(), x.begin(), x.end());
    events_.key("hatchcolors");
    writeArray({a.hatchcolors.size(), 4}, h);
    events_.endObject();
}

void AggTraceRenderer::drawText(const GC& gc, double x, double y, const std::string& s, const FontSpec& font,
                                double angle, bool ismath, const TextOptions& mtext) {
    const std::string hinting = rc_.text("text.hinting");
    events_.beginObject();
    events_.key("op").value("text");
    events_.key("s").value(s);
    events_.key("x").value(x);
    events_.key("y").value(y);
    events_.key("angle").value(angle);
    events_.key("ismath").value(ismath);
    events_.key("fonts").beginArray();
    for (const auto& f : font.files) events_.value(f);
    events_.endArray();
    events_.key("size").value(font.size);
    events_.key("features");
    if (mtext.features) {
        events_.beginArray();
        for (const auto& f : *mtext.features) events_.value(f);
        events_.endArray();
    } else {
        events_.null();
    }
    events_.key("language");
    writeOptionalText(mtext.language);
    events_.key("antialiased").value(gc.antialiased);
    events_.key("hinting").value(hinting);
    events_.key("gc");
    writeGc(gc);
    events_.endObject();
    const auto flags = text::hintingFlags(hinting);
    if (!flags) {
        error_ = "unknown text.hinting " + hinting;
        return;
    }
    std::string error;
    std::vector<text::Glyph> glyphs;
    std::vector<std::array<double, 4>> boxes;
    if (ismath) {
        // RendererAgg.draw_mathtext: mathtext's vector layout, glyphs placed by their offsets
        const auto parse = text::parseMath(fonts_, s, dpi_, font.props, rc_, &error);
        if (!parse) {
            error_ = error;
            return;
        }
        for (const auto& g : parse->glyphs) glyphs.push_back(text::Glyph{g.font, g.fontsize, g.index, 0.0, 1.0, g.ox, g.oy});
        boxes = parse->rects;
        if (!boxes.empty()) {
            error_ = "mathtext rules (fraction bars, roots) are not supported yet";
            return;
        }
    } else {
        // RendererAgg.draw_text: the layout, then _draw_text_glyphs_and_boxes.
        glyphs = text::layoutText(fonts_, font.files, s, font.size, dpi_, *flags, mtext.features, mtext.language, error,
                                  locale_);
        if (!error.empty()) {
            error_ = error;
            return;
        }
    }
    events_.beginObject();
    events_.key("op").value("glyphs");
    events_.key("x").value(x);
    events_.key("y").value(y);
    events_.key("angle").value(angle);
    events_.key("dpi").value(dpi_);
    events_.key("height").value(height_);
    events_.key("glyphs").beginArray();
    for (const auto& g : glyphs) {
        events_.beginObject();
        events_.key("font").value(fonts_.nameOf(g.font));
        events_.key("size").value(g.size);
        events_.key("index").value(static_cast<long long>(g.index));
        events_.key("slant").value(g.slant);
        events_.key("extend").value(g.extend);
        events_.key("dx").value(g.dx);
        events_.key("dy").value(g.dy);
        events_.endObject();
    }
    events_.endArray();
    events_.key("boxes").beginArray().endArray();
    events_.key("hinting").value(hinting);
    events_.key("gc");
    writeGc(gc);
    events_.endObject();

    const auto images = text::renderGlyphs(height_, dpi_, x, y, angle, glyphs, gc.antialiased, *flags, error);
    if (!error.empty()) {
        error_ = error;
        return;
    }
    for (const auto& image : images) {
        events_.beginObject();
        events_.key("op").value("draw_text_image");
        events_.key("image").beginObject();
        events_.key("shape").beginArray().value(static_cast<long long>(image.rows)).value(static_cast<long long>(image.width)).endArray();
        events_.key("base64").value(support::encodeBase64(
            std::string_view(reinterpret_cast<const char*>(image.buffer.data()), image.buffer.size())));
        events_.endObject();
        events_.key("x").value(image.x);
        events_.key("y").value(image.y);
        events_.key("angle").value(0.0);
        events_.key("gc");
        writeGc(gc);
        events_.endObject();
    }
}

TextMetrics AggTraceRenderer::textWidthHeightDescent(const std::string& s, const FontSpec& font, bool ismath) {
    if (ismath) {
        std::string error;
        const auto parse = text::parseMath(fonts_, s, dpi_, font.props, rc_, &error);
        if (!parse) {
            error_ = error;
            return {};
        }
        return {parse->width, parse->height, parse->depth};
    }
    const auto flags = text::hintingFlags(rc_.text("text.hinting"));
    if (!flags) {
        error_ = "unknown text.hinting";
        return {};
    }
    std::string error;
    const auto e = text::textExtent(fonts_, font.files, s, font.size, dpi_, *flags, std::nullopt, error, locale_);
    if (!error.empty()) error_ = error;
    return {e.width, e.height, e.descent};
}

std::string AggTraceRenderer::trace() {
    support::JsonWriter out;
    out.beginObject();
    out.key("version").value(1);
    out.key("matplotlib").value("3.11.2");
    out.key("locale").value(locale_);
    out.key("width").value(static_cast<long long>(width_));
    out.key("height").value(static_cast<long long>(height_));
    out.key("dpi").value(dpi_);
    out.key("events").raw(events());
    out.endObject();
    return out.text();
}

SvgTraceRenderer::SvgTraceRenderer(double width, double height, double imageDpi, const rc::Params& rc,
                                   std::string locale, std::string fontDirectory, SvgMetadata metadata,
                                   std::string hashsalt)
    : TraceRenderer(width, height, 72.0, rc, std::move(locale), std::move(fontDirectory)),
      imageDpi_(imageDpi),
      metadata_(std::move(metadata)),
      hashsalt_(std::move(hashsalt)) {
    events_.beginObject();
    events_.key("op").value("begin");
    events_.key("width").value(width_);
    events_.key("height").value(height_);
    events_.key("image_dpi").value(imageDpi_);
    events_.endObject();
}

void SvgTraceRenderer::openGroup(const std::string& s, const std::optional<std::string>& gid) {
    events_.beginObject().key("op").value("open_group").key("s").value(s).key("gid");
    writeOptionalText(gid);
    events_.endObject();
}

void SvgTraceRenderer::closeGroup(const std::string& s) {
    events_.beginObject().key("op").value("close_group").key("s").value(s).endObject();
}

void SvgTraceRenderer::writeSvgGc(const GC& gc) {
    // The Agg fields, then what RendererSVG also reads (record.py's svg_gc).
    events_.beginObject();
    writeGcFields(gc);
    events_.key("url");
    writeOptionalText(gc.url);
    events_.key("gid");
    writeOptionalText(gc.gid);
    events_.key("hatch_string");
    writeOptionalText(gc.hatch);
    events_.key("hatch_numpy").value(gc.hatchNumpy);
    events_.key("rgb_length").value(static_cast<long long>(gc.rgb.size()));
    events_.key("clip_index");
    if (gc.clippath) events_.value(clipIndex(*gc.clippath));
    else events_.null();
    events_.endObject();
}

void SvgTraceRenderer::writeClipTransform(const GC& gc) {
    // str() of the clip path's transform tree, which the clip path's id hashes; it comes with the
    // first clipped artists.
    if (gc.clippath) error_ = "SVG traces of clipped artists are not supported yet";
    events_.null();
}

void SvgTraceRenderer::drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                                const std::optional<std::vector<double>>& face) {
    events_.beginObject();
    events_.key("op").value("draw_path");
    events_.key("gc");
    writeSvgGc(gc);
    events_.key("path");
    writePath(path);
    events_.key("transform");
    writeMatrix(affine);
    events_.key("face");
    if (face) writeNumbers(*face);
    else events_.null();
    events_.key("clip_transform");
    writeClipTransform(gc);
    events_.endObject();
}

void SvgTraceRenderer::drawMarkers(const GC& gc, const Path& markerPath, const transforms::Matrix& markerTrans,
                                   const Path& path, const transforms::Matrix& trans,
                                   const std::optional<std::vector<double>>& face) {
    events_.beginObject();
    events_.key("op").value("draw_markers");
    events_.key("gc");
    writeSvgGc(gc);
    events_.key("marker_path");
    writePath(markerPath);
    events_.key("marker_transform");
    writeMatrix(markerTrans);
    events_.key("path");
    writePath(path);
    events_.key("transform");
    writeMatrix(trans);
    events_.key("face");
    if (face) writeNumbers(*face);
    else events_.null();
    events_.key("clip_transform");
    writeClipTransform(gc);
    events_.endObject();
}

void SvgTraceRenderer::drawPathCollection(const GC& gc, const PathCollectionArgs& a) {
    events_.beginObject();
    events_.key("op").value("draw_path_collection");
    events_.key("gc");
    writeSvgGc(gc);
    writeCollectionFields(a);
    events_.key("urls").beginArray();
    for (const auto& u : a.urls) writeOptionalText(u);
    events_.endArray();
    std::vector<double> h;
    for (const auto& x : a.hatchcolors) h.insert(h.end(), x.begin(), x.end());
    events_.key("hatchcolors");
    writeArray({a.hatchcolors.size(), 4}, h);
    events_.key("clip_transform");
    writeClipTransform(gc);
    events_.endObject();
}

void SvgTraceRenderer::drawText(const GC& gc, double x, double y, const std::string& s, const FontSpec& font,
                                double angle, bool ismath, const TextOptions& mtext) {
    events_.beginObject();
    events_.key("op").value("draw_text");
    events_.key("gc");
    writeSvgGc(gc);
    events_.key("x").value(x);
    events_.key("y").value(y);
    events_.key("s").value(s);
    events_.key("fonts").beginArray();
    for (const auto& f : font.files) events_.value(f);
    events_.endArray();
    events_.key("size").value(font.size);
    events_.key("angle").value(angle);
    events_.key("ismath").value(ismath);
    events_.key("features");
    if (mtext.features) {
        events_.beginArray();
        for (const auto& f : *mtext.features) events_.value(f);
        events_.endArray();
    } else {
        events_.null();
    }
    events_.key("language");
    writeOptionalText(mtext.language);
    events_.key("clip_transform");
    writeClipTransform(gc);
    if (ismath) {
        // record_svg's "math": the layout TextToPath draws, at FONT_SCALE and 72 dpi
        text::FontProperties scaled = font.props;
        scaled.size = 100.0;
        std::string error;
        const auto parse = text::parseMath(fonts_, s, 72.0, scaled, rc_, &error);
        if (!parse) error_ = error;
        events_.key("math").beginObject().key("glyphs").beginArray();
        if (parse)
            for (const auto& g : parse->glyphs) {
                events_.beginObject();
                events_.key("font").value(g.file);
                events_.key("size").value(g.fontsize);
                events_.key("index").value(static_cast<long long>(g.index));
                events_.key("ox").value(g.ox);
                events_.key("oy").value(g.oy);
                events_.endObject();
            }
        events_.endArray().key("rects").beginArray();
        if (parse)
            for (const auto& r : parse->rects) writeNumbers({r[0], r[1], r[2], r[3]});
        events_.endArray().endObject();
    }
    events_.endObject();
}

TextMetrics SvgTraceRenderer::textWidthHeightDescent(const std::string& s, const FontSpec& font, bool ismath) {
    constexpr double kFontScale = 100.0;  // TextToPath.FONT_SCALE, at TextToPath.DPI = 72
    const double scale = font.size / kFontScale;
    if (ismath) {
        text::FontProperties scaled = font.props;
        scaled.size = kFontScale;
        std::string error;
        const auto parse = text::parseMath(fonts_, s, 72.0, scaled, rc_, &error);
        if (!parse) {
            error_ = error;
            return {};
        }
        return {parse->width * scale, parse->height * scale, parse->depth * scale};
    }
    std::string error;
    const auto e = text::textExtent(fonts_, font.files, s, kFontScale, 72.0, FT_LOAD_NO_HINTING, std::nullopt, error,
                                    locale_);
    if (!error.empty()) error_ = error;
    return {e.width * scale, e.height * scale, e.descent * scale};
}

std::string SvgTraceRenderer::trace() {
    support::JsonWriter out;
    out.beginObject();
    out.key("version").value(1);
    out.key("matplotlib").value("3.11.2");
    out.key("locale").value(locale_);
    out.key("metadata").beginObject();
    for (const auto& [key, value] : metadata_) {
        out.key(key);
        if (value) out.value(*value);
        else out.null();
    }
    out.endObject();
    out.key("hashsalt").value(hashsalt_);
    out.key("svg_id");
    if (rc_.value("svg.id").isString()) out.value(rc_.text("svg.id"));
    else out.null();
    out.key("image_inline").value(rc_.boolean("svg.image_inline"));
    out.key("fonttype").value(rc_.text("svg.fonttype"));
    out.key("events").raw(events());
    out.endObject();
    return out.text();
}

}  // namespace calculate_plot::engine
