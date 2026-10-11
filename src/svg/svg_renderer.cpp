// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_svg.py (RendererSVG),
// lib/matplotlib/backend_bases.py (the path collection helpers), lib/matplotlib/textpath.py
// (TextToPath for plain text) and lib/matplotlib/colors.py (rgb2hex).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "svg/svg_renderer.h"

#include "agg/path_text.h"
#include "png/png.hpp"
#include "support/base64.hpp"
#include "support/pyformat.hpp"
#include "support/sha256.hpp"
#include "text/text_render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace calculate_plot::svg {

using support::numpyRepr;
using support::pythonRepr;
using support::shortFloat;

namespace {

const char* const kProlog =
    "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n"
    "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
    "  \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";

std::string css(const std::vector<std::pair<std::string, std::string>>& attrib) {
    std::string out;
    for (const auto& [k, v] : attrib) out += (out.empty() ? "" : "; ") + k + ": " + v;
    return out;
}

// _generate_transform: identity scales, translations and rotations are left out.
struct TransformPart {
    std::string type;
    std::vector<double> values;
};
std::string generateTransform(const std::vector<TransformPart>& parts) {
    std::string out;
    for (const auto& p : parts) {
        const auto& v = p.values;
        if ((p.type == "scale" && ((v.size() == 1 && v[0] == 1) || (v.size() == 2 && v[0] == 1 && v[1] == 1))) ||
            (p.type == "translate" && v.size() == 2 && v[0] == 0 && v[1] == 0) ||
            (p.type == "rotate" && v.size() == 1 && v[0] == 0))
            continue;
        std::string values;
        for (double x : v) values += (values.empty() ? "" : " ") + shortFloat(x);
        out += (out.empty() ? "" : " ") + p.type + "(" + values + ")";
    }
    return out;
}

agg::trans_affine toAgg(const Affine& a) {
    agg::trans_affine t;
    t.sx = a.sx, t.shx = a.shx, t.tx = a.tx, t.shy = a.shy, t.sy = a.sy, t.ty = a.ty;
    return t;
}

// a + b in matplotlib: a, then b; the matrix np.dot(b, a). NumPy's matrix product accumulates
// each entry from +0.0, term by term (so a sum of negative zeros is +0).
double dot3(double b0, double a0, double b1, double a1, double b2, double a2) {
    double acc = 0.0;
    acc += b0 * a0;
    acc += b1 * a1;
    acc += b2 * a2;
    return acc;
}

Affine compose(const Affine& a, const Affine& b) {
    Affine r;
    r.sx = dot3(b.sx, a.sx, b.shx, a.shy, b.tx, 0.0);
    r.shx = dot3(b.sx, a.shx, b.shx, a.sy, b.tx, 0.0);
    r.tx = dot3(b.sx, a.tx, b.shx, a.ty, b.tx, 1.0);
    r.shy = dot3(b.shy, a.sx, b.sy, a.shy, b.ty, 0.0);
    r.sy = dot3(b.shy, a.shx, b.sy, a.sy, b.ty, 0.0);
    r.ty = dot3(b.shy, a.tx, b.sy, a.ty, b.ty, 1.0);
    return r;
}

// Affine2D.scale and .translate, element by element in place, as matplotlib does them.
Affine& scale(Affine& m, double sx, double sy) {
    m.sx *= sx, m.shx *= sx, m.tx *= sx;
    m.shy *= sy, m.sy *= sy, m.ty *= sy;
    return m;
}
Affine& translate(Affine& m, double tx, double ty) {
    m.tx += tx, m.ty += ty;
    return m;
}

mpl::PathIterator iterator(const PathData& p) {
    return mpl::PathIterator(p.vertices, p.codes, p.simplify, p.threshold);
}

// Transform.transform of an affine on a point, as matplotlib's C++ affine_transform does it.
std::array<double, 2> apply(const Affine& t, double x, double y) {
    double t0 = t.sx * x, t1 = t.shx * y;
    const double rx = t0 + t1 + t.tx;
    t0 = t.shy * x, t1 = t.sy * y;
    return {rx, t0 + t1 + t.ty};
}

std::string hex2(long long v) {
    char buffer[8];
    std::snprintf(buffer, sizeof buffer, "%02llx", v);
    return buffer;
}

std::string toHexId(int v) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%x", v);
    return buffer;
}

// urllib.parse.quote with its default safe characters ('/' and the unreserved ones).
std::string quote(const std::string& s) {
    static const char* const digits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
            c == '-' || c == '~' || c == '/')
            out.push_back(char(c));
        else
            out += std::string("%") + digits[c >> 4] + digits[c & 15];
    }
    return out;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
    return s;
}

std::string pyFloat(double v, bool numpy) { return numpy ? numpyRepr(v) : pythonRepr(v); }

std::string pyTuple(const std::vector<std::string>& items) {
    std::string out = "(";
    for (std::size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + items[i];
    return out + (items.size() == 1 ? ",)" : ")");
}

std::string pyColour(const std::optional<std::vector<double>>& c, bool numpy) {
    if (!c) return "None";
    std::vector<std::string> items;
    for (double v : *c) items.push_back(pyFloat(v, numpy));
    return pyTuple(items);
}

}  // namespace

