// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_pdf.py (pdfRepr, _fill, Name, Stream,
// PdfFile, RendererPdf, GraphicsContextPdf) and lib/matplotlib/backend_bases.py (GraphicsContextBase's state).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "pdf/pdf_file.h"

#include "agg/path_text.h"
#include "engine/path.hpp"
#include "engine/text_path.hpp"
#include "support/pyformat.hpp"

#include <zlib.h>

#include <cmath>
#include <cstdio>

namespace calculate_plot::pdf {

namespace {

agg::trans_affine toAgg(const svg::Affine& a) {
    agg::trans_affine t;
    t.sx = a.sx, t.shx = a.shx, t.tx = a.tx, t.shy = a.shy, t.sy = a.sy, t.ty = a.ty;
    return t;
}

// The code points of UTF-8 text (it comes from a JSON parser: valid)
std::u32string codepoints(const std::string& s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        char32_t cp = extra == 0 ? c : extra == 1 ? (c & 0x1F) : extra == 2 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k <= extra && i + std::size_t(k) < s.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + std::size_t(k)]) & 0x3F);
        out.push_back(cp);
        i += std::size_t(extra) + 1;
    }
    return out;
}

// pdfRepr of bytes: in parentheses, with backslashes, parentheses and line ends escaped (_str_escapes)
std::string reprBytes(const std::string& bytes) {
    std::string out = "(";
    for (const char c : bytes) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '(': out += "\\("; break;
            case ')': out += "\\)"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c;
        }
    }
    return out + ")";
}

}  // namespace

std::string reprNumber(double x, std::string* error) {
    if (!std::isfinite(x)) {
        if (error && error->empty()) *error = "Can only output finite numbers in PDF";
        return "0";
    }
    std::string r = support::fixed(x, 10);
    while (!r.empty() && r.back() == '0') r.pop_back();
    if (!r.empty() && r.back() == '.') r.pop_back();
    return r;
}

std::string reprInt(long long x) { return std::to_string(x); }

std::string pathOperations(const svg::PathData& path, const svg::Affine& transform,
                           std::optional<std::array<double, 4>> clip, bool simplify,
                           const std::optional<std::array<double, 3>>& sketch, std::string* error) {
    agg::rect_d clipRect(0.0, 0.0, 0.0, 0.0);
    if (clip) clipRect = agg::rect_d((*clip)[0], (*clip)[1], (*clip)[2], (*clip)[3]);
    agg::trans_affine trans = toAgg(transform);
    SketchParams sk{0.0, 0.0, 0.0};
    if (sketch) sk = SketchParams{(*sketch)[0], (*sketch)[1], (*sketch)[2]};
    mpl::PathIterator it(path.vertices, path.codes, path.simplify, path.threshold);
    std::string buffer;
    static const std::array<std::string, 5> codes{"m", "l", "", "c", "h"};
    if (!convert_to_string(it, trans, clipRect, simplify, sk, 6, codes, true, buffer) && error && error->empty())
        *error = "Malformed path codes";
    return buffer;
}

std::array<double, 4> pathExtents(const svg::PathData& path, const svg::Affine& transform) {
    // transform.transform_path(self), then the extents of the vertices drawn (curves by their extrema)
    auto vertices = std::make_shared<std::vector<double>>();
    if (path.vertices)
        for (std::size_t i = 0; i + 1 < path.vertices->size(); i += 2) {
            const auto p = svg::applyAffine(transform, (*path.vertices)[i], (*path.vertices)[i + 1]);
            vertices->push_back(p[0]);
            vertices->push_back(p[1]);
        }
    engine::Path p;
    p.vertices = vertices;
    p.codes = path.codes;
    p.shouldSimplify = path.simplify;
    p.simplifyThreshold = path.threshold;
    return engine::pathExtents(p);
}

std::string reprName(const std::string& name) {
    // Name._hexify: every byte outside '!' ... '~' as #xx
    std::string out = "/";
    for (const char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= '!' && c <= '~') {
            out += ch;
        } else {
            char hex[4];
            std::snprintf(hex, sizeof hex, "#%02x", c);
            out += hex;
        }
    }
    return out;
}

std::string reprString(const std::string& utf8) {
    bool ascii = true;
    for (const char c : utf8) ascii = ascii && static_cast<unsigned char>(c) < 0x80;
    if (ascii) return reprBytes(utf8);
    // codecs.BOM_UTF16_BE + obj.encode('UTF-16BE')
    std::string bytes = "\xFE\xFF";
    const auto unit = [&](unsigned u) { bytes += char(u >> 8), bytes += char(u & 0xFF); };
    for (char32_t cp : codepoints(utf8)) {
        if (cp >= 0x10000) {
            cp -= 0x10000;
            unit(0xD800 + unsigned(cp >> 10));
            unit(0xDC00 + unsigned(cp & 0x3FF));
        } else {
            unit(unsigned(cp));
        }
    }
    return reprBytes(bytes);
}

