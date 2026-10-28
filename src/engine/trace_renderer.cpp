#include "engine/trace_renderer.hpp"

#include "engine/hatch.hpp"
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

void TraceRenderer::writeImage(const RgbaImage& im) {
    events_.beginObject();
    events_.key("shape").beginArray().value(static_cast<long long>(im.height)).value(static_cast<long long>(im.width)).value(4LL).endArray();
    const std::string b64 = support::encodeBase64(std::string_view(reinterpret_cast<const char*>(im.rgba.data()), im.rgba.size()));
    std::string lines;
    for (std::size_t k = 0; k < b64.size(); k += 16) lines += (k ? "\n" : "") + b64.substr(k, 16);
    events_.key("base64").value(lines);
    events_.endObject();
}

void TraceRenderer::writeTriangles(const std::vector<std::array<std::array<double, 2>, 3>>& triangles,
                                   const std::vector<std::array<std::array<double, 4>, 3>>& colors) {
    std::vector<double> points, rgba;
    for (const auto& t : triangles)
        for (const auto& p : t) points.insert(points.end(), p.begin(), p.end());
    for (const auto& c : colors)
        for (const auto& v : c) rgba.insert(rgba.end(), v.begin(), v.end());
    events_.key("points");
    writeArray({triangles.size(), 3, 2}, points);
    events_.key("colors");
    writeArray({colors.size(), 3, 4}, rgba);
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
    // get_hatch_path(): Path.hatch(hatch, 6.0); the artists set a hatch only when it is not empty
    events_.key("hatch");
    if (gc.hatch && !gc.hatch->empty()) {
        if (const auto hatch = hatchPath(*gc.hatch, rc_)) writePath(*hatch);
        else events_.null();
    } else {
        events_.null();
    }
    events_.key("hatch_color");
    if (gc.hatchColor) writeNumbers(*gc.hatchColor);
    else events_.null();
    events_.key("hatch_linewidth").value(gc.hatchLinewidth);
    events_.key("sketch");
    if (gc.sketch) writeNumbers({(*gc.sketch)[0], (*gc.sketch)[1], (*gc.sketch)[2]});
    else events_.null();
}

void TraceRenderer::writeColour(const std::vector<double>& rgba, bool intAlpha) {
    // an alpha that is a Python int is written as a JSON integer (the SVG id hashes tell them apart)
    if (!intAlpha || rgba.size() != 4 || rgba[3] != double(static_cast<long long>(rgba[3]))) return writeNumbers(rgba);
    events_.beginArray();
    for (std::size_t i = 0; i < 3; ++i) events_.value(rgba[i]);
    events_.value(static_cast<long long>(rgba[3]));
    events_.endArray();
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
    const std::vector<Path> chunks = aggChunks(path, face.has_value(), gc.hatch.has_value(), rc_);
    if (chunks.empty()) return drawWhole(gc, path, affine, face);
    for (const Path& chunk : chunks) drawWhole(gc, chunk, affine, face);
}