std::string pythonStrRepr(const std::string& s) {
    const bool single = s.find('\'') != std::string::npos && s.find('"') == std::string::npos;
    const char q = single ? '"' : '\'';
    std::string out(1, q);
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == q) out += std::string("\\") + char(q);
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20 || c == 0x7f) out += "\\x" + hex2(c);
        else out.push_back(char(c));  // UTF-8 bytes of printable characters pass through
    }
    return out + char(q);
}

std::string pythonBytesRepr(const std::string& bytes) {
    const bool single = bytes.find('\'') != std::string::npos && bytes.find('"') == std::string::npos;
    const char q = single ? '"' : '\'';
    std::string out = std::string("b") + q;
    for (unsigned char c : bytes) {
        if (c == '\\') out += "\\\\";
        else if (c == q) out += std::string("\\") + char(q);
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20 || c >= 0x7f) out += "\\x" + hex2(c);
        else out.push_back(char(c));
    }
    return out + char(q);
}

std::string rgb2hex(const std::vector<double>& c) {
    std::string out = "#";
    for (std::size_t i = 0; i < 3 && i < c.size(); ++i) out += hex2(text::pythonRound(c[i] * 255));
    return out;
}

RendererSVG::RendererSVG(double width, double height, double imageDpi, const Metadata& metadata,
                         std::string hashsalt, std::optional<std::string> svgId, text::FontCache& fonts,
                         std::string language)
    : writer_(out_), width_(width), height_(height), imageDpi_(imageDpi), hashsalt_(std::move(hashsalt)),
      fonts_(fonts), language_(std::move(language)) {
    const std::string w = shortFloat(width), h = shortFloat(height);
    out_ += kProlog;
    startId_ = writer_.start("svg", {{"xmlns:xlink", "http://www.w3.org/1999/xlink"},
                                     {"width", w + "pt"},
                                     {"height", h + "pt"},
                                     {"viewBox", "0 0 " + w + " " + h},
                                     {"xmlns", "http://www.w3.org/2000/svg"},
                                     {"version", "1.1"},
                                     {"id", svgId.value_or("")}});
    writeMetadata(metadata);
    writeDefaultStyle();
}

void RendererSVG::writeMetadata(const Metadata& given) {
    // Add metadata following the Dublin Core Metadata Initiative, and the Creative Commons Rights
    // Expression Language. This is mainly for compatibility with Inkscape.
    auto metadata = given.entries;
    auto setDefault = [&](const std::string& key, const std::string& value) {
        if (!metadata.count(key)) metadata[key] = std::vector<std::string>{value};
    };
    setDefault("Format", "image/svg+xml");
    setDefault("Type", "http://purl.org/dc/dcmitype/StillImage");
    // matplotlib names itself here; calculate-plot never claims to be matplotlib.
    setDefault("Creator", "calculate-plot");
    if (!metadata.count("Date")) {
        error = "the SVG metadata needs an explicit Date (or None): calculate-plot writes no clock time";
        metadata["Date"] = std::nullopt;
    }
    auto value = [&](const std::string& key) -> std::optional<std::vector<std::string>> {
        auto it = metadata.find(key);
        if (it == metadata.end()) return std::nullopt;
        auto v = it->second;
        metadata.erase(it);
        return v;
    };
    if (metadata.count("Title") && metadata["Title"]) writer_.element("title", metadata["Title"]->at(0));
    if (metadata.count("Date") && metadata["Date"]) {
        std::string joined;
        for (const auto& d : *metadata["Date"]) joined += (joined.empty() ? "" : "/") + d;
        metadata["Date"] = std::vector<std::string>{joined};
    }
    int mid = -1;
    auto ensure = [&]() {
        if (mid >= 0) return;
        mid = writer_.start("metadata");
        writer_.start("rdf:RDF", {{"xmlns:dc", "http://purl.org/dc/elements/1.1/"},
                                  {"xmlns:cc", "http://creativecommons.org/ns#"},
                                  {"xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"}});
        writer_.start("cc:Work");
    };
    if (auto uri = value("Type")) {
        ensure();
        writer_.element("dc:type", "", {{"rdf:resource", uri->at(0)}});
    }
    for (const char* key : {"Title", "Coverage", "Date", "Description", "Format", "Identifier", "Language", "Relation",
                            "Source"}) {
        if (auto info = value(key)) {
            ensure();
            std::string tag = std::string("dc:") + key;
            for (auto& ch : tag) ch = char(std::tolower(static_cast<unsigned char>(ch)));
            writer_.element(tag, info->at(0));
        }
    }
    for (const char* key : {"Creator", "Contributor", "Publisher", "Rights"}) {
        auto agents = value(key);
        if (!agents) continue;
        ensure();
        std::string tag = std::string("dc:") + key;
        for (auto& ch : tag) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        writer_.start(tag);
        for (const auto& agent : *agents) {
            writer_.start("cc:Agent");
            writer_.element("dc:title", agent);
            writer_.end("cc:Agent");
        }
        writer_.end(tag);
    }
    if (auto keywords = value("Keywords")) {
        ensure();
        writer_.start("dc:subject");
        writer_.start("rdf:Bag");
        for (const auto& k : *keywords) writer_.element("rdf:li", k);
        writer_.end("rdf:Bag");
        writer_.end("dc:subject");
    }
    if (mid >= 0) writer_.close(mid);
    for (const auto& [key, v] : metadata)
        if (v) error = "Unknown metadata key passed to SVG writer: " + key;
}

