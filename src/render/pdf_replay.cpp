#include "render/pdf_replay.hpp"

#include "pdf/pdf_file.h"
#include "render/trace_reader.hpp"
#include "support/json.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

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

std::optional<std::vector<double>> optionalNumbers(Reader& in, const Json& j) {
    if (j.isNull()) return std::nullopt;
    return in.numbers(j);
}

std::optional<long long> optionalInteger(Reader& in, const Json& j) {
    if (j.isNull()) return std::nullopt;
    return in.integer(j);
}

// A drawing call's graphics context, as record.py's pdf_gc writes it.
pdf::DrawGc graphicsContext(Reader& in, const Json& g) {
    pdf::DrawGc gc;
    pdf::GcState& s = gc.state;
    s.linewidth = in.number(in.field(g, "linewidth"));
    s.alpha = in.number(in.field(g, "alpha"));
    s.forcedAlpha = in.boolean(in.field(g, "forced_alpha"));
    if (auto rgb = optionalNumbers(in, in.field(g, "rgb"))) s.rgb = *rgb;
    else gc.rgbIsNone = true;
    s.capstyle = in.text(in.field(g, "capstyle"));
    s.joinstyle = in.text(in.field(g, "joinstyle"));
    const Json& dashes = in.field(g, "dashes");
    if (!dashes.isArray() || dashes.size() != 2) {
        in.fail("dashes must be [offset, pattern]");
    } else {
        s.dashOffset = in.number(dashes[0]);
        s.dashes = optionalNumbers(in, dashes[1]);
    }
    if (auto r = optionalNumbers(in, in.field(g, "cliprect"))) {
        if (r->size() != 4) in.fail("a clip rectangle needs 4 numbers");
        else s.cliprect = std::array<double, 4>{(*r)[0], (*r)[1], (*r)[2], (*r)[3]};
    }
    s.cliprectId = optionalInteger(in, in.field(g, "cliprect_id"));
    const Json& clip = in.field(g, "clippath");
    if (!clip.isNull()) {
        s.clippath = path(in, in.field(clip, "path"));
        s.clipTransform = affine(in, in.field(clip, "transform"));
    }
    s.clippathId = optionalInteger(in, in.field(g, "clippath_id"));
    if (s.cliprect.has_value() != s.cliprectId.has_value() || s.clippath.has_value() != s.clippathId.has_value())
        in.fail("a clip without its identity");
    const Json& hatch = in.field(g, "hatch_string");
    if (!hatch.isNull()) s.hatch = in.text(hatch);
    s.hatchColor = optionalNumbers(in, in.field(g, "hatch_color"));
    s.hatchLinewidth = in.number(in.field(g, "hatch_linewidth"));
    gc.hasHatchPath = !in.field(g, "hatch").isNull();
    if (auto sk = optionalNumbers(in, in.field(g, "sketch"))) {
        if (sk->size() != 3) in.fail("sketch parameters need 3 numbers");
        else gc.sketch = std::array<double, 3>{(*sk)[0], (*sk)[1], (*sk)[2]};
    }
    const Json& url = in.field(g, "url");
    if (!url.isNull()) gc.url = in.text(url);
    return gc;
}

std::optional<std::vector<double>> face(Reader& in, const Json& j) {
    auto v = optionalNumbers(in, j);
    if (v && v->size() != 3 && v->size() != 4) in.fail("a colour must have 3 or 4 components");
    return v;
}

// _create_pdf_info_dict: the defaults (Creator, Producer, CreationDate) updated by the metadata,
// then the entries that are None dropped. calculate-plot writes no date (the bytes would change
// from run to run) and does not name matplotlib as the creator: the metadata must set those three.
pdf::Dict infoDict(Reader& in, const Json& metadata) {
    if (!metadata.isObject()) {
        in.fail("the metadata must be a dictionary");
        return {};
    }
    std::vector<std::pair<std::string, const Json*>> info{{"Creator", nullptr}, {"Producer", nullptr},
                                                          {"CreationDate", nullptr}};
    for (const auto& [key, value] : metadata.members()) {
        bool found = false;
        for (auto& entry : info)
            if (entry.first == key) entry.second = &value, found = true;
        if (!found) info.emplace_back(key, &value);
    }
    pdf::Dict out;
    for (const auto& [key, value] : info) {
        if (!value) {
            in.fail("the PDF metadata must set " + key + " (to a value or None)");
            return {};
        }
        if (value->isNull()) continue;
        if (key == "Trapped" && value->isString()) {
            out.emplace_back(key, pdf::reprName(value->asString()));
        } else if (value->isString()) {
            out.emplace_back(key, pdf::reprString(value->asString()));
        } else {
            in.fail("the PDF metadata " + key + " must be a string or None (dates are not supported)");
            return {};
        }
    }
    return out;
}

}  // namespace

PdfReplayResult replayPdfTrace(std::string_view traceJson, const std::string& fontDirectory) {
    (void)fontDirectory;  // (text comes later)
    PdfReplayResult result;
    support::JsonError jsonError;
    const auto trace = support::parseJson(traceJson, &jsonError);
    if (!trace || !trace->isObject()) {
        result.error = "the trace is not valid JSON: " + jsonError.message;
        return result;
    }
    Reader in;
    if (in.integer(in.field(*trace, "version")) != 1) in.fail("unsupported trace version");
    if (in.boolean(in.field(*trace, "use14corefonts"))) in.fail("pdf.use14corefonts is not supported");
    const long long compression = in.integer(in.field(*trace, "compression"));
    if (compression < 0 || compression > 9) in.fail("pdf.compression must be from 0 to 9");
    const bool inheritColor = in.boolean(in.field(*trace, "inheritcolor"));
    const Json& events = in.field(*trace, "events");
    if (!events.isArray() || events.size() == 0 || in.text(in.field(events[0], "op")) != "begin")
        in.fail("a trace starts with its 'begin' event");
    pdf::Dict info = infoDict(in, in.field(*trace, "metadata"));
    if (!in.error.empty()) {
        result.error = in.error;
        return result;
    }
    const Json& begin = events[0];
    pdf::PdfFile file(int(compression), info);
    // print_pdf: the page, then the renderer
    file.newPage(in.number(in.field(begin, "width")), in.number(in.field(begin, "height")));
    pdf::RendererPdf r(file, in.number(in.field(begin, "image_dpi")), in.number(in.field(begin, "hatch_linewidth")),
                       inheritColor);
    for (std::size_t i = 1; i < events.size() && in.error.empty() && r.error.empty(); ++i) {
        const Json& e = events[i];
        const std::string op = in.text(in.field(e, "op"));
        if (op == "draw_path") {
            pdf::DrawGc gc = graphicsContext(in, in.field(e, "gc"));
            const svg::PathData p = path(in, in.field(e, "path"));
            const svg::Affine t = affine(in, in.field(e, "transform"));
            const auto f = face(in, in.field(e, "face"));
            if (in.error.empty()) r.drawPath(gc, p, t, f);
        } else {
            in.fail(op + " is not supported in PDF yet");
        }
    }
    if (in.error.empty() && r.error.empty()) {
        r.finalize();
        file.finalize();
    }
    result.error = !in.error.empty() ? in.error : !r.error.empty() ? r.error : file.error;
    if (result.error.empty()) result.pdf = file.bytes();
    return result;
}

}  // namespace calculate_plot::render
