// Derived from Matplotlib 3.11.2: lib/matplotlib/figure.py (Figure) and
// lib/matplotlib/backends/backend_agg.py (FigureCanvasAgg.draw).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"
#include "engine/trace_renderer.hpp"
#include "rc/rc_params.hpp"

#include <memory>
#include <optional>
#include <string>

namespace calculate_plot::engine {

struct FigureOptions {
    std::optional<std::array<double, 2>> figsize;
    std::optional<double> dpi;
    std::optional<std::string> facecolor, edgecolor;
    double linewidth = 0.0;
    std::optional<bool> frameon;
};

class Figure : public Artist {
public:
    Figure(const rc::Params& rc, const FigureOptions& options);
    void draw(Renderer& renderer) override;
    void setDpi(double value);  // Figure.dpi = value: the boxes in pixels follow

    double dpi;
    std::shared_ptr<transforms::Bbox> bboxInches;
    std::shared_ptr<transforms::Affine2D> dpiScaleTrans;
    std::shared_ptr<transforms::TransformedBbox> bbox;
    transforms::TransformPtr transFigure, transSubfigure;
    std::unique_ptr<Rectangle> patch;
};

// FigureCanvasAgg.draw into a trace: the renderer is cleared, then the figure drawn. Returns the
// trace JSON (render/replay draws it to pixels); `error` says what could not be traced.
std::string drawAggTrace(Figure& figure, const std::string& locale, std::string* error);
// FigureCanvasSVG.print_svg into a trace (render/svg_replay writes the SVG): the figure is drawn at
// 72 dpi, its images at its own dpi, which it keeps afterwards.
std::string drawSvgTrace(Figure& figure, const std::string& locale, const SvgMetadata& metadata,
                         const std::string& hashsalt, std::string* error);

}  // namespace calculate_plot::engine