void RendererSVG::writeDefaultStyle() {
    const std::string defaultStyle = css({{"stroke-linejoin", "round"}, {"stroke-linecap", "butt"}});
    writer_.start("defs");
    writer_.element("style", "*{" + defaultStyle + "}", {{"type", "text/css"}});
    writer_.end("defs");
}

std::string RendererSVG::makeId(const std::string& type, const std::string& content) {
    hashedContents.push_back(content);
    support::Sha256 m;
    m.update(hashsalt_);
    m.update(content);
    return type + m.hexDigest().substr(0, 10);
}

Affine RendererSVG::flip(const Affine& transform) const {
    Affine f;
    scale(f, 1, -1);
    translate(f, 0, height_);
    return compose(transform, f);
}

std::string RendererSVG::convertPath(const PathData& path, const std::optional<Affine>& transform, bool clip,
                                     bool simplify, const std::optional<std::array<double, 3>>& sketch) {
    agg::rect_d clipRect(0.0, 0.0, 0.0, 0.0);
    if (clip) clipRect = agg::rect_d(0.0, 0.0, width_, height_);
    agg::trans_affine trans = transform ? toAgg(*transform) : agg::trans_affine();
    SketchParams sk{0.0, 0.0, 0.0};
    if (sketch) sk = SketchParams{(*sketch)[0], (*sketch)[1], (*sketch)[2]};
    mpl::PathIterator it = iterator(path);
    std::string buffer;
    static const std::array<std::string, 5> codes{"M", "L", "Q", "C", "z"};
    if (!convert_to_string(it, trans, clipRect, simplify, sk, 6, codes, false, buffer)) error = "Malformed path codes";
    return buffer;
}

std::vector<std::pair<std::string, std::string>> RendererSVG::styleDict(const Gc& gc, const std::optional<Face>& face) {
    std::vector<std::pair<std::string, std::string>> attrib;
    const bool forcedAlpha = gc.forcedAlpha;
    if (gc.hatch) {
        attrib.emplace_back("fill", "url(#" + hatch(gc, face) + ")");
        if (face && face->rgba.size() == 4 && face->rgba[3] != 1.0 && !forcedAlpha)
            attrib.emplace_back("fill-opacity", shortFloat(face->rgba[3]));
    } else {
        if (!face) {
            attrib.emplace_back("fill", "none");
        } else {
            const auto& f = face->rgba;
            if (!(f[0] == 0 && f[1] == 0 && f[2] == 0)) attrib.emplace_back("fill", rgb2hex(f));
            if (f.size() == 4 && f[3] != 1.0 && !forcedAlpha) attrib.emplace_back("fill-opacity", shortFloat(f[3]));
        }
    }
    if (forcedAlpha && gc.alpha != 1.0) attrib.emplace_back("opacity", shortFloat(gc.alpha));
    if (gc.dashes) {
        std::string seq;
        for (double v : *gc.dashes) seq += (seq.empty() ? "" : ",") + shortFloat(v);
        attrib.emplace_back("stroke-dasharray", seq);
        attrib.emplace_back("stroke-dashoffset", shortFloat(gc.dashOffset));
    }
    if (gc.linewidth != 0) {
        attrib.emplace_back("stroke", rgb2hex(gc.rgb));
        if (!forcedAlpha && gc.rgb.size() == 4 && gc.rgb[3] != 1.0)
            attrib.emplace_back("stroke-opacity", shortFloat(gc.rgb[3]));
        if (gc.linewidth != 1.0) attrib.emplace_back("stroke-width", shortFloat(gc.linewidth));
        if (gc.joinstyle != "round") attrib.emplace_back("stroke-linejoin", gc.joinstyle);
        if (gc.capstyle != "butt")
            attrib.emplace_back("stroke-linecap", gc.capstyle == "projecting" ? "square" : gc.capstyle);
    }
    return attrib;
}

std::string RendererSVG::style(const Gc& gc, const std::optional<Face>& face) { return css(styleDict(gc, face)); }

std::string RendererSVG::hatch(const Gc& gc, const std::optional<Face>& face) {
    const std::vector<double> edge = gc.hatchColor.value_or(std::vector<double>{0, 0, 0, 1});
    std::optional<std::vector<double>> faceColour;
    if (face) faceColour = face->rgba;
    // Patches keep the hatch pattern as a NumPy string (np.str_('/')), collections as a str.
    const std::string hatchText =
        gc.hatchNumpy ? "np.str_(" + pythonStrRepr(*gc.hatch) + ")" : pythonStrRepr(*gc.hatch);
    const std::string key = pyTuple({hatchText, pyColour(faceColour, face && face->numpy),
                                     pyColour(edge, false), pythonRepr(gc.hatchLinewidth)});
    for (const auto& h : hatches_)
        if (h.key == key) return h.id;
    const std::string id = makeId("h", key);
    hatches_.push_back(Hatch{key, gc.hatchPath, faceColour, edge, gc.hatchLinewidth, id});
    return id;
}

