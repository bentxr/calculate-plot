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

void Patch::setEdgecolor(const std::optional<colors::Spec>& color) {
    originalEdgecolor_ = color;
    applyEdgecolor();
}

void Patch::applyEdgecolor() {
    colors::Spec c;
    if (originalEdgecolor_) c = *originalEdgecolor_;
    else if (rc_.boolean("patch.force_edgecolor") || !fill_ || edgeDefault_) c = rc_.text("patch.edgecolor");
    else c = std::string("none");
    edgecolor_ = *colors::toRgba(c, alpha, rc_, nullptr);
}

void Patch::setFacecolor(const std::optional<colors::Spec>& color) {
    originalFacecolor_ = color;
    applyFacecolor();
}

void Patch::applyFacecolor() {
    const colors::Spec c = originalFacecolor_ ? *originalFacecolor_ : colors::Spec(rc_.text("patch.facecolor"));
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

void Patch::updateFrom(const Patch& o) {
    // Artist.update_from: of what it copies only the alpha shows in a legend's handle, the one use
    // of this (the transform and the clip are set anew there; a span cannot be hidden or sketched)
    alpha = o.alpha;
    // Patch.update_from: the colours as resolved and the dashes (not the line style's name, nor
    // antialiasing, cap or join), then set_linewidth, which scales the dashes. (It also copies the
    // colours as they were set and the fill: only a later change of alpha or fill would read them.)
    edgecolor_ = o.edgecolor_;
    facecolor_ = o.facecolor_;
    unscaledDashes_ = o.unscaledDashes_;
    setLinewidth(o.linewidth_);
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
    renderer.drawPath(gc, transformPathNonAffine(path(), *t), t->matrix(), face);
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

FancyBboxPatch::FancyBboxPatch(const rc::Params& rc, double x, double y, double width, double height,
                               std::string boxstyle, double pad, std::optional<double> roundingSize, double scale)
    : Patch(rc), mutationScale(scale), x_(x), y_(y), width_(width), height_(height), boxstyle_(std::move(boxstyle)),
      pad_(pad), roundingSize_(roundingSize) {}

Path FancyBboxPatch::path() const {
    // boxstyle(x, y / aspect, width, height / aspect, mutation_scale), then y * aspect
    const double mutationSize = mutationScale;
    const double pad = mutationSize * pad_;
    double x0 = x_, y0 = y_ / mutationAspect, width = width_, height = height_ / mutationAspect;
    std::vector<double> v;
    std::vector<unsigned char> codes;
    if (boxstyle_ == "round") {  // BoxStyle.Round
        const double dr = roundingSize_ && *roundingSize_ ? mutationSize * *roundingSize_ : pad;
        width = width + 2 * pad, height = height + 2 * pad;
        x0 = x0 - pad, y0 = y0 - pad;
        const double x1 = x0 + width, y1 = y0 + height;
        v = {x0 + dr, y0, x1 - dr, y0, x1, y0, x1, y0 + dr, x1, y1 - dr, x1, y1, x1 - dr, y1, x0 + dr, y1,
             x0, y1, x0, y1 - dr, x0, y0 + dr, x0, y0, x0 + dr, y0, x0 + dr, y0};
        codes = {MOVETO, LINETO, CURVE3, CURVE3, LINETO, CURVE3, CURVE3, LINETO, CURVE3, CURVE3, LINETO, CURVE3, CURVE3, CLOSEPOLY};
    } else {  // BoxStyle.Square: Path._create_closed
        width = width + 2 * pad, height = height + 2 * pad;
        x0 = x0 - pad, y0 = y0 - pad;
        const double x1 = x0 + width, y1 = y0 + height;
        v = {x0, y0, x1, y0, x1, y1, x0, y1, x0, y0};
    }
    for (std::size_t i = 1; i < v.size(); i += 2) v[i] = v[i] * mutationAspect;  // * [1, m_aspect]
    return Path::make(std::move(v), std::move(codes), rc_, codes.empty());
}

}  // namespace calculate_plot::engine
