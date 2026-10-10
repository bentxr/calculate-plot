#include "engine/trace_renderer.hpp"

#include "rc/rc_params.hpp"

namespace calculate_plot::engine {

TraceRenderer::TraceRenderer(double width, double height, double dpi, const rc::Params& rc, std::string locale)
    : width_(width), height_(height), dpi_(dpi), rc_(rc), locale_(std::move(locale)) {
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
    if (face) writeNumbers(*face);
    else events_.null();
    events_.endObject();
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
                                   std::string locale, SvgMetadata metadata, std::string hashsalt)
    : TraceRenderer(width, height, 72.0, rc, std::move(locale)),
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