void RendererSVG::writeHatches() {
    if (hatches_.empty()) return;
    const int size = 72;  // HATCH_SIZE
    writer_.start("defs");
    for (const auto& h : hatches_) {
        writer_.start("pattern", {{"id", h.id},
                                  {"patternUnits", "userSpaceOnUse"},
                                  {"x", "0"},
                                  {"y", "0"},
                                  {"width", std::to_string(size)},
                                  {"height", std::to_string(size)}});
        Affine t;
        scale(t, size, size);
        scale(t, 1.0, -1.0);
        translate(t, 0, size);
        const std::string pathData = convertPath(h.path, t, false, false);
        const std::string fill = h.face ? rgb2hex(*h.face) : "none";
        writer_.element("rect", "", {{"x", "0"}, {"y", "0"}, {"width", std::to_string(size + 1)},
                                     {"height", std::to_string(size + 1)}, {"fill", fill}});
        std::vector<std::pair<std::string, std::string>> hatchStyle{{"fill", rgb2hex(h.edge)},
                                                                    {"stroke", rgb2hex(h.edge)},
                                                                    {"stroke-width", pythonRepr(h.linewidth)},
                                                                    {"stroke-linecap", "butt"},
                                                                    {"stroke-linejoin", "miter"}};
        if (h.edge.size() == 4 && h.edge[3] < 1) hatchStyle.emplace_back("stroke-opacity", pythonRepr(h.edge[3]));
        writer_.element("path", "", {{"d", pathData}, {"style", css(hatchStyle)}});
        writer_.end("pattern");
    }
    writer_.end("defs");
}

Attributes RendererSVG::clipAttrs(const Gc& gc) {
    std::string key;
    std::optional<std::array<double, 4>> rect;
    if (gc.clippath) {
        key = "(" + std::to_string(gc.clippath->index) + ", " + pythonStrRepr(gc.clippath->transformText) + ")";
    } else if (gc.cliprect) {
        const auto& r = *gc.cliprect;
        const double x = r[0], w = r[2] - r[0], h = r[3] - r[1];
        const double y = height_ - (r[1] + h);
        rect = std::array<double, 4>{x, y, w, h};
        key = pyTuple({numpyRepr(x), numpyRepr(y), numpyRepr(w), numpyRepr(h)});
    } else {
        return {};
    }
    for (const auto& c : clips_)
        if (c.key == key) return {{"clip-path", "url(#" + c.id + ")"}};
    Clip clip{key, rect, {}, {}, makeId("p", key)};
    if (gc.clippath) {
        clip.path = gc.clippath->path;
        clip.transform = flip(gc.clippath->transform);
    }
    clips_.push_back(clip);
    return {{"clip-path", "url(#" + clip.id + ")"}};
}

void RendererSVG::writeClips() {
    if (clips_.empty()) return;
    writer_.start("defs");
    for (const auto& c : clips_) {
        writer_.start("clipPath", {{"id", c.id}});
        if (!c.rect) {
            writer_.element("path", "", {{"d", convertPath(c.path, c.transform, false, false)}});
        } else {
            const auto& r = *c.rect;
            writer_.element("rect", "", {{"x", shortFloat(r[0])}, {"y", shortFloat(r[1])}, {"width", shortFloat(r[2])},
                                         {"height", shortFloat(r[3])}});
        }
        writer_.end("clipPath");
    }
    writer_.end("defs");
}

void RendererSVG::openGroup(const std::string& s, const std::optional<std::string>& gid) {
    if (gid && !gid->empty()) {
        writer_.start("g", {{"id", *gid}});
    } else {
        const int n = ++groups_[s];
        writer_.start("g", {{"id", s + "_" + std::to_string(n)}});
    }
}

void RendererSVG::closeGroup() { writer_.end("g"); }

void RendererSVG::drawPath(const Gc& gc, const PathData& path, const Affine& transform,
                           const std::optional<Face>& face) {
    const Affine transAndFlip = flip(transform);
    const bool clip = !face && gc.hatchPath.size() == 0;
    const bool simplify = path.simplify && clip;
    const std::string pathData = convertPath(path, transAndFlip, clip, simplify, gc.sketch);
    if (gc.url) writer_.start("a", {{"xlink:href", *gc.url}, {"target", "_blank"}});
    Attributes attrib{{"d", pathData}};
    for (const auto& a : clipAttrs(gc)) attrib.push_back(a);
    attrib.emplace_back("style", style(gc, face));
    writer_.element("path", "", attrib);
    if (gc.url) writer_.end("a");
}

void RendererSVG::drawMarkers(const Gc& gc, const PathData& markerPath, const Affine& markerTrans,
                              const PathData& path, const Affine& trans, const std::optional<Face>& face) {
    if (path.size() == 0) return;
    Affine flipY;
    scale(flipY, 1.0, -1.0);
    const std::string pathData = convertPath(markerPath, compose(markerTrans, flipY), false, false);
    const auto styleItems = styleDict(gc, face);
    const std::string key = pyTuple({pythonStrRepr(pathData), pythonStrRepr(css(styleItems))});
    std::vector<std::pair<std::string, std::string>> strokeStyle;
    for (const auto& [k, v] : styleItems)
        if (k.rfind("stroke", 0) == 0) strokeStyle.emplace_back(k, v);
    std::string oid;
    if (auto it = markers_.find(key); it != markers_.end()) {
        oid = it->second;
    } else {
        oid = makeId("m", key);
        writer_.start("defs");
        writer_.element("path", "", {{"id", oid}, {"d", pathData}, {"style", css(strokeStyle)}});
        writer_.end("defs");
        markers_[key] = oid;
    }
    writer_.start("g", clipAttrs(gc));
    if (gc.url) writer_.start("a", {{"xlink:href", *gc.url}, {"target", "_blank"}});
    // path.iter_segments(trans_and_flip, clip=(0, 0, width*72, height*72), simplify=False)
    mpl::PathIterator it = iterator(path);
    agg::trans_affine t = toAgg(flip(trans));
    std::vector<double> vertices;
    std::vector<unsigned char> codes;
    cleanup_path(it, t, true, true, agg::rect_d(0.0, 0.0, width_ * 72, height_ * 72), SNAP_FALSE, 1.0, false, true,
                 SketchParams{0.0, 0.0, 0.0}, vertices, codes);
    static const int verticesForCode[] = {1, 1, 1, 2, 3};
    const std::string markerStyle = style(gc, face);
    for (std::size_t i = 0; i < codes.size(); ++i) {
        const unsigned code = codes[i];
        if (code == agg::path_cmd_stop) break;
        const std::size_t extra = (code < 5 ? std::size_t(verticesForCode[code]) : 1) - 1;
        i += extra;
        const double x = vertices[2 * i], y = vertices[2 * i + 1];
        writer_.element("use", "", {{"xlink:href", "#" + oid}, {"x", shortFloat(x)}, {"y", shortFloat(y)},
                                    {"style", markerStyle}});
    }
    if (gc.url) writer_.end("a");
    writer_.end("g");
}

