#pragma once

#include "engine/renderer.hpp"
#include "support/json_writer.hpp"
#include "text/text_render.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::engine {

// Renderers that write down what matplotlib's renderers would be asked to draw, in the trace
// formats of tools/oracle/record.py, so a figure drawn by calculate-plot can be compared with
// matplotlib's call by call, then replayed to pixels (render/replay) or SVG (render/svg_replay).
class TraceRenderer : public Renderer {
public:
    // `fontDirectory` holds the fonts that texts name (FontSpec::files).
    TraceRenderer(double width, double height, double dpi, const rc::Params& rc, std::string locale,
                  std::string fontDirectory);

    double dpi() const override { return dpi_; }
    double width() const override { return width_; }
    double height() const override { return height_; }
    const rc::Params& rc() const override { return rc_; }
    text::FontCache& fontCache() override { return fonts_; }
    void unsupported(const std::string& what) override {
        if (error_.empty()) error_ = what;
    }

    virtual std::string trace() = 0;  // the whole trace, as JSON
    const std::string& error() const { return error_; }  // what the trace format cannot express yet

protected:
    void writeGc(const GC& gc);
    void writeGcFields(const GC& gc);
    void writePath(const Path& path);
    void writeMatrix(const transforms::Matrix& m);
    void writeNumbers(const std::vector<double>& values);
    void writeColour(const std::vector<double>& rgba, bool intAlpha);
    void writeOptionalText(const std::optional<std::string>& text);
    void writeArray(const std::vector<std::size_t>& shape, const std::vector<double>& data);  // record.py's array()
    void writeCollectionFields(const PathCollectionArgs& args);
    int clipIndex(const ClipPath& clip);  // clip paths numbered by the identity of their Path
    std::string events();                 // the events array

    double width_, height_, dpi_;
    const rc::Params& rc_;
    std::string locale_;
    text::FontCache fonts_;
    support::JsonWriter events_;
    std::string error_;

private:
    std::map<const void*, int> clipIds_;
};

// What matplotlib's RendererAgg is asked to draw (record.py's record).
class AggTraceRenderer : public TraceRenderer {
public:
    using TraceRenderer::TraceRenderer;

    void clear();
    void drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                  const std::optional<std::vector<double>>& face) override;
    void drawMarkers(const GC& gc, const Path& markerPath, const transforms::Matrix& markerTrans, const Path& path,
                     const transforms::Matrix& trans, const std::optional<std::vector<double>>& face) override;
    void drawPathCollection(const GC& gc, const PathCollectionArgs& args) override;
    // The draw_text call, then what it hands on: the laid-out glyphs (_draw_text_glyphs_and_boxes) and
    // each glyph's bitmap (draw_text_image).
    void drawText(const GC& gc, double x, double y, const std::string& s, const FontSpec& font, double angle,
                  bool ismath, const TextOptions& mtext) override;
    TextMetrics textWidthHeightDescent(const std::string& s, const FontSpec& font, bool ismath) override;
    std::string trace() override;

private:
    // The draw_path call itself, after RendererAgg.draw_path's chunks.
    void drawWhole(const GC& gc, const Path& path, const transforms::Matrix& affine,
                   const std::optional<std::vector<double>>& face);
    void writeFace(const std::optional<std::vector<double>>& face);
};

using SvgMetadata = std::vector<std::pair<std::string, std::optional<std::string>>>;

// What matplotlib's RendererSVG is asked to draw (record.py's record_svg): the canvas in points,
// at 72 dpi; images at the figure's own dpi.
class SvgTraceRenderer : public TraceRenderer {
public:
    SvgTraceRenderer(double width, double height, double imageDpi, const rc::Params& rc, std::string locale,
                     std::string fontDirectory, SvgMetadata metadata, std::string hashsalt);

    void openGroup(const std::string& s, const std::optional<std::string>& gid) override;
    void closeGroup(const std::string& s) override;
    void drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                  const std::optional<std::vector<double>>& face) override;
    void drawMarkers(const GC& gc, const Path& markerPath, const transforms::Matrix& markerTrans, const Path& path,
                     const transforms::Matrix& trans, const std::optional<std::vector<double>>& face) override;
    void drawPathCollection(const GC& gc, const PathCollectionArgs& args) override;
    void drawText(const GC& gc, double x, double y, const std::string& s, const FontSpec& font, double angle,
                  bool ismath, const TextOptions& mtext) override;
    // TextToPath.get_text_width_height_descent: unhinted, at TextToPath.FONT_SCALE, scaled to the size.
    TextMetrics textWidthHeightDescent(const std::string& s, const FontSpec& font, bool ismath) override;
    std::string trace() override;

private:
    void writeSvgGc(const GC& gc);
    void writeClipTransform(const GC& gc);

    double imageDpi_;
    SvgMetadata metadata_;
    std::string hashsalt_;
};

}  // namespace calculate_plot::engine
