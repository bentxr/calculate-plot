// Derived from Matplotlib 3.11.2: lib/matplotlib/figure.py (Figure) and
// lib/matplotlib/backends/backend_agg.py (FigureCanvasAgg.draw).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/figure.hpp"


namespace calculate_plot::engine {

Figure::Figure(const rc::Params& rc, const FigureOptions& o) : Artist(rc) {
    std::array<double, 2> figsize;
    if (o.figsize) figsize = *o.figsize;
    else {
        const auto v = rc.numbers("figure.figsize");
        figsize = {v[0], v[1]};
    }
    dpi = o.dpi ? *o.dpi : rc.number("figure.dpi");
    const std::string facecolor = o.facecolor ? *o.facecolor : rc.text("figure.facecolor");
    const std::string edgecolor = o.edgecolor ? *o.edgecolor : rc.text("figure.edgecolor");
    const bool frameon = o.frameon ? *o.frameon : rc.boolean("figure.frameon");

    bboxInches = transforms::Bbox::fromBounds(0, 0, figsize[0], figsize[1]);
    dpiScaleTrans = std::make_shared<transforms::Affine2D>();
    dpiScaleTrans->scale(dpi);
    bbox = std::make_shared<transforms::TransformedBbox>(bboxInches, dpiScaleTrans);
    transFigure = std::make_shared<transforms::BboxTransformTo>(bbox);
    transSubfigure = transFigure;

    // Rectangle(xy=(0, 0), width=1, height=1, visible=frameon, facecolor=, edgecolor=, linewidth=)
    patch = std::make_unique<Rectangle>(rc, 0.0, 0.0, 1.0, 1.0);
    patch->setEdgecolor(edgecolor);
    patch->setFacecolor(facecolor);
    patch->setLinewidth(o.linewidth);
    patch->visible = frameon;
    patch->transform = transSubfigure;  // _set_artist_props
    patch->antialiased = false;
}

void Figure::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup("figure", gid);
    patch->draw(renderer);
    renderer.closeGroup("figure");
}

void Figure::setDpi(double value) {
    if (value == dpi) return;
    dpi = value;
    *dpiScaleTrans = transforms::Affine2D();
    dpiScaleTrans->scale(dpi);
}

std::string drawAggTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         std::string* error) {
    // RendererAgg(w, h, dpi) takes int(w), int(h) of the figure's box.
    const auto p = figure.bbox->points();
    AggTraceRenderer renderer(double(int(p[2] - p[0])), double(int(p[3] - p[1])), figure.dpi, figure.rc(), locale, fontDirectory);
    renderer.clear();
    figure.draw(renderer);
    *error = renderer.error();
    return renderer.trace();
}

std::string drawSvgTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         const SvgMetadata& metadata,
                         const std::string& hashsalt, std::string* error) {
    const double dpi = figure.dpi;
    figure.setDpi(72);
    const auto inches = figure.bboxInches->points();
    SvgTraceRenderer renderer(inches[2] * 72, inches[3] * 72, dpi, figure.rc(), locale, fontDirectory, metadata, hashsalt);
    figure.draw(renderer);
    figure.setDpi(dpi);  // print_figure puts the dpi back
    *error = renderer.error();
    return renderer.trace();
}

}  // namespace calculate_plot::engine