void RendererSVG::drawPathCollection(const Gc& gc, const Collection& c) {
    // _iter_collection_raw_paths: each path with its transform, then the master transform.
    std::vector<std::pair<PathData, Affine>> rawPaths;
    const std::size_t nPaths = c.paths.size(), nTransforms = c.transforms.size();
    for (std::size_t i = 0, n = std::max(nPaths, nTransforms); nPaths && i < n; ++i)
        rawPaths.emplace_back(c.paths[i % nPaths],
                              nTransforms ? compose(c.transforms[i % nTransforms], c.masterTransform) : c.masterTransform);

    // _iter_collection_uses_per_path, and whether defining each path once pays off.
    const std::size_t lenPath = nPaths ? c.paths[0].size() : 0;
    std::size_t usesPerPath = 0;
    if (nPaths && !(c.facecolors.empty() && c.edgecolors.empty())) {
        const std::size_t nPathIds = std::max(nPaths, nTransforms);
        const std::size_t n = std::max(nPathIds, c.offsets.size());
        usesPerPath = (n + nPathIds - 1) / nPathIds;
    }
    const bool optimize = lenPath + 9 * usesPerPath + 3 < (lenPath + 5) * usesPerPath;

    std::vector<std::string> pathIds;
    if (optimize) {
        writer_.start("defs");
        for (std::size_t i = 0; i < rawPaths.size(); ++i) {
            Affine t = rawPaths[i].second;
            scale(t, 1.0, -1.0);
            const std::string d = convertPath(rawPaths[i].first, t, false, false);
            const std::string oid = "C" + toHexId(pathCollectionId_) + "_" + toHexId(int(i)) + "_" + makeId("", d);
            writer_.element("path", "", {{"id", oid}, {"d", d}});
            pathIds.push_back(oid);
        }
        writer_.end("defs");
    }

    // _iter_collection
    const std::size_t nIds = rawPaths.size(), nOffsets = c.offsets.size();
    const std::size_t n = std::max(nIds, nOffsets);
    if (!((c.facecolors.empty() && c.edgecolors.empty() && c.hatchcolors.empty()) || nIds == 0)) {
        Gc gc0 = gc;
        if (c.edgecolors.empty()) gc0.linewidth = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            double xo = 0, yo = 0;
            if (nOffsets) {
                const auto p = apply(c.offsetTransform, c.offsets[i % nOffsets][0], c.offsets[i % nOffsets][1]);
                xo = p[0], yo = p[1];
            }
            if (!(std::isfinite(xo) && std::isfinite(yo))) continue;
            if (!c.edgecolors.empty()) {
                if (!c.linewidths.empty()) gc0.linewidth = c.linewidths[i % c.linewidths.size()];
                if (!c.linestyles.empty()) {
                    const auto& ls = c.linestyles[i % c.linestyles.size()];
                    // gc0.set_dashes(*ls): no value may be negative, and one must be positive
                    bool positive = !ls.second || ls.second->empty();
                    for (double d : ls.second ? *ls.second : std::vector<double>{}) {
                        if (d < 0.0) error = "All values in the dash list must be non-negative";
                        positive = positive || d > 0.0;
                    }
                    if (error.empty() && !positive) error = "At least one value in the dash list must be positive";
                    if (!error.empty()) return;
                    gc0.dashOffset = ls.first;
                    gc0.dashes = ls.second;
                }
                const auto& ec = c.edgecolors[i % c.edgecolors.size()];
                if (ec[3] == 0.0) {
                    gc0.linewidth = 0;
                } else if (gc0.forcedAlpha) {  // set_foreground(ec, isRGBA=True)
                    gc0.rgb = {ec[0], ec[1], ec[2], gc0.alpha};
                } else {
                    gc0.rgb = ec;
                }
            }
            if (!c.hatchcolors.empty()) gc0.hatchColor = c.hatchcolors[i % c.hatchcolors.size()];
            std::optional<Face> fc;
            if (!c.facecolors.empty()) {
                const auto& f = c.facecolors[i % c.facecolors.size()];
                if (!(f.size() == 4 && f[3] == 0)) fc = Face{f, true};
            }
            if (!c.antialiaseds.empty()) gc0.antialiased = c.antialiaseds[i % c.antialiaseds.size()];
            if (!c.urls.empty()) gc0.url = c.urls[i % c.urls.size()];

            if (!optimize) {  // RendererBase.draw_path_collection: one path each
                Affine t = rawPaths[i % nIds].second;
                if (xo != 0 || yo != 0) translate(t, xo, yo);
                drawPath(gc0, rawPaths[i % nIds].first, t, fc);
                continue;
            }
            if (gc0.url) writer_.start("a", {{"xlink:href", *gc0.url}, {"target", "_blank"}});
            const Attributes clip = clipAttrs(gc0);
            if (!clip.empty()) writer_.start("g", clip);
            writer_.element("use", "", {{"xlink:href", "#" + pathIds[i % nIds]}, {"x", shortFloat(xo)},
                                        {"y", shortFloat(height_ - yo)}, {"style", style(gc0, fc)}});
            if (!clip.empty()) writer_.end("g");
            if (gc0.url) writer_.end("a");
        }
    }
    if (optimize) ++pathCollectionId_;
}