std::string reprRef(int id) { return std::to_string(id) + " 0 R"; }

std::string fill(const std::vector<std::string>& strings, std::size_t linelen) {
    // Make one string from the strings, with whitespace in between: lines of at most linelen
    // characters, if possible.
    std::size_t currpos = 0, lasti = 0;
    std::vector<std::string> result;
    const auto join = [&](std::size_t from, std::size_t to) {
        std::string line;
        for (std::size_t k = from; k < to; ++k) line += (k > from ? " " : "") + strings[k];
        return line;
    };
    for (std::size_t i = 0; i < strings.size(); ++i) {
        const std::size_t length = strings[i].size();
        if (currpos + length < linelen) {
            currpos += length + 1;
        } else {
            result.push_back(join(lasti, i));
            lasti = i;
            currpos = length;
        }
    }
    result.push_back(join(lasti, strings.size()));
    std::string out;
    for (std::size_t k = 0; k < result.size(); ++k) out += (k ? "\n" : "") + result[k];
    return out;
}

std::string reprList(const std::vector<std::string>& items) {
    std::vector<std::string> tokens{"["};
    tokens.insert(tokens.end(), items.begin(), items.end());
    tokens.push_back("]");
    return fill(tokens);
}

std::string reprDict(const Dict& items) {
    std::vector<std::string> tokens{"<<"};
    for (const auto& [key, value] : items) tokens.push_back(reprName(key) + " " + value);
    tokens.push_back(">>");
    return fill(tokens);
}

// Stream: a stream object, compressed as zlib.compressobj(level) compresses (Z_NO_FLUSH for each
// write, Z_FINISH at the end). Without a length object, its contents are kept until the end so
// that the length can be written inline.
struct PdfFile::Stream {
    int id;
    std::optional<int> len;
    Dict extra;
    bool compress = false;
    z_stream z{};
    bool buffered = false;
    std::string buffer;  // the contents, when buffered
    std::size_t pos = 0;

    ~Stream() {
        if (compress) deflateEnd(&z);
    }
    void deflateInto(std::string& out, const std::string& data, int flush) {
        z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        z.avail_in = uInt(data.size());
        unsigned char chunk[16384];
        do {
            z.next_out = chunk;
            z.avail_out = sizeof chunk;
            deflate(&z, flush);
            out.append(reinterpret_cast<const char*>(chunk), sizeof chunk - z.avail_out);
        } while (z.avail_out == 0 || (flush == Z_FINISH && z.avail_in > 0));
    }
};

PdfFile::PdfFile(int compression, Dict info) : compression_(compression), info_(std::move(info)) {
    fh_ += "%PDF-1.4\n";  // 1.4 is the first version to have alpha
    // some eight-bit characters in a comment, so that tools take the file for binary
    fh_ += "%\xAC\xDC \xAB\xBA\n";
    rootObject_ = reserveObject();
    pagesObject_ = reserveObject();
    fontObject_ = reserveObject();
    extGStateObject_ = reserveObject();
    hatchObject_ = reserveObject();
    gouraudObject_ = reserveObject();
    xObjectObject_ = reserveObject();
    resourceObject_ = reserveObject();
    writeObject(rootObject_, reprDict({{"Type", reprName("Catalog")}, {"Pages", reprRef(pagesObject_)}}));
    // every procset, as the PDF reference recommends
    const std::vector<std::string> procsets{reprName("PDF"), reprName("Text"), reprName("ImageB"), reprName("ImageC"),
                                            reprName("ImageI")};
    writeObject(resourceObject_, reprDict({{"Font", reprRef(fontObject_)},
                                           {"XObject", reprRef(xObjectObject_)},
                                           {"ExtGState", reprRef(extGStateObject_)},
                                           {"Pattern", reprRef(hatchObject_)},
                                           {"Shading", reprRef(gouraudObject_)},
                                           {"ProcSet", reprList(procsets)}}));
}

PdfFile::~PdfFile() = default;

int PdfFile::reserveObject() {
    xref_.push_back(std::nullopt);
    return int(xref_.size()) - 1;
}

void PdfFile::recordXref(int id) { xref_[std::size_t(id)] = fh_.size(); }

void PdfFile::writeObject(int id, const std::string& contents) {
    recordXref(id);
    fh_ += std::to_string(id) + " 0 obj\n" + contents + "\nendobj\n";
}

