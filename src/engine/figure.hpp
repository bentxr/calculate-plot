// Derived from Matplotlib 3.11.2: lib/matplotlib/figure.py (Figure), lib/matplotlib/backend_bases.py
// (FigureCanvasBase.print_figure, get_width_height), lib/matplotlib/_tight_bbox.py (adjust_bbox) and
// lib/matplotlib/backends/backend_agg.py (FigureCanvasAgg.draw).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/constrained_layout.hpp"
#include "engine/tight_layout.hpp"

#include "engine/artist.hpp"
#include "engine/gridspec.hpp"
#include "engine/trace_renderer.hpp"
#include "rc/rc_params.hpp"

#include <memory>
#include <optional>
#include <string>

namespace calculate_plot::engine {

struct FigureOptions {
    std::optional<std::array<double, 2>> figsize;
    std::optional<double> dpi;
    std::optional<colors::Spec> facecolor, edgecolor;
    double linewidth = 0.0;
    std::optional<bool> frameon;
};

class Axes;

// The figure's layout engine (Figure.set_layout_engine): constrained (or compressed), tight, or the
// placeholder that fig.tight_layout() and layout='none' leave
struct LayoutEngine {
    enum class Kind { Constrained, Tight, Placeholder } kind = Kind::Placeholder;
    ConstrainedLayoutParams constrained;
    TightLayoutOptions tight;
    bool adjustCompatible = true;  // subplots_adjust is taken (not with the constrained layout)
};

class Figure : public Artist {
public:
    Figure(const rc::Params& rc, const FigureOptions& options);
    ~Figure() override;
    void draw(Renderer& renderer) override;
    void setDpi(double value);  // Figure.dpi = value: the boxes in pixels follow
    void setSizeInches(double width, double height);  // set_size_inches(forward=False): bbox_inches.p1 = (w, h)

    double dpi;
    std::shared_ptr<transforms::Bbox> bboxInches;
    std::shared_ptr<transforms::Affine2D> dpiScaleTrans;
    std::shared_ptr<transforms::TransformedBbox> bbox;
    std::shared_ptr<transforms::BboxTransformTo> transFigure;
    transforms::TransformPtr transSubfigure;
    std::unique_ptr<Rectangle> patch;
    SubplotParams subplotpars;
    std::vector<std::unique_ptr<Axes>> axes;
    std::vector<std::unique_ptr<GridSpec>> gridspecs;  // kept alive for the axes' subplot specs
    std::vector<std::unique_ptr<Artist>> texts;         // figure texts (suptitle)
    std::vector<std::unique_ptr<Artist>> images;        // fig.figimage's FigureImages
    Artist* suptitle = nullptr;
    bool suptitleAutopos = true;  // (its y was not given: a layout places it)
    std::optional<LayoutEngine> layoutEngine;
    bool layoutOff = false;  // print_figure's layout_engine='none' while it prints (the layout is done)

    // FigureCanvasBase.get_width_height(physical=True): the canvas in whole pixels, int(size + 1e-8)
    // of the figure's box (its upper corner).
    std::array<double, 2> canvasSize() const;

    // Figure.get_tightbbox(renderer): the box around the figure's artists, in inches (the figure's
    // own box when there are none).
    transforms::Bbox tightbbox(Renderer& renderer);

    // Figure.subplots(nrows, ncols, sharex=, sharey=): the axes in row-major order. The sharing is
    // "none", "all", "row" or "col".
    std::vector<Axes*> subplots(int nrows, int ncols, const std::string& sharex = "none",
                                const std::string& sharey = "none", const GridSpec* ratios = nullptr, bool polar = false);
    // Figure.add_subplot(nrows, ncols, index): an Axes in cells num1 ... num2 (row-major, from 0) of a
    // grid of that shape: the one an Axes of the figure already uses (GridSpec._check_gridspec_exists),
    // or a new one.
    // (`polar`: projection='polar', a PolarAxes)
    Axes* addSubplot(int nrows, int ncols, int num1, int num2, bool polar = false);
    Axes* addSubplot(const SubplotSpec& spec, bool polar = false);  // add_subplot(ss): another Axes in those cells
    // Figure.add_axes((left, bottom, width, height)): an Axes at that position, in figure fractions
    Axes* addAxes(double left, double bottom, double width, double height, bool polar = false);
    Axes* addAxesBox(const std::array<double, 4>& points);  // add_axes(Bbox): the box itself as the position
};

// Figure.savefig as a PNG, into a trace: what FigureCanvasBase.print_figure sets for the save
// (rcParams savefig.dpi, facecolor, edgecolor, transparent, bbox 'tight' with pad_inches), then
// FigureCanvasAgg.draw: the renderer is cleared and the figure drawn. The figure is as it was
// afterwards. Returns the trace JSON (render/replay draws it to pixels); `error` says what could not
// be traced.
std::string drawAggTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         std::string* error);
// FigureCanvasQTAgg's drawing on a screen of `ratio` device pixels per logical pixel: the figure at its
// dpi times the ratio (_set_device_pixel_ratio), resized to a widget of `size` logical pixels when given
// (resizeEvent: set_size_inches(w * ratio / dpi, h * ratio / dpi, forward=False)), drawn as
// FigureCanvasAgg.draw draws it (no savefig settings), into a trace.
std::string drawScreenTrace(Figure& figure, double ratio, const std::string& locale, const std::string& fontDirectory,
                            std::string* error, std::optional<std::array<double, 2>> size = std::nullopt);
// Figure.savefig as an SVG, into a trace (render/svg_replay writes the SVG): print_figure as above,
// then FigureCanvasSVG.print_svg: the figure is drawn at 72 dpi, its images at the dpi of the save.
std::string drawSvgTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         const SvgMetadata& metadata,
                         const std::string& hashsalt, std::string* error);

}  // namespace calculate_plot::engine