void RendererSVG::drawGouraudTriangle(const std::array<std::array<double, 2>, 3>& points,
                                      const std::array<std::array<double, 4>, 3>& colors) {
    // Three overlapping linear gradients over a solid triangle of the average colour.
    std::vector<double> avg(4);
    for (int k = 0; k < 4; ++k) avg[k] = (colors[0][k] + colors[1][k] + colors[2][k]) / 3.0;
    if (avg[3] == 0) return;  // Skip fully-transparent triangles
    writer_.start("defs");
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    for (int i = 0; i < 3; ++i) {
        x1 = points[i][0], y1 = points[i][1];
        x2 = points[(i + 1) % 3][0], y2 = points[(i + 1) % 3][1];
        x3 = points[(i + 2) % 3][0], y3 = points[(i + 2) % 3][1];
        const auto& rgba = colors[i];
        double xb, yb;
        if (x2 == x3) {
            xb = x2, yb = y1;
        } else if (y2 == y3) {
            xb = x1, yb = y2;
        } else {
            const double m1 = (y2 - y3) / (x2 - x3);
            const double b1 = y2 - (m1 * x2);
            const double m2 = -(1.0 / m1);
            const double b2 = y1 - (m2 * x1);
            xb = (-b1 + b2) / (m1 - m2);
            yb = m2 * xb + b2;
        }
        writer_.start("linearGradient", {{"id", "GR" + toHexId(gradients_) + "_" + std::to_string(i)},
                                         {"gradientUnits", "userSpaceOnUse"},
                                         {"x1", shortFloat(x1)},
                                         {"y1", shortFloat(y1)},
                                         {"x2", shortFloat(xb)},
                                         {"y2", shortFloat(yb)}});
        writer_.element("stop", "", {{"offset", "1"},
                                     {"style", css({{"stop-color", rgb2hex(avg)}, {"stop-opacity", shortFloat(rgba[3])}})}});
        writer_.element("stop", "", {{"offset", "0"},
                                     {"style", css({{"stop-color", rgb2hex({rgba[0], rgba[1], rgba[2], rgba[3]})},
                                                    {"stop-opacity", "0"}})}});
        writer_.end("linearGradient");
    }
    writer_.end("defs");
    const std::string dpath = "M " + shortFloat(x1) + "," + shortFloat(y1) + " L " + shortFloat(x2) + "," +
                              shortFloat(y2) + " " + shortFloat(x3) + "," + shortFloat(y3) + " Z";
    writer_.element("path", "", {{"d", dpath}, {"fill", rgb2hex(avg)}, {"fill-opacity", "1"},
                                 {"shape-rendering", "crispEdges"}});
    writer_.start("g", {{"stroke", "none"}, {"stroke-width", "0"}, {"shape-rendering", "crispEdges"},
                        {"filter", "url(#colorMat)"}});
    const std::string gr = "url(#GR" + toHexId(gradients_) + "_";
    writer_.element("path", "", {{"d", dpath}, {"fill", gr + "0)"}, {"shape-rendering", "crispEdges"}});
    writer_.element("path", "", {{"d", dpath}, {"fill", gr + "1)"}, {"filter", "url(#colorAdd)"},
                                 {"shape-rendering", "crispEdges"}});
    writer_.element("path", "", {{"d", dpath}, {"fill", gr + "2)"}, {"filter", "url(#colorAdd)"},
                                 {"shape-rendering", "crispEdges"}});
    writer_.end("g");
    ++gradients_;
}

void RendererSVG::drawGouraudTriangles(const Gc& gc, const std::vector<std::array<std::array<double, 2>, 3>>& triangles,
                                       const std::vector<std::array<std::array<double, 4>, 3>>& colors,
                                       const Affine& transform) {
    writer_.start("g", clipAttrs(gc));
    const Affine transAndFlip = flip(transform);
    if (!hasGouraud_) {
        hasGouraud_ = true;
        writer_.start("filter", {{"id", "colorAdd"}});
        writer_.element("feComposite", "", {{"in", "SourceGraphic"}, {"in2", "BackgroundImage"},
                                            {"operator", "arithmetic"}, {"k2", "1"}, {"k3", "1"}});
        writer_.end("filter");
        // feColorMatrix filter to correct opacity
        writer_.start("filter", {{"id", "colorMat"}});
        writer_.element("feColorMatrix", "", {{"type", "matrix"},
                                              {"values", "1 0 0 0 0 \n0 1 0 0 0 \n0 0 1 0 0 \n1 1 1 1 0 \n0 0 0 0 1 "}});
        writer_.end("filter");
    }
    for (std::size_t t = 0; t < triangles.size() && t < colors.size(); ++t) {
        std::array<std::array<double, 2>, 3> points;
        for (int i = 0; i < 3; ++i) points[i] = apply(transAndFlip, triangles[t][i][0], triangles[t][i][1]);
        drawGouraudTriangle(points, colors[t]);
    }
    writer_.end("g");
}