void PdfFile::newPage(double w, double h) {
    endStream();
    width = w, height = h;
    const int contentObject = reserveObject();
    const int annotsObject = reserveObject();
    const int pageObject = reserveObject();
    writeObject(pageObject, reprDict({{"Type", reprName("Page")},
                                      {"Parent", reprRef(pagesObject_)},
                                      {"Resources", reprRef(resourceObject_)},
                                      {"MediaBox", reprList({reprInt(0), reprInt(0), reprNumber(72 * w, &error),
                                                             reprNumber(72 * h, &error)})},
                                      {"Contents", reprRef(contentObject)},
                                      {"Annots", reprRef(annotsObject)}}));
    pageList_.push_back(pageObject);
    annotations_.push_back(annotsObject);
    beginStream(contentObject, reserveObject());
    // the PDF graphics state of matplotlib's default graphics context (colour spaces, join style)
    output({reprName("DeviceRGB"), "CS"});
    output({reprName("DeviceRGB"), "cs"});
    output({reprInt(1), "j"});
}

void PdfFile::write(const std::string& data) {
    if (!current_) {
        fh_ += data;
        return;
    }
    Stream& s = *current_;
    std::string& target = s.buffered ? s.buffer : fh_;
    if (s.compress) s.deflateInto(target, data, Z_NO_FLUSH);
    else target += data;
}

void PdfFile::output(const std::vector<std::string>& data) {
    write(fill(data));
    write("\n");
}

