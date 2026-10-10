// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_svg.py (RendererSVG),
// lib/matplotlib/backend_bases.py (the path collection helpers), lib/matplotlib/textpath.py
// (TextToPath for plain text) and lib/matplotlib/colors.py (rgb2hex).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "svg/xml_writer.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::text { class FontCache; }

namespace calculate_plot::svg {

// matplotlib's 3x3 affine matrix [[sx, shx, tx], [shy, sy, ty], [0, 0, 1]].
struct Affine {
    double sx = 1, shx = 0, tx = 0, shy = 0, sy = 1, ty = 0;
};

struct PathData {
    std::shared_ptr<const std::vector<double>> vertices;  // x0, y0, x1, y1, ...
    std::shared_ptr<const std::vector<unsigned char>> codes;  // null: no codes
    bool simplify = false;
    double threshold = 0.111111111111;
    std::size_t size() const { return vertices ? vertices->size() / 2 : 0; }
};

// The graphics context, as RendererSVG reads it.
struct Gc {
    double linewidth = 1.0;
    double alpha = 1.0;
    bool forcedAlpha = false;
    std::vector<double> rgb{0, 0, 0, 1};
    bool antialiased = true;
    std::string capstyle = "butt";
    std::string joinstyle = "round";
    double dashOffset = 0.0;
    std::optional<std::vector<double>> dashes;
    std::optional<std::array<double, 4>> cliprect;  // x0, y0, x1, y1
    struct ClipPath {
        PathData path;
        Affine transform;
        int index = 0;              // RendererSVG numbers clip path objects in order of use
        std::string transformText;  // str() of the flipped clip transform (hashed into the id)
    };
    std::optional<ClipPath> clippath;
    std::optional<bool> snap;
    std::optional<std::string> hatch;
    bool hatchNumpy = false;  // a np.str_ (patches) rather than a str (collections)
    PathData hatchPath;
    std::optional<std::vector<double>> hatchColor;
    double hatchLinewidth = 1.0;
    std::optional<std::array<double, 3>> sketch;
    std::optional<std::string> url;
    std::optional<std::string> gid;
};

// A face colour; `numpy` says whether its components are NumPy scalars (it changes the text that
// matplotlib hashes into hatch ids).
struct Face {
    std::vector<double> rgba;
    bool numpy = false;
};

struct Metadata {
    // Values in matplotlib's metadata dict: absent, explicitly None, a string, or a list of strings.
    std::map<std::string, std::optional<std::vector<std::string>>> entries;
};

struct Glyph {  // a mathtext glyph as TextToPath receives it
    std::string font;
    double size;
    unsigned index;
    double ox, oy;
};

class RendererSVG {
public:
    RendererSVG(double width, double height, double imageDpi, const Metadata& metadata, std::string hashsalt,
                std::optional<std::string> svgId, text::FontCache& fonts, std::string language);

    void openGroup(const std::string& s, const std::optional<std::string>& gid);
    void closeGroup();
    void drawPath(const Gc& gc, const PathData& path, const Affine& transform, const std::optional<Face>& face);
    void drawMarkers(const Gc& gc, const PathData& markerPath, const Affine& markerTrans, const PathData& path,
                     const Affine& trans, const std::optional<Face>& face);
    struct Collection {
        Affine masterTransform;
        std::vector<PathData> paths;
        std::vector<Affine> transforms;
        std::vector<std::array<double, 2>> offsets;
        Affine offsetTransform;
        std::vector<std::vector<double>> facecolors, edgecolors, hatchcolors;
        std::vector<double> linewidths;
        std::vector<std::pair<double, std::optional<std::vector<double>>>> linestyles;
        std::vector<bool> antialiaseds;
        std::vector<std::optional<std::string>> urls;
    };
    void drawPathCollection(const Gc& gc, const Collection& c);
    void drawGouraudTriangles(const Gc& gc, const std::vector<std::array<std::array<double, 2>, 3>>& triangles,
                              const std::vector<std::array<std::array<double, 4>, 3>>& colors, const Affine& transform);
    void drawImage(const Gc& gc, double x, double y, int width, int height, const std::vector<unsigned char>& rgba,
                   const std::optional<Affine>& transform);
    void drawTextAsPath(const Gc& gc, double x, double y, const std::string& s, const std::vector<std::string>& fonts,
                        double size, double angle, const std::optional<std::vector<std::string>>& features,
                        const std::optional<std::string>& language);
    void drawMathtextAsPath(const Gc& gc, double x, double y, const std::string& s, double size, double angle,
                            const std::vector<Glyph>& glyphs, const std::vector<std::array<double, 4>>& rects);
    // Writes the clip paths and hatches and closes the document; returns the SVG text.
    std::string finalize();

    std::string error;
    std::vector<std::string> hashedContents;  // what each id hashed, in order (for checking)

private:
    std::string makeId(const std::string& type, const std::string& content);
    std::string convertPath(const PathData& path, const std::optional<Affine>& transform, bool clip, bool simplify,
                            const std::optional<std::array<double, 3>>& sketch = std::nullopt);
    std::vector<std::pair<std::string, std::string>> styleDict(const Gc& gc, const std::optional<Face>& face);
    std::string style(const Gc& gc, const std::optional<Face>& face);
    Attributes clipAttrs(const Gc& gc);
    std::string hatch(const Gc& gc, const std::optional<Face>& face);
    void writeClips();
    void writeHatches();
    void writeMetadata(const Metadata& metadata);
    void writeDefaultStyle();
    Affine flip(const Affine& transform) const;
    void drawGouraudTriangle(const std::array<std::array<double, 2>, 3>& points,
                             const std::array<std::array<double, 4>, 3>& colors);
    void updateGlyphMapDefs(const std::vector<std::pair<std::string, PathData>>& glyphs);

    std::string out_;
    XmlWriter writer_;
    double width_, height_, imageDpi_;
    std::string hashsalt_;
    text::FontCache& fonts_;
    std::string language_;
    int startId_ = 0;
    std::map<std::string, int> groups_;
    std::map<std::string, std::string> markers_;
    int pathCollectionId_ = 0;
    struct Hatch {
        std::string key;
        PathData path;
        std::optional<std::vector<double>> face;
        std::vector<double> edge;
        double linewidth;
        std::string id;
    };
    std::vector<Hatch> hatches_;  // in insertion order, as Python dicts
    struct Clip {
        std::string key;
        std::optional<std::array<double, 4>> rect;  // x, y, w, h
        PathData path;
        Affine transform;
        std::string id;
    };
    std::vector<Clip> clips_;
    bool hasGouraud_ = false;
    int gradients_ = 0;
    std::vector<std::string> glyphMap_;  // glyph ids already defined
};

// Python's repr() of a str and of bytes (single quotes unless the text has some and no double).
std::string pythonStrRepr(const std::string& s);
std::string pythonBytesRepr(const std::string& bytes);
// matplotlib.colors.rgb2hex for an RGB(A) tuple.
std::string rgb2hex(const std::vector<double>& c);

}  // namespace calculate_plot::svg