void RendererSVG::drawImage(const Gc& gc, double x, double y, int w, int h, const std::vector<unsigned char>& rgba,
                            const std::optional<Affine>& transform) {
    if (w == 0 || h == 0) return;
    const Attributes clip = clipAttrs(gc);
    // Can't apply clip-path directly to the image because the image has a transformation.
    if (!clip.empty()) writer_.start("g", clip);
    if (gc.url) writer_.start("a", {{"xlink:href", *gc.url}, {"target", "_blank"}});
    Attributes attrib;
    // svg.image_inline: the PNG is embedded (written by calculate-plot's encoder, not Pillow).
    const std::string png = png::encodePng(png::Image{w, h, rgba}, {});
    const std::string oid = gc.gid && !gc.gid->empty() ? *gc.gid : makeId("image", pythonBytesRepr(png));
    attrib.emplace_back("xlink:href", "data:image/png;base64,\n" + support::encodeBase64(png));
    attrib.emplace_back("id", oid);
    if (!transform) {
        const double ww = 72.0 * w / imageDpi_, hh = 72.0 * h / imageDpi_;
        attrib.emplace_back("transform", generateTransform({{"scale", {1, -1}}, {"translate", {0, -hh}}}));
        attrib.emplace_back("x", shortFloat(x));
        attrib.emplace_back("y", shortFloat(-(height_ - y - hh)));
        attrib.emplace_back("width", shortFloat(ww));
        attrib.emplace_back("height", shortFloat(hh));
        writer_.element("image", "", attrib);
    } else {
        if (gc.alpha != 1.0) attrib.emplace_back("opacity", shortFloat(gc.alpha));
        Affine s;
        scale(s, 1.0 / w, 1.0 / h);
        Affine last;
        translate(last, x, y);
        scale(last, 1, -1);
        translate(last, 0, height_);
        const Affine flipped = compose(compose(s, *transform), last);
        attrib.emplace_back("width", shortFloat(w));
        attrib.emplace_back("height", shortFloat(h));
        attrib.emplace_back("transform", generateTransform({{"matrix", {flipped.sx, flipped.shy, flipped.shx, flipped.sy,
                                                                        flipped.tx, flipped.ty}}}));
        attrib.emplace_back("style", "image-rendering:crisp-edges;image-rendering:pixelated");
        writer_.element("image", "", attrib);
    }
    if (gc.url) writer_.end("a");
    if (!clip.empty()) writer_.end("g");
}

void RendererSVG::updateGlyphMapDefs(const std::vector<std::pair<std::string, PathData>>& glyphs) {
    // Emit definitions for not-yet-defined glyphs, and record them as having been defined.
    if (glyphs.empty()) return;
    writer_.start("defs");
    for (const auto& [repr, path] : glyphs) {
        // x64 to go back to FreeType's internal (integral) units.
        PathData scaled = path;
        auto v = std::make_shared<std::vector<double>>(*path.vertices);
        for (double& value : *v) value *= 64;
        scaled.vertices = v;
        scaled.simplify = false;
        writer_.element("path", "", {{"id", replaceAll(repr, "%20", "_")},
                                     {"d", convertPath(scaled, std::nullopt, false, false)},
                                     {"transform", generateTransform({{"scale", {1.0 / 64}}})}});
        glyphMap_.push_back(repr);
    }
    writer_.end("defs");
}

namespace {

// FT2Font.get_path() of the glyph in the font's slot, as a Path.
PathData glyphPath(FT2Font& font) {
    std::vector<double> vertices;
    std::vector<unsigned char> codes;
    font.get_path(vertices, codes);
    PathData p;
    p.vertices = std::make_shared<std::vector<double>>(std::move(vertices));
    p.codes = std::make_shared<std::vector<unsigned char>>(std::move(codes));
    return p;
}

std::string glyphRepr(FT2Font& font, unsigned index) {
    const char* name = FT_Get_Postscript_Name(font.get_face());
    char hex[16];
    std::snprintf(hex, sizeof hex, "%x", index);
    return quote(std::string(name ? name : "") + "-" + hex);
}

}  // namespace