void PdfFile::beginStream(int id, std::optional<int> lengthObject, Dict extra) {
    current_ = std::make_unique<Stream>();
    Stream& s = *current_;
    s.id = id;
    s.len = lengthObject;
    s.extra = std::move(extra);
    recordXref(id);
    if (compression_) {
        s.compress = true;
        deflateInit2(&s.z, compression_, Z_DEFLATED, MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    }
    const auto header = [&](const std::string& length) {
        Dict d = s.extra;
        d.emplace_back("Length", length);
        if (compression_) d.emplace_back("Filter", reprName("FlateDecode"));
        fh_ += std::to_string(s.id) + " 0 obj\n" + reprDict(d) + "\nstream\n";
    };
    if (!s.len) {
        s.buffered = true;
    } else {
        header(reprRef(*s.len));
        s.pos = fh_.size();
    }
    (void)header;
}

void PdfFile::endStream() {
    if (!current_) return;
    Stream& s = *current_;
    if (s.compress) {
        s.deflateInto(s.buffered ? s.buffer : fh_, std::string(), Z_FINISH);
    }
    if (s.buffered) {
        Dict d = s.extra;
        d.emplace_back("Length", reprInt(static_cast<long long>(s.buffer.size())));
        if (compression_) d.emplace_back("Filter", reprName("FlateDecode"));
        fh_ += std::to_string(s.id) + " 0 obj\n" + reprDict(d) + "\nstream\n";
        fh_ += s.buffer;
        fh_ += "\nendstream\nendobj\n";
        current_.reset();
    } else {
        const std::size_t length = fh_.size() - s.pos;
        const int lengthObject = *s.len;
        fh_ += "\nendstream\nendobj\n";
        current_.reset();
        writeObject(lengthObject, reprInt(static_cast<long long>(length)));
    }
}

std::string PdfFile::alphaState(double strokeAlpha, double fillAlpha) {
    for (const auto& a : alphaStates_)
        if (a.stroke == strokeAlpha && a.fill == fillAlpha) return a.name;
    alphaStates_.push_back({strokeAlpha, fillAlpha, reprName("A" + std::to_string(alphaStates_.size() + 1))});
    return alphaStates_.back().name;
}

std::string PdfFile::markerObject(const svg::PathData& path, const svg::Affine& trans, bool fill, bool stroke,
                                  double lw, const std::string& joinstyle, const std::string& capstyle) {
    Marker m;
    m.pathops = pathOperations(path, trans, std::nullopt, false, std::nullopt, &error);
    m.fill = fill, m.stroke = stroke, m.joinstyle = joinstyle, m.capstyle = capstyle;
    m.name = reprName("M" + std::to_string(markers_.size()));
    m.ob = reserveObject();
    m.bbox = pathExtents(path, trans);
    m.lw = lw;
    markers_.push_back(m);
    return m.name;
}

std::string PdfFile::pathCollectionObject(const std::string& joinstyle, const std::string& capstyle,
                                          const svg::PathData& path, const svg::Affine& trans, double padding,
                                          bool filled, bool stroked) {
    PathTemplate t{reprName("P" + std::to_string(paths_.size())), path, trans, 0, joinstyle, capstyle, padding,
                   filled, stroked};
    t.ob = reserveObject();
    paths_.push_back(t);
    return t.name;
}

namespace {
int joinNumber(const std::string& j) { return j == "miter" ? 0 : j == "round" ? 1 : 2; }
int capNumber(const std::string& c) { return c == "butt" ? 0 : c == "round" ? 1 : 2; }
std::string paintPath(bool fill, bool stroke) { return stroke ? (fill ? "B" : "S") : (fill ? "f" : "n"); }
}  // namespace

void PdfFile::writeMarkers() {
    for (const auto& m : markers_) {
        // the box of the control points, padded by 5 line widths (half a miter at the default limit 10)
        const double p = m.lw * 5;
        const std::array<double, 4> b{m.bbox[0] - p, m.bbox[1] - p, m.bbox[2] + p, m.bbox[3] + p};
        beginStream(m.ob, std::nullopt,
                    {{"Type", reprName("XObject")},
                     {"Subtype", reprName("Form")},
                     {"BBox", reprList({reprNumber(b[0], &error), reprNumber(b[1], &error), reprNumber(b[2], &error),
                                        reprNumber(b[3], &error)})}});
        output({reprInt(joinNumber(m.joinstyle)), "j"});
        output({reprInt(capNumber(m.capstyle)), "J"});
        output({m.pathops});
        output({paintPath(m.fill, m.stroke)});
        endStream();
    }
}

void PdfFile::writePathCollectionTemplates() {
    for (const auto& t : paths_) {
        const std::string pathops = pathOperations(t.path, t.trans, std::nullopt, false, std::nullopt, &error);
        const auto e = pathExtents(t.path, t.trans);
        std::vector<std::string> extents;
        if (!(std::isfinite(e[0]) && std::isfinite(e[1]) && std::isfinite(e[2]) && std::isfinite(e[3]))) {
            extents = {reprInt(0), reprInt(0), reprInt(0), reprInt(0)};
        } else {
            const double p = t.padding;
            for (const double v : {e[0] - p, e[1] - p, e[2] + p, e[3] + p}) extents.push_back(reprNumber(v, &error));
        }
        beginStream(t.ob, std::nullopt,
                    {{"Type", reprName("XObject")}, {"Subtype", reprName("Form")}, {"BBox", reprList(extents)}});
        output({reprInt(joinNumber(t.joinstyle)), "j"});
        output({reprInt(capNumber(t.capstyle)), "J"});
        output({pathops});
        output({paintPath(t.filled, t.stroked)});
        endStream();
    }
}

void PdfFile::finalize() {
    endStream();
    for (const int annots : annotations_) writeObject(annots, reprList({}));
    writeObject(fontObject_, reprDict({}));  // (no fonts yet)
    Dict states;
    for (const auto& a : alphaStates_)
        states.emplace_back(a.name.substr(1), reprDict({{"Type", reprName("ExtGState")},
                                                        {"CA", reprNumber(a.stroke, &error)},
                                                        {"ca", reprNumber(a.fill, &error)}}));
    writeObject(extGStateObject_, reprDict(states));
    writeObject(hatchObject_, reprDict({}));    // (no hatches yet)
    writeObject(gouraudObject_, reprDict({}));  // (no Gouraud triangles yet)
    Dict xobjects;  // (no images yet)
    for (const auto& m : markers_) xobjects.emplace_back(m.name.substr(1), reprRef(m.ob));
    for (const auto& t : paths_) xobjects.emplace_back(t.name.substr(1), reprRef(t.ob));
    writeObject(xObjectObject_, reprDict(xobjects));
    writeMarkers();
    writePathCollectionTemplates();
    std::vector<std::string> kids;
    for (const int page : pageList_) kids.push_back(reprRef(page));
    writeObject(pagesObject_, reprDict({{"Type", reprName("Pages")},
                                        {"Kids", reprList(kids)},
                                        {"Count", reprInt(static_cast<long long>(pageList_.size()))}}));
    infoObject_ = reserveObject();
    writeObject(infoObject_, reprDict(info_));
    writeXref();
    writeTrailer();
}

void PdfFile::writeXref() {
    startxref_ = fh_.size();
    write("xref\n0 " + std::to_string(xref_.size()) + "\n");
    for (std::size_t i = 0; i < xref_.size(); ++i) {
        if (!xref_[i]) {
            error = "No offset for object " + std::to_string(i);
            return;
        }
        char line[32];
        std::snprintf(line, sizeof line, "%010llu %05d %s \n", static_cast<unsigned long long>(*xref_[i]),
                      i == 0 ? 65535 : 0, i == 0 ? "f" : "n");
        write(line);
    }
}

void PdfFile::writeTrailer() {
    write("trailer\n");
    write(reprDict({{"Size", reprInt(static_cast<long long>(xref_.size()))},
                    {"Root", reprRef(rootObject_)},
                    {"Info", reprRef(infoObject_)}}));
    write("\nstartxref\n" + std::to_string(startxref_) + "\n%%EOF\n");
}

// RendererPdf and its GraphicsContextPdf

RendererPdf::RendererPdf(PdfFile& file, double imageDpi, double hatchLinewidth, bool inheritColor)
    : file_(file), inheritColor_(inheritColor) {
    (void)imageDpi;
    gc_.hatchLinewidth = hatchLinewidth;
}

std::string RendererPdf::num(double x) { return reprNumber(x, &error); }

void RendererPdf::checkGc(DrawGc& gc, const std::optional<std::vector<double>>& fillcolor) {
    GcState& s = gc.state;
    s.fillcolor = fillcolor;
    if (gc.rgbIsNone) {
        // set_foreground((0, 0, 0, 0), isRGBA=True)
        s.rgb = s.forcedAlpha ? std::vector<double>{0, 0, 0, s.alpha} : std::vector<double>{0, 0, 0, 0};
        gc.rgbIsNone = false;
    }
    if (s.rgb.size() < 4) {
        error = "a colour of the graphics context without alpha is not supported in PDF";
        return;
    }
    if (s.forcedAlpha) s.effectiveAlphas = {s.alpha, s.alpha};
    else if (!fillcolor || fillcolor->size() < 4) s.effectiveAlphas = {s.rgb[3], 1.0};
    else s.effectiveAlphas = {s.rgb[3], (*fillcolor)[3]};
    const auto cmds = delta(s);
    if (!cmds.empty()) file_.output(cmds);
}

std::vector<std::string> RendererPdf::fillcolorCmd(const std::optional<std::vector<double>>& rgb) {
    if (!rgb || inheritColor_) return {};
    const auto& c = *rgb;
    if (c[0] == c[1] && c[1] == c[2]) return {num(c[0]), "g"};
    return {num(c[0]), num(c[1]), num(c[2]), "rg"};
}

std::vector<std::string> RendererPdf::clipCmd(const GcState& other) {
    // Set the clip: pop saved states until the right one or none is left, then push and clip.
    const auto same = [&] { return gc_.cliprectId == other.cliprectId && gc_.clippathId == other.clippathId; };
    std::vector<std::string> cmds;
    while (!same() && !parents_.empty()) {
        gc_ = parents_.back();
        parents_.pop_back();
        cmds.push_back("Q");
    }
    if (!same() || parents_.empty()) {
        parents_.push_back(gc_);
        cmds.push_back("q");
        if (gc_.cliprectId != other.cliprectId) {
            const auto& r = *other.cliprect;  // pdfRepr of a Bbox: its bounds
            cmds.push_back(fill({num(r[0]), num(r[1]), num(r[2] - r[0]), num(r[3] - r[1])}));
            cmds.insert(cmds.end(), {"re", "W", "n"});
        }
        if (gc_.clippathId != other.clippathId) {
            const auto ops = pathOperations(*other.clippath, other.clipTransform, std::nullopt, false, std::nullopt);
            cmds.insert(cmds.end(), ops.begin(), ops.end());
            cmds.insert(cmds.end(), {"W", "n"});
        }
    }
    return cmds;
}

std::vector<std::string> RendererPdf::delta(const GcState& other) {
    // GraphicsContextPdf.delta: the commands that take this state to `other`, group by group in
    // its order (the clip first, since it may pop), copying each group that differs.
    std::vector<std::string> cmds;
    bool fillPerformed = false;
    if (gc_.cliprectId != other.cliprectId || gc_.clippathId != other.clippathId) {
        const auto c = clipCmd(other);
        cmds.insert(cmds.end(), c.begin(), c.end());
        gc_.cliprect = other.cliprect, gc_.cliprectId = other.cliprectId;
        gc_.clippath = other.clippath, gc_.clipTransform = other.clipTransform, gc_.clippathId = other.clippathId;
    }
    if (gc_.alpha != other.alpha || gc_.forcedAlpha != other.forcedAlpha || gc_.effectiveAlphas != other.effectiveAlphas) {
        cmds.push_back(file_.alphaState(other.effectiveAlphas[0], other.effectiveAlphas[1]));
        cmds.push_back("gs");
        gc_.alpha = other.alpha, gc_.forcedAlpha = other.forcedAlpha, gc_.effectiveAlphas = other.effectiveAlphas;
    }
    if (gc_.capstyle != other.capstyle) {
        const int cap = other.capstyle == "butt" ? 0 : other.capstyle == "round" ? 1 : 2;
        cmds.insert(cmds.end(), {reprInt(cap), "J"});
        gc_.capstyle = other.capstyle;
    }
    if (gc_.fillcolor != other.fillcolor) {
        fillPerformed = true;
        const auto c = fillcolorCmd(other.fillcolor);
        cmds.insert(cmds.end(), c.begin(), c.end());
        gc_.fillcolor = other.fillcolor;
    }
    if (gc_.joinstyle != other.joinstyle) {
        const int join = other.joinstyle == "miter" ? 0 : other.joinstyle == "round" ? 1 : 2;
        cmds.insert(cmds.end(), {reprInt(join), "j"});
        gc_.joinstyle = other.joinstyle;
    }
    if (gc_.linewidth != other.linewidth) {
        cmds.insert(cmds.end(), {num(other.linewidth), "w"});
        gc_.linewidth = other.linewidth;
    }
    if (gc_.dashOffset != other.dashOffset || gc_.dashes != other.dashes) {
        std::vector<std::string> dash;
        double offset = other.dashOffset;
        if (other.dashes) {
            for (const double d : *other.dashes) dash.push_back(num(d));
        } else {
            offset = 0;
        }
        cmds.insert(cmds.end(), {reprList(dash), num(offset), "d"});
        gc_.dashOffset = other.dashOffset, gc_.dashes = other.dashes;
    }
    if (gc_.rgb != other.rgb) {
        if (!inheritColor_) {
            const auto& c = other.rgb;
            if (c[0] == c[1] && c[1] == c[2]) cmds.insert(cmds.end(), {num(c[0]), "G"});
            else cmds.insert(cmds.end(), {num(c[0]), num(c[1]), num(c[2]), "RG"});
        }
        gc_.rgb = other.rgb;
    }
    // (must come after the fill colour and rgb; done again when the fill colour changed)
    if (fillPerformed || gc_.hatch != other.hatch || gc_.hatchColor != other.hatchColor ||
        gc_.hatchLinewidth != other.hatchLinewidth) {
        if (!other.hatch || other.hatch->empty()) {
            if (gc_.fillcolor) {
                const auto c = fillcolorCmd(gc_.fillcolor);
                cmds.insert(cmds.end(), c.begin(), c.end());
            } else {
                cmds.insert(cmds.end(), {reprName("DeviceRGB"), "cs"});
            }
        } else if (error.empty()) {
            error = "hatches are not supported in PDF yet";
        }
        gc_.hatch = other.hatch, gc_.hatchColor = other.hatchColor, gc_.hatchLinewidth = other.hatchLinewidth;
    }
    return cmds;
}

std::vector<std::string> RendererPdf::pathOperations(const svg::PathData& path, const svg::Affine& transform,
                                                     std::optional<std::array<double, 4>> clip, bool simplify,
                                                     const std::optional<std::array<double, 3>>& sketch) {
    return {pdf::pathOperations(path, transform, clip, simplify, sketch, &error)};
}

std::string RendererPdf::paint() const {
    // Op.paint_path(fill(), stroke())
    const bool doFill = (gc_.hatch && !gc_.hatch->empty()) ||
                        (gc_.fillcolor && (gc_.fillcolor->size() <= 3 || (*gc_.fillcolor)[3] != 0.0));
    const bool doStroke = gc_.linewidth > 0 && gc_.alpha > 0 && (gc_.rgb.size() <= 3 || gc_.rgb[3] != 0.0);
    if (doStroke) return doFill ? "B" : "S";
    return doFill ? "f" : "n";
}

void RendererPdf::drawPath(DrawGc& gc, const svg::PathData& path, const svg::Affine& transform,
                           const std::optional<std::vector<double>>& rgbFace) {
    if (gc.url && error.empty()) error = "links in PDF are not supported yet";
    checkGc(gc, rgbFace);
    // writePath: clipped to the page and simplified as the path says when it is not filled
    std::optional<std::array<double, 4>> clip;
    bool simplify = false;
    if (!rgbFace && !gc.hasHatchPath) {
        clip = std::array<double, 4>{0.0, 0.0, file_.width * 72, file_.height * 72};
        simplify = path.simplify;
    }
    file_.output(pathOperations(path, transform, clip, simplify, gc.sketch));
    file_.output({paint()});
}

std::vector<std::string> RendererPdf::push() {
    parents_.push_back(gc_);
    return {"q"};
}

std::vector<std::string> RendererPdf::pop() {
    if (parents_.empty()) {
        if (error.empty()) error = "the PDF graphics state was restored more often than saved";
        return {};
    }
    gc_ = parents_.back();
    parents_.pop_back();
    return {"Q"};
}

namespace {

// Path.iter_segments(transform, clip=..., simplify=False): the last vertex of each segment
std::vector<std::array<double, 2>> segmentEnds(const svg::PathData& path, const svg::Affine& trans,
                                               std::optional<std::array<double, 4>> clip) {
    mpl::PathIterator it(path.vertices, path.codes, path.simplify, path.threshold);
    agg::trans_affine t = toAgg(trans);
    std::vector<double> vertices;
    std::vector<unsigned char> codes;
    const agg::rect_d rect = clip ? agg::rect_d((*clip)[0], (*clip)[1], (*clip)[2], (*clip)[3]) : agg::rect_d(0, 0, 0, 0);
    cleanup_path(it, t, true, clip.has_value() && rect.x1 < rect.x2 && rect.y1 < rect.y2, rect, SNAP_FALSE, 1.0, false,
                 true, SketchParams{0.0, 0.0, 0.0}, vertices, codes);
    static const int verticesForCode[] = {1, 1, 1, 2, 3};
    std::vector<std::array<double, 2>> ends;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        const unsigned code = codes[i];
        if (code == agg::path_cmd_stop) break;
        i += (code < 5 ? std::size_t(verticesForCode[code]) : 1) - 1;
        ends.push_back({vertices[2 * i], vertices[2 * i + 1]});
    }
    return ends;
}

svg::Affine translation(double x, double y) {
    svg::Affine t;
    t.tx = x, t.ty = y;
    return t;
}

}  // namespace

