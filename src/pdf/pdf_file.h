// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_pdf.py (pdfRepr, _fill, Name, Stream,
// PdfFile, RendererPdf, GraphicsContextPdf) and lib/matplotlib/backend_bases.py (GraphicsContextBase's state).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "svg/svg_renderer.h"  // PathData, Affine: the trace's paths and matrices

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::pdf {

// pdfRepr, piece by piece. Every function returns the bytes of one PDF token (a nested list or
// dictionary is one token, broken into lines by _fill).
std::string reprNumber(double x, std::string* error);  // a float: "%.10f" without trailing zeros
std::string reprInt(long long x);
std::string reprName(const std::string& name);          // Name(name).pdfRepr()
std::string reprString(const std::string& utf8);         // a str: ASCII as is, else UTF-16BE with its mark
std::string reprBytes(const std::string& bytes);         // bytes: in parentheses, escaped
std::string reprRef(int id);                             // Reference(id).pdfRepr()
std::string fill(const std::vector<std::string>& strings, std::size_t linelen = 75);
std::string reprList(const std::vector<std::string>& items);
using Dict = std::vector<std::pair<std::string, std::string>>;  // keys as names (unescaped), values as reprs
std::string reprDict(const Dict& items);

// PdfFile.pathOperations: the path as PDF operators (convert_to_string, precision 6, postfix).
std::string pathOperations(const svg::PathData& path, const svg::Affine& transform,
                           std::optional<std::array<double, 4>> clip, bool simplify,
                           const std::optional<std::array<double, 3>>& sketch, std::string* error);
// Path.get_extents(transform): x0, y0, x1, y1 (inf, inf, -inf, -inf for a null box)
std::array<double, 4> pathExtents(const svg::PathData& path, const svg::Affine& transform);

// PdfFile: the objects of one file, written in matplotlib's order, with its streams.
class PdfFile {
public:
    // `compression`: rcParams["pdf.compression"] (0 writes streams uncompressed); `info`: the
    // document information dictionary as _create_pdf_info_dict returns it.
    PdfFile(int compression, Dict info);
    ~PdfFile();
    PdfFile(const PdfFile&) = delete;
    PdfFile& operator=(const PdfFile&) = delete;

    int reserveObject();
    void writeObject(int id, const std::string& contents);
    void newPage(double width, double height);  // in inches
    void write(const std::string& data);        // to the current stream, if any
    void output(const std::vector<std::string>& data);
    void beginStream(int id, std::optional<int> lengthObject, Dict extra = {});
    void endStream();
    std::string alphaState(double strokeAlpha, double fillAlpha);  // the name's repr
    // markerObject: a marker XObject's name. (Its key holds the path operations as a new Verbatim,
    // which compares by identity: every call makes a new marker.)
    std::string markerObject(const svg::PathData& path, const svg::Affine& trans, bool fill, bool stroke, double lw,
                             const std::string& joinstyle, const std::string& capstyle);
    // hatchPattern: the name of the tiling pattern of (hatch colour, fill colour, hatch, line width);
    // `hatchPath` is Path.hatch(hatch), which writeHatches draws
    std::string hatchPattern(const std::optional<std::vector<double>>& edge, const std::optional<std::vector<double>>& face,
                             const std::string& hatch, double lw, const svg::PathData& hatchPath);
    // addGouraudTriangles: a type 4 shading of the triangles (points in the page's coordinates) with
    // RGBA vertex colours (their alpha is not written); its name
    std::string addGouraudTriangles(const std::vector<std::array<std::array<double, 2>, 3>>& points,
                                    const std::vector<std::array<std::array<double, 4>, 3>>& colors);
    // imageObject: an image XObject's name (I1, I2, ...) for an RGBA image of `width` x `height`
    std::string imageObject(int width, int height, std::vector<unsigned char> rgba);
    std::string pathCollectionObject(const std::string& joinstyle, const std::string& capstyle,
                                     const svg::PathData& path, const svg::Affine& trans, double padding,
                                     bool filled, bool stroked);
    void finalize();

    const std::string& bytes() const { return fh_; }
    double width = 0, height = 0;  // of the current page, in inches
    std::string error;

private:
    struct Stream;
    void recordXref(int id);
    void writeXref();
    void writeTrailer();
    void writeHatches();
    void writeGouraudTriangles();
    void writeImages();
    void writeImg(const std::vector<unsigned char>& data, int width, int height, int channels, int id,
                  std::optional<int> smask);
    void writeMarkers();
    void writePathCollectionTemplates();

    int compression_;
    Dict info_;
    std::string fh_;
    std::vector<std::optional<std::size_t>> xref_{std::size_t(0)};  // offsets; [0] is the zero object
    std::unique_ptr<Stream> current_;
    int rootObject_ = 0, pagesObject_ = 0, fontObject_ = 0, extGStateObject_ = 0, hatchObject_ = 0,
        gouraudObject_ = 0, xObjectObject_ = 0, resourceObject_ = 0, infoObject_ = 0;
    std::size_t startxref_ = 0;
    std::vector<int> pageList_;
    std::vector<int> annotations_;  // each page's annotations object (no annotations are written yet)
    struct AlphaState {
        double stroke, fill;
        std::string name;
    };
    std::vector<AlphaState> alphaStates_;
    struct Marker {
        std::string pathops;
        bool fill, stroke;
        std::string joinstyle, capstyle, name;
        int ob;
        std::array<double, 4> bbox;
        double lw;
    };
    std::vector<Marker> markers_;
    struct ImageEntry {
        int width, height;
        std::vector<unsigned char> rgba;
        std::string name;
        int ob;
    };
    std::vector<ImageEntry> images_;
    struct HatchPattern {
        std::optional<std::vector<double>> edge, face;
        std::string hatch;
        double lw;
        svg::PathData path;
        std::string name;
    };
    std::vector<HatchPattern> hatchPatterns_;
    struct Gouraud {
        std::string name;
        int ob;
        std::vector<std::array<std::array<double, 2>, 3>> points;
        std::vector<std::array<std::array<double, 4>, 3>> colors;
    };
    std::vector<Gouraud> gouraudTriangles_;
    struct PathTemplate {
        std::string name;
        svg::PathData path;
        svg::Affine trans;
        int ob;
        std::string joinstyle, capstyle;
        double padding;
        bool filled, stroked;
    };
    std::vector<PathTemplate> paths_;
};

// The graphics state GraphicsContextPdf.delta compares: GraphicsContextBase's fields with
// GraphicsContextPdf's fill colour and effective alphas. The clip box and the clip path are
// compared by the identity of their objects (`*Id`, numbered by the recorder).
struct GcState {
    std::optional<std::array<double, 4>> cliprect;  // x0, y0, x1, y1
    std::optional<long long> cliprectId;
    std::optional<svg::PathData> clippath;  // transformed, as get_transformed_path_and_affine gives it
    svg::Affine clipTransform;
    std::optional<long long> clippathId;
    double alpha = 1.0;
    bool forcedAlpha = false;
    std::array<double, 2> effectiveAlphas{1.0, 1.0};
    std::string capstyle = "butt";
    std::optional<std::vector<double>> fillcolor = std::vector<double>{0.0, 0.0, 0.0};
    std::string joinstyle = "round";
    double linewidth = 1.0;
    double dashOffset = 0.0;
    std::optional<std::vector<double>> dashes;
    std::vector<double> rgb{0.0, 0.0, 0.0, 1.0};
    std::optional<std::string> hatch;
    std::optional<std::vector<double>> hatchColor;
    double hatchLinewidth = 1.0;
    svg::PathData hatchPath;  // get_hatch_path() (not compared: it follows the hatch)
};

// What a drawing call's graphics context holds besides that state.
struct DrawGc {
    GcState state;  // fillcolor / effectiveAlphas are set by check_gc
    bool rgbIsNone = false;
    bool hasHatchPath = false;
    std::optional<std::array<double, 3>> sketch;
    std::optional<std::string> url;
};

class RendererPdf {
public:
    RendererPdf(PdfFile& file, double imageDpi, double hatchLinewidth, bool inheritColor);
    // (check_gc's set_foreground of a missing colour stays on the caller's context, as in matplotlib)
    void drawPath(DrawGc& gc, const svg::PathData& path, const svg::Affine& transform,
                  const std::optional<std::vector<double>>& rgbFace);
    void drawMarkers(DrawGc& gc, const svg::PathData& markerPath, const svg::Affine& markerTrans,
                     const svg::PathData& path, const svg::Affine& trans,
                     const std::optional<std::vector<double>>& rgbFace);
    void drawPathCollection(const DrawGc& gc, const svg::RendererSVG::Collection& c);
    void drawImage(DrawGc& gc, double x, double y, int width, int height, std::vector<unsigned char> rgba,
                   const std::optional<svg::Affine>& transform);
    void drawGouraudTriangles(DrawGc& gc, const std::vector<std::array<std::array<double, 2>, 3>>& points,
                              const std::vector<std::array<std::array<double, 4>, 3>>& colors, const svg::Affine& trans);
    void finalize();
    std::string error;

private:
    void checkGc(DrawGc& gc, const std::optional<std::vector<double>>& fillcolor);
    std::vector<std::string> delta(const GcState& other);
    std::vector<std::string> clipCmd(const GcState& other);
    std::vector<std::string> fillcolorCmd(const std::optional<std::vector<double>>& rgb);
    std::vector<std::string> pathOperations(const svg::PathData& path, const svg::Affine& transform,
                                            std::optional<std::array<double, 4>> clip, bool simplify,
                                            const std::optional<std::array<double, 3>>& sketch);
    std::string paint() const;
    std::vector<std::string> push();
    std::vector<std::string> pop();
    std::string num(double x);

    PdfFile& file_;
    double imageDpi_;
    bool inheritColor_;
    GcState gc_;                   // the state the content stream is in
    std::vector<GcState> parents_;  // the states saved by q
};

}  // namespace calculate_plot::pdf