void RendererSVG::drawTextAsPath(const Gc& gc, double x, double y, const std::string& s,
                                 const std::vector<std::string>& fonts, double size, double angle,
                                 const std::optional<std::vector<std::string>>& features,
                                 const std::optional<std::string>& language) {
    const Attributes clip = clipAttrs(gc);
    if (!clip.empty()) writer_.start("g", clip);
    if (gc.url) writer_.start("a", {{"xlink:href", *gc.url}, {"target", "_blank"}});
    writer_.comment(s);
    const std::string color = rgb2hex(gc.rgb);
    std::vector<std::pair<std::string, std::string>> styleItems;
    if (color != "#000000") styleItems.emplace_back("fill", color);
    const double alpha = gc.forcedAlpha ? gc.alpha : (gc.rgb.size() == 4 ? gc.rgb[3] : 1.0);
    if (alpha != 1) styleItems.emplace_back("opacity", shortFloat(alpha));
    const double fontScale = size / 100.0;  // TextToPath.FONT_SCALE
    writer_.start("g", {{"style", css(styleItems)},
                        {"transform", generateTransform({{"translate", {x, y}},
                                                         {"rotate", {-angle}},
                                                         {"scale", {fontScale, -fontScale}}})}});
    // TextToPath._get_font and get_glyphs_with_font, unhinted at 100 pt, 72 dpi.
    FT2Font* font = fonts_.get(fonts);
    if (!font) {
        error = fonts_.error;
        return;
    }
    font->set_size(100.0, 72.0);
    const std::u32string cps = text::codepoints(s);
    const FT2Font::LanguageType languages =
        std::vector<FT2Font::LanguageRange>{{language.value_or(language_), 0, int(cps.size())}};
    const auto items = font->layout_items(cps, FT_LOAD_NO_HINTING, features, languages);
    if (!font->error.empty()) error = font->error;
    std::vector<std::pair<std::string, PathData>> fresh;
    std::vector<std::tuple<std::string, double, double>> uses;
    for (const auto& item : items) {
        item.ft_object->load_glyph(FT_UInt(item.glyph_index), FT_LOAD_NO_HINTING);
        const std::string repr = glyphRepr(*item.ft_object, unsigned(item.glyph_index));
        uses.emplace_back(repr, item.x, item.y);
        const bool known = std::find(glyphMap_.begin(), glyphMap_.end(), repr) != glyphMap_.end();
        bool already = false;
        for (auto& [r, p] : fresh)
            if (r == repr) {
                p = glyphPath(*item.ft_object);
                already = true;
            }
        if (!known && !already) fresh.emplace_back(repr, glyphPath(*item.ft_object));
    }
    updateGlyphMapDefs(fresh);
    for (const auto& [repr, xp, yp] : uses)
        writer_.element("use", "", {{"xlink:href", "#" + repr},
                                    {"transform", generateTransform({{"translate", {xp, yp}}, {"scale", {1.0}}})}});
    writer_.end("g");
    if (gc.url) writer_.end("a");
    if (!clip.empty()) writer_.end("g");
}

void RendererSVG::drawMathtextAsPath(const Gc& gc, double x, double y, const std::string& s, double size, double angle,
                                     const std::vector<Glyph>& glyphs, const std::vector<std::array<double, 4>>& rects) {
    const Attributes clip = clipAttrs(gc);
    if (!clip.empty()) writer_.start("g", clip);
    if (gc.url) writer_.start("a", {{"xlink:href", *gc.url}, {"target", "_blank"}});
    writer_.comment(s);
    const std::string color = rgb2hex(gc.rgb);
    std::vector<std::pair<std::string, std::string>> styleItems;
    if (color != "#000000") styleItems.emplace_back("fill", color);
    const double alpha = gc.forcedAlpha ? gc.alpha : (gc.rgb.size() == 4 ? gc.rgb[3] : 1.0);
    if (alpha != 1) styleItems.emplace_back("opacity", shortFloat(alpha));
    const double fontScale = size / 100.0;
    writer_.start("g", {{"style", css(styleItems)},
                        {"transform", generateTransform({{"translate", {x, y}},
                                                         {"rotate", {-angle}},
                                                         {"scale", {fontScale, -fontScale}}})}});
    // TextToPath.get_glyphs_mathtext
    std::vector<std::pair<std::string, PathData>> fresh;
    std::vector<std::tuple<std::string, double, double, double>> uses;
    for (const auto& g : glyphs) {
        FT2Font* font = fonts_.byPath(g.font);
        if (!font) {
            error = fonts_.error;
            return;
        }
        const std::string repr = glyphRepr(*font, g.index);
        const bool known = std::find(glyphMap_.begin(), glyphMap_.end(), repr) != glyphMap_.end();
        if (!known) {
            font->clear();
            font->set_size(100.0, 72.0);
            font->load_glyph(g.index, FT_LOAD_NO_HINTING);
            bool already = false;
            for (auto& [r, p] : fresh)
                if (r == repr) {
                    p = glyphPath(*font);
                    already = true;
                }
            if (!already) fresh.emplace_back(repr, glyphPath(*font));
        }
        uses.emplace_back(repr, g.ox, g.oy, g.size / 100.0);
    }
    updateGlyphMapDefs(fresh);
    for (const auto& [repr, xp, yp, sc] : uses)
        writer_.element("use", "", {{"xlink:href", "#" + replaceAll(repr, "%20", "_")},
                                    {"transform", generateTransform({{"translate", {xp, yp}}, {"scale", {sc}}})}});
    for (const auto& r : rects) {
        const double ox = r[0], oy = r[1], w = r[2], h = r[3];
        PathData p;
        p.vertices = std::make_shared<std::vector<double>>(
            std::vector<double>{ox, oy, ox, oy + h, ox + w, oy + h, ox + w, oy, ox, oy, 0, 0});
        p.codes = std::make_shared<std::vector<unsigned char>>(std::vector<unsigned char>{1, 2, 2, 2, 2, 79});
        writer_.element("path", "", {{"d", convertPath(p, std::nullopt, false, false)}});
    }
    writer_.end("g");
    if (gc.url) writer_.end("a");
    if (!clip.empty()) writer_.end("g");
}

std::string RendererSVG::finalize() {
    writeClips();
    writeHatches();
    writer_.close(startId_);
    return out_;
}

}  // namespace calculate_plot::svg