void RendererPdf::drawMarkers(DrawGc& gc, const svg::PathData& markerPath, const svg::Affine& markerTrans,
                              const svg::PathData& path, const svg::Affine& trans,
                              const std::optional<std::vector<double>>& rgbFace) {
    // drawn as one XObject used at each vertex, unless drawing the marker at each is shorter
    const std::size_t lenMarkerPath = markerPath.size(), uses = path.size();
    if (lenMarkerPath * uses < lenMarkerPath + uses + 5) {
        // RendererBase.draw_markers: a draw_path at each vertex (no clipping to the page)
        for (const auto& [x, y] : segmentEnds(path, trans, std::nullopt))
            drawPath(gc, markerPath, svg::composeAffine(markerTrans, translation(x, y)), rgbFace);
        return;
    }
    if (gc.url && error.empty()) error = "links in PDF are not supported yet";
    checkGc(gc, rgbFace);
    // gc.fill(rgbFace), gc.stroke() of the drawing's context (as check_gc left it)
    const GcState& s = gc.state;
    const bool fill = (s.hatch && !s.hatch->empty()) || (rgbFace && (rgbFace->size() <= 3 || (*rgbFace)[3] != 0.0));
    const bool stroke = s.linewidth > 0 && s.alpha > 0 && (s.rgb.size() <= 3 || s.rgb[3] != 0.0);
    const std::string marker =
        file_.markerObject(markerPath, markerTrans, fill, stroke, gc_.linewidth, s.joinstyle, s.capstyle);
    file_.output({"q"});
    double lastx = 0, lasty = 0;
    const double w = file_.width * 72, h = file_.height * 72;
    for (const auto& [x, y] : segmentEnds(path, trans, std::array<double, 4>{0, 0, w, h})) {
        if (!(0 <= x && x <= w && 0 <= y && y <= h)) continue;
        file_.output({reprInt(1), reprInt(0), reprInt(0), reprInt(1), num(x - lastx), num(y - lasty), "cm", marker, "Do"});
        lastx = x, lasty = y;
    }
    file_.output({"Q"});
}

