// Derived from Matplotlib 3.11.2: lib/matplotlib/artist.py, lib/matplotlib/patches.py (Patch,
// Rectangle), lib/matplotlib/lines.py (dash patterns).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/artist.hpp"

#include "rc/rc_params.hpp"

namespace calculate_plot::engine {

std::pair<double, std::optional<std::vector<double>>> dashPattern(const std::string& style, const rc::Params& rc) {
    if (style == "solid" || style == "None") return {0.0, std::nullopt};
    if (style == "dashed" || style == "dashdot" || style == "dotted") {
        // dashes and dashdot styles: the rcParams lines.<style>_pattern, offset 0
        return {0.0, rc.numbers("lines." + style + "_pattern")};
    }
    return {0.0, std::nullopt};
}

std::pair<double, std::optional<std::vector<double>>> scaleDashes(
    const std::pair<double, std::optional<std::vector<double>>>& pattern, double lw, const rc::Params& rc) {
    if (!rc.boolean("lines.scale_dashes")) return pattern;
    std::pair<double, std::optional<std::vector<double>>> out{pattern.first * lw, std::nullopt};
    if (pattern.second) {
        std::vector<double> scaled;
        for (double x : *pattern.second) scaled.push_back(x * lw);
        out.second = scaled;
    }
    return out;
}

void Artist::setGcClip(GC& gc) const {
    if (clipOn) {
        if (clipBox) {
            const auto p = clipBox->points();
            gc.cliprect = std::array<double, 4>{p[0], p[1], p[2], p[3]};
        }
        gc.clippath = clipPath;
    } else {
        gc.cliprect.reset();
        gc.clippath.reset();
    }
}

// Patch.__init__ with its defaults, in its order: edge colour, face colour, line style, line width.
Patch::Patch(const rc::Params& rc) : Artist(rc), antialiased(rc.boolean("patch.antialiased")) {
    zorder = 1;
    setEdgecolor(std::nullopt);
    setFacecolor(std::nullopt);
    setLinestyle("solid");
    setLinewidth(std::nullopt);
}

void Patch::setEdgecolor(const std::optional<std::string>& color) {
    originalEdgecolor_ = color;
    applyEdgecolor();
}

void Patch::applyEdgecolor() {
    std::string c;
    if (originalEdgecolor_) c = *originalEdgecolor_;
    else if (rc_.boolean("patch.force_edgecolor") || !fill_ || edgeDefault_) c = rc_.text("patch.edgecolor");
    else c = "none";
    edgecolor_ = *colors::toRgba(c, alpha, rc_, nullptr);
}

void Patch::setFacecolor(const std::optional<std::string>& color) {
    originalFacecolor_ = color;
    applyFacecolor();
}

void Patch::applyFacecolor() {
    const std::string c = originalFacecolor_ ? *originalFacecolor_ : rc_.text("patch.facecolor");
    facecolor_ = *colors::toRgba(c, fill_ ? alpha : std::optional<double>(0.0), rc_, nullptr);
}

void Patch::setAlpha(std::optional<double> a) {
    alpha = a;
    applyFacecolor();
    applyEdgecolor();
}

void Patch::setFill(bool b) {
    fill_ = b;
    applyFacecolor();
    applyEdgecolor();
}

void Patch::setLinewidth(std::optional<double> w) {
    linewidth_ = w ? *w : rc_.number("patch.linewidth");
    dashes_ = scaleDashes(unscaledDashes_, linewidth_, rc_);
}

void Patch::setLinestyle(const std::string& style) {
    linestyle_ = (style == " " || style == "" || style == "none") ? "None" : style;
    unscaledDashes_ = dashPattern(linestyle_, rc_);
    dashes_ = scaleDashes(unscaledDashes_, linewidth_, rc_);
}

void Patch::draw(Renderer& renderer) {
    if (!visible) return;
    // get_transform: the patch's own transform, then the artist's; all affine so far.
    const auto t = transforms::compose(patchTransform(), transform);
    std::optional<std::vector<double>> face;
    if (facecolor_[3] != 0) face = std::vector<double>(facecolor_.begin(), facecolor_.end());

    // _draw_paths_with_artist_properties
    renderer.openGroup("patch", gid);
    GC gc = renderer.newGc();
    double lw = linewidth_;
    if (edgecolor_[3] == 0 || linestyle_ == "None") lw = 0;
    gc.linewidth = lw;
    gc.capstyle = capstyle;
    gc.joinstyle = joinstyle;
    gc.antialiased = antialiased;
    setGcClip(gc);
    gc.url = url;
    gc.snap = snap;
    gc.setAlpha(alpha);
    if (sketch) gc.sketch = sketch;
    gc.setForeground(std::vector<double>(edgecolor_.begin(), edgecolor_.end()));
    if (lw > 0) gc.setDashes(dashes_.first, dashes_.second);
    else gc.setDashes(0, std::nullopt);
    renderer.drawPath(gc, path(), t->matrix(), face);
    renderer.closeGroup("patch");
}

Rectangle::Rectangle(const rc::Params& rc, double x, double y, double w, double h, double a)
    : Patch(rc), x0(x), y0(y), width(w), height(h), angle(a) {}

transforms::TransformPtr Rectangle::patchTransform() const {
    // BboxTransformTo(bbox) + Affine2D() rotating about rotation_point 'xy' (the aspect ratio
    // correction is 1 outside of RectangleSelector).
    auto bbox = transforms::Bbox::fromExtents(x0, y0, x0 + width, y0 + height);
    const auto p = bbox->points();
    const double rx = p[0], ry = p[1];
    auto rotation = std::make_shared<transforms::Affine2D>();
    rotation->translate(-rx, -ry);
    rotation->scale(1, 1.0);
    rotation->rotateDeg(angle);
    rotation->scale(1, 1 / 1.0);
    rotation->translate(rx, ry);
    return transforms::compose(std::make_shared<transforms::BboxTransformTo>(bbox), rotation);
}

}  // namespace calculate_plot::engine