void AggTraceRenderer::drawWhole(const GC& gc, const Path& path, const transforms::Matrix& affine,
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

void AggTraceRenderer::drawImage(const GC& gc, double x, double y, const RgbaImage& im,
                                 const std::optional<transforms::Matrix>& transform) {
    if (transform) return unsupported("draw_image with a transform");  // (RendererAgg has no option_scale_image)
    events_.beginObject();
    events_.key("op").value("draw_image");
    events_.key("gc");
    writeGc(gc);
    events_.key("x").value(x);
    events_.key("y").value(y);
    events_.key("image");
    writeImage(im);
    events_.endObject();
}

void AggTraceRenderer::drawQuadMesh(const GC& gc, const transforms::Matrix& master, int meshWidth, int meshHeight,
                                    const std::vector<std::array<double, 2>>& coordinates,
                                    const std::vector<std::array<double, 2>>& offsets, const transforms::Matrix& offsetTrans,
                                    const std::vector<std::array<double, 4>>& facecolors, bool antialiased,
                                    const std::vector<std::array<double, 4>>& edgecolors) {
    const auto flat = [](const auto& rows) {
        std::vector<double> out;
        for (const auto& r : rows) out.insert(out.end(), r.begin(), r.end());
        return out;
    };
    events_.beginObject();
    events_.key("op").value("draw_quad_mesh");
    events_.key("gc");
    writeGc(gc);
    events_.key("master_transform");
    writeMatrix(master);
    events_.key("mesh_width").value(static_cast<long long>(meshWidth));
    events_.key("mesh_height").value(static_cast<long long>(meshHeight));
    events_.key("coordinates");
    writeArray({std::size_t(meshHeight + 1), std::size_t(meshWidth + 1), 2}, flat(coordinates));
    events_.key("offsets");
    writeArray({offsets.size(), 2}, flat(offsets));
    events_.key("offset_transform");
    writeMatrix(offsetTrans);
    events_.key("facecolors");
    writeArray({facecolors.size(), 4}, flat(facecolors));
    events_.key("antialiased").value(antialiased);
    events_.key("edgecolors");
    writeArray({edgecolors.size(), 4}, flat(edgecolors));
    events_.endObject();
}

void AggTraceRenderer::drawGouraudTriangles(const GC& gc, const std::vector<std::array<std::array<double, 2>, 3>>& triangles,
                                            const std::vector<std::array<std::array<double, 4>, 3>>& colors,
                                            const transforms::Matrix& transform) {
    events_.beginObject();
    events_.key("op").value("draw_gouraud_triangles");
    events_.key("gc");
    writeGc(gc);
    writeTriangles(triangles, colors);
    events_.key("transform");
    writeMatrix(transform);
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
    events_.key("boxes").beginArray();
    for (const auto& b : boxes) writeNumbers({b[0], b[1], b[2], b[3]});
    events_.endArray();
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
        // in lines of 16 characters, as record.py writes them
        const std::string b64 = support::encodeBase64(
            std::string_view(reinterpret_cast<const char*>(image.buffer.data()), image.buffer.size()));
        std::string lines;
        for (std::size_t k = 0; k < b64.size(); k += 16) lines += (k ? "\n" : "") + b64.substr(k, 16);
        events_.key("base64").value(lines);
        events_.endObject();
        events_.key("x").value(image.x);
        events_.key("y").value(image.y);
        events_.key("angle").value(0.0);
        events_.key("gc");
        writeGc(gc);
        events_.endObject();
    }

    // The boxes (the rules of mathtext): filled paths with a new context that keeps the snap.
    std::vector<double> rgba = gc.rgb;
    if (gc.forcedAlpha) rgba[3] = gc.alpha;  // (a colour here always has its four parts)
    GC gc1(rc_);
    gc1.linewidth = 0;
    gc1.snap = gc.snap;
    for (auto b : boxes) {  // dx, dy (upwards), width, height
        if (!gc1.snap || *gc1.snap) {
            // Prevent thin bars from disappearing by growing symmetrically.
            if (b[2] < 1) {
                b[0] -= (1 - b[2]) / 2;
                b[2] = 1;
            }
            if (b[3] < 1) {
                b[1] -= (1 - b[3]) / 2;
                b[3] = 1;
            }
        }
        // Path._create_closed: the first corner again, closing
        const Path path = Path::make(
            {b[0], b[1], b[0] + b[2], b[1], b[0] + b[2], b[1] + b[3], b[0], b[1] + b[3], b[0], b[1]}, {}, rc_, true);
        drawPath(gc1, path, transforms::Affine2D().rotateDeg(angle).translate(x, height_ - y).matrix(), rgba);
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
    const auto e = text::textExtent(fonts_, font.files, s, font.size, dpi_, *flags, error, locale_);
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
    // str() of the clip path's transform tree with RendererSVG's flip (_make_flip_transform), which the clip
    // path's id hashes
    if (!gc.clippath) {
        events_.null();
        return;
    }
    auto flip = std::make_shared<transforms::Affine2D>();
    flip->scale(1, -1).translate(0, height());
    const auto text = transforms::compose(gc.clippath->affine, flip)->str();
    if (!text) {
        error_ = "SVG traces of artists clipped through this transform are not supported yet";
        events_.null();
        return;
    }
    events_.value(*text);
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
    if (face) writeColour(*face, gc.faceAlphaInt);
    else events_.null();
    events_.key("clip_transform");
    writeClipTransform(gc);
    if (face && gc.faceNumpy) events_.key("face_numpy").value(true);
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

void SvgTraceRenderer::drawGouraudTriangles(const GC& gc, const std::vector<std::array<std::array<double, 2>, 3>>& triangles,
                                            const std::vector<std::array<std::array<double, 4>, 3>>& colors,
                                            const transforms::Matrix& transform) {
    events_.beginObject();
    events_.key("op").value("draw_gouraud_triangles");
    events_.key("gc");
    writeSvgGc(gc);
    writeTriangles(triangles, colors);
    events_.key("transform");
    writeMatrix(transform);
    events_.key("clip_transform");
    writeClipTransform(gc);
    events_.endObject();
}

bool SvgTraceRenderer::optionImageNocomposite() const { return !rc_.boolean("image.composite_image"); }

void SvgTraceRenderer::drawImage(const GC& gc, double x, double y, const RgbaImage& im,
                                 const std::optional<transforms::Matrix>& transform) {
    events_.beginObject();
    events_.key("op").value("draw_image");
    events_.key("gc");
    writeSvgGc(gc);
    events_.key("x").value(x);
    events_.key("y").value(y);
    events_.key("image");
    writeImage(im);
    events_.key("transform");
    if (transform) writeMatrix(*transform);
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
    const auto e = text::textExtent(fonts_, font.files, s, kFontScale, 72.0, FT_LOAD_NO_HINTING, error,
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