void RendererPdf::drawPathCollection(const DrawGc& gc, const svg::RendererSVG::Collection& c) {
    // The XObjects can be shared only if the presence of fill and stroke (and their alpha) is the same
    // for all the paths.
    bool canDoOptimization = true, filled = false, stroked = false;
    if (c.facecolors.empty()) {
        canDoOptimization = !(gc.state.hatch && !gc.state.hatch->empty());
    } else {
        bool same = true;
        for (const auto& f : c.facecolors) same = same && f[3] == c.facecolors[0][3];
        if (same) filled = c.facecolors[0][3] != 0.0;
        else canDoOptimization = false;
    }
    if (!c.edgecolors.empty()) {
        bool allZero = true;
        for (const double lw : c.linewidths) allZero = allZero && lw == 0.0;
        bool same = true;
        for (const auto& e : c.edgecolors) same = same && e[3] == c.edgecolors[0][3];
        if (allZero) stroked = false;
        else if (same) stroked = c.edgecolors[0][3] != 0.0;
        else canDoOptimization = false;
    }
    // Is it worth it? A path in line costs len_path * uses_per_path; an XObject len_path + 5 for its
    // definition and uses_per_path for the uses.
    const std::size_t nPaths = c.paths.size(), nTransforms = c.transforms.size();
    const std::size_t lenPath = nPaths ? c.paths[0].size() : 0;
    std::size_t usesPerPath = 0;
    if (nPaths && !(c.facecolors.empty() && c.edgecolors.empty())) {
        const std::size_t nPathIds = std::max(nPaths, nTransforms);
        const std::size_t n = std::max(nPathIds, c.offsets.size());
        usesPerPath = (n + nPathIds - 1) / nPathIds;
    }
    const bool shouldDoOptimization = lenPath + usesPerPath + 5 < lenPath * usesPerPath;

    // _iter_collection_raw_paths
    std::vector<std::pair<svg::PathData, svg::Affine>> rawPaths;
    for (std::size_t i = 0, n = std::max(nPaths, nTransforms); nPaths && i < n; ++i)
        rawPaths.emplace_back(c.paths[i % nPaths], nTransforms ? svg::composeAffine(c.transforms[i % nTransforms], c.masterTransform)
                                                               : c.masterTransform);
    const bool optimize = canDoOptimization && shouldDoOptimization;
    std::vector<std::string> pathCodes;
    std::vector<std::array<double, 4>> pathExtentsOf;
    if (optimize) {
        double padding = c.linewidths.empty() ? 0.0 : c.linewidths[0];  // np.max(linewidths)
        for (const double lw : c.linewidths) padding = (std::isnan(lw) || lw > padding) ? lw : padding;
        if (c.linewidths.empty() && error.empty()) error = "a path collection without line widths";
        for (const auto& [p, t] : rawPaths) {
            pathCodes.push_back(file_.pathCollectionObject(gc.state.joinstyle, gc.state.capstyle, p, t, padding, filled, stroked));
            pathExtentsOf.push_back(pathExtents(p, t));
        }
        file_.output(push());
    }

    // _iter_collection
    const std::size_t nIds = rawPaths.size(), nOffsets = c.offsets.size();
    const std::size_t n = std::max(nIds, nOffsets);
    const double canvasWidth = file_.width * 72, canvasHeight = file_.height * 72;
    double lastx = 0, lasty = 0;
    if (!((c.facecolors.empty() && c.edgecolors.empty() && c.hatchcolors.empty()) || nIds == 0)) {
        DrawGc gc0 = gc;
        if (c.edgecolors.empty()) gc0.state.linewidth = 0.0;
        for (std::size_t i = 0; i < n && error.empty(); ++i) {
            double xo = 0, yo = 0;
            if (nOffsets) {
                const auto p = svg::applyAffine(c.offsetTransform, c.offsets[i % nOffsets][0], c.offsets[i % nOffsets][1]);
                xo = p[0], yo = p[1];
            }
            if (!(std::isfinite(xo) && std::isfinite(yo))) continue;
            if (!c.edgecolors.empty()) {
                if (!c.linewidths.empty()) gc0.state.linewidth = c.linewidths[i % c.linewidths.size()];
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
                    gc0.state.dashOffset = ls.first;
                    gc0.state.dashes = ls.second;
                }
                const auto& ec = c.edgecolors[i % c.edgecolors.size()];
                if (ec[3] == 0.0) {
                    gc0.state.linewidth = 0;
                } else {  // set_foreground(ec, isRGBA=True)
                    gc0.state.rgb = gc0.state.forcedAlpha ? std::vector<double>{ec[0], ec[1], ec[2], gc0.state.alpha} : ec;
                    gc0.rgbIsNone = false;
                }
            }
            if (!c.hatchcolors.empty()) gc0.state.hatchColor = c.hatchcolors[i % c.hatchcolors.size()];
            std::optional<std::vector<double>> fc;
            if (!c.facecolors.empty()) {
                const auto& f = c.facecolors[i % c.facecolors.size()];
                if (!(f.size() == 4 && f[3] == 0)) fc = f;
            }
            if (!c.urls.empty()) gc0.url = c.urls[i % c.urls.size()];

            if (!optimize) {  // RendererBase.draw_path_collection: one draw_path each
                svg::Affine t = rawPaths[i % nIds].second;
                if (xo != 0 || yo != 0) t.tx += xo, t.ty += yo;  // transform.frozen().translate(xo, yo)
                drawPath(gc0, rawPaths[i % nIds].first, t, fc);
                continue;
            }
            // markers completely outside the page are left out (by their path's box at the offset)
            const auto& b = pathExtentsOf[i % nIds];
            if (b[2] + xo < 0 || b[0] + xo > canvasWidth || b[3] + yo < 0 || b[1] + yo > canvasHeight) continue;
            if (gc0.url && error.empty()) error = "links in PDF are not supported yet";
            checkGc(gc0, fc);  // (its fill colour and effective alphas are set again by each check)
            file_.output({reprInt(1), reprInt(0), reprInt(0), reprInt(1), num(xo - lastx), num(yo - lasty), "cm",
                          pathCodes[i % nIds], "Do"});
            lastx = xo, lasty = yo;
        }
    }
    if (optimize) file_.output(pop());
}

void RendererPdf::finalize() {
    // make sure every pushed graphics state is popped
    std::vector<std::string> cmds;
    while (!parents_.empty()) {
        gc_ = parents_.back();
        parents_.pop_back();
        cmds.push_back("Q");
    }
    file_.output(cmds);
}

}  // namespace calculate_plot::pdf
