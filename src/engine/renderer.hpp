// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py (the RendererBase interface).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/gc.hpp"
#include "engine/path.hpp"
#include "transforms/transforms.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }
namespace calculate_plot::text { class FontCache; }

namespace calculate_plot::engine {

// The fonts of a text as the font manager found them (file names in the font directory, the
// fallback chain in order) and its size in points: what RendererAgg takes from FontProperties.
struct FontSpec {
    std::vector<std::string> files;
    double size = 10.0;
};

// What draw_text takes from the Text artist when the text has one line (mtext).
struct TextOptions {
    std::optional<std::vector<std::string>> features;
    std::optional<std::string> language;
};

// get_text_width_height_descent, in pixels.
struct TextMetrics {
    double width = 0, height = 0, descent = 0;
};

// What artists draw with: matplotlib's RendererBase methods.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual double dpi() const = 0;
    virtual double width() const = 0;   // the canvas, in pixels
    virtual double height() const = 0;
    virtual const rc::Params& rc() const = 0;

    virtual void openGroup(const std::string& s, const std::optional<std::string>& gid) { (void)s, (void)gid; }
    virtual void closeGroup(const std::string& s) { (void)s; }
    virtual void drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                          const std::optional<std::vector<double>>& face) = 0;

    virtual void drawMarkers(const GC& gc, const Path& markerPath, const transforms::Matrix& markerTrans,
                             const Path& path, const transforms::Matrix& trans,
                             const std::optional<std::vector<double>>& face) = 0;
    // x, y: the text's origin in pixels from the top-left (flipy); angle in degrees.
    virtual void drawText(const GC& gc, double x, double y, const std::string& s, const FontSpec& font, double angle,
                          bool ismath, const TextOptions& mtext) = 0;
    virtual TextMetrics textWidthHeightDescent(const std::string& s, const FontSpec& font, bool ismath) = 0;
    virtual bool flipy() const { return true; }
    // The fonts the renderer opened (matplotlib's get_font cache), for the font metrics of layouts.
    virtual text::FontCache& fontCache() = 0;
    // An artist met something calculate-plot cannot draw yet; the drawing reports it.
    virtual void unsupported(const std::string& what) = 0;

    GC newGc() const { return GC(rc()); }
    double pointsToPixels(double points) const { return points * dpi() / 72; }
};

}  // namespace calculate_plot::engine
