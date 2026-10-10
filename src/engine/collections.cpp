// Derived from Matplotlib 3.11.2: lib/matplotlib/collections.py (Collection, PathCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/collections.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <cmath>
#include <limits>

namespace calculate_plot::engine {

ColorArg ColorArg::of(colors::Spec c) {
    ColorArg a;
    if (const auto* s = std::get_if<std::string>(&c)) {
        std::string lower = *s;
        for (char& ch : lower) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (lower == "none") return a.kind = None, a;
        if (lower == "face") return a.kind = Face, a;
    }
    a.kind = Colors;
    a.colors.push_back(std::move(c));
    return a;
}

Collection::Collection(const rc::Params& rc) : Artist(rc) {
    zorder = 1;
    setFacecolor(ColorArg{});
    setEdgecolor(ColorArg{});
    setLinewidths({});
    antialiaseds_ = {rc.boolean("patch.antialiased")};
    offsetTransform = std::make_shared<transforms::IdentityTransform>();
}

std::vector<colors::Rgba> Collection::toRgbaArray(const ColorArg& c) const {
    if (c.kind == ColorArg::None) return {};
    std::vector<colors::Rgba> out;
    for (const auto& spec : c.colors) out.push_back(*colors::toRgba(spec, alpha, rc_, nullptr));
    return out;
}

void Collection::setFacecolor(const ColorArg& c) {
    originalFacecolor_ = c;
    applyFacecolor();
}

void Collection::applyFacecolor() {
    ColorArg c = originalFacecolor_;
    if (c.kind == ColorArg::Default) c = ColorArg::of(rc_.text("patch.facecolor"));  // _get_default_facecolor
    facecolors_ = toRgbaArray(c);
}

void Collection::setEdgecolor(const ColorArg& c) {
    originalEdgecolor_ = c;
    applyEdgecolor();
}

void Collection::applyEdgecolor() {
    ColorArg c = originalEdgecolor_;
    if (c.kind == ColorArg::Default) {
        // the default edge colour when edges are forced or the face is 'none', else no edge
        if (rc_.boolean("patch.force_edgecolor") || originalFacecolor_.kind == ColorArg::None)
            c = ColorArg::of(rc_.text("patch.edgecolor"));
        else
            c.kind = ColorArg::None;
    }
    edgeIsFace_ = c.kind == ColorArg::Face;
    edgecolors_ = edgeIsFace_ ? std::vector<colors::Rgba>{} : toRgbaArray(c);
}

std::vector<colors::Rgba> Collection::edgecolors() const { return edgeIsFace_ ? facecolors_ : edgecolors_; }

void Collection::setLinewidths(std::vector<double> lw) {
    if (lw.empty()) lw = {rc_.number("patch.linewidth")};  // _get_default_linewidth
    linewidths_ = lw;
    // _bcast_lwls: the dash patterns (all solid so far) scaled by the line widths
    linestyles_.assign(linewidths_.size(), {0.0, std::nullopt});
}

void Collection::setAlpha(std::optional<double> a) {
    alpha = a;
    applyFacecolor();
    applyEdgecolor();
}

Collection::DataLim Collection::dataLim() const {
    // The transform is the identity: the offsets, through offset_trf - transData (the identity),
    // bound the data (Bbox.update_from_data_xy on a null box).
    const double inf = std::numeric_limits<double>::infinity();
    DataLim d{{inf, inf, -inf, -inf}, {inf, inf}};
    for (const auto& o : offsets) {
        if (!std::isfinite(o[0]) || !std::isfinite(o[1])) continue;
        d.points[0] = std::min(d.points[0], o[0]), d.points[2] = std::max(d.points[2], o[0]);
        d.points[1] = std::min(d.points[1], o[1]), d.points[3] = std::max(d.points[3], o[1]);
        if (o[0] > 0) d.minpos[0] = std::min(d.minpos[0], o[0]);
        if (o[1] > 0) d.minpos[1] = std::min(d.minpos[1], o[1]);
    }
    return d;
}

void Collection::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup(groupName, gid);
    if (offsets.empty()) {
        renderer.closeGroup(groupName);
        return;
    }
    beforeDraw();
    updateScalarMappable();
    // _prepare_points: the paths' transform is the identity; non-affine offset transforms apply here
    const transforms::Matrix master{};
    transforms::Matrix offsetTrf = offsetTransform->matrix();
    std::vector<std::array<double, 2>> offs = offsets;
    if (!offsetTransform->isAffine())
        for (auto& o : offs) {
            const auto p = offsetTransform->transformNonAffine(o[0], o[1]);
            o = {p[0], p[1]};
        }
    GC gc = renderer.newGc();
    setGcClip(gc);
    gc.snap = snap;
    if (sketch) gc.sketch = sketch;
    const std::vector<colors::Rgba> faces = facecolors_, edges = edgecolors();
    bool single = false;
    transforms::Matrix combined = master;
    if (paths.size() == 1 && transforms.size() <= 1 && faces.size() == 1 && edges.size() == 1 &&
        linewidths_.size() == 1 && antialiaseds_.size() == 1) {
        if (!transforms.empty()) combined = transforms[0];  // Affine2D(trans[0]) + IdentityTransform
        // extents of the path through the combined transform, against the figure's box: the control
        // points' hull decides when it fits (the curve then fits too)
        double x0 = std::numeric_limits<double>::infinity(), x1 = -x0, y0 = x0, y1 = -x0;
        const auto& v = *paths[0].vertices;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            const auto p = transforms::apply(combined, v[i], v[i + 1]);
            x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]), y0 = std::min(y0, p[1]), y1 = std::max(y1, p[1]);
        }
        const auto fb = figure->bbox->points();
        if (x1 - x0 < fb[2] - fb[0] && y1 - y0 < fb[3] - fb[1]) single = true;
        else renderer.unsupported("collections larger than the figure are not supported yet");
    }
    if (single) {
        gc.setForeground(std::vector<double>(edges[0].begin(), edges[0].end()));
        gc.linewidth = linewidths_[0];
        gc.setDashes(linestyles_[0].first, linestyles_[0].second);
        gc.antialiased = antialiaseds_[0];
        gc.url = url;
        std::vector<double> xy;
        for (const auto& o : offs) xy.push_back(o[0]), xy.push_back(o[1]);
        renderer.drawMarkers(gc, paths[0], combined, Path::make(std::move(xy), {}, rc_), offsetTrf,
                             std::vector<double>(faces[0].begin(), faces[0].end()));
    } else {
        PathCollectionArgs a;
        a.master = master;
        a.transforms = transforms;
        a.offsets = offs;
        a.offsetTrans = offsetTrf;
        a.facecolors = faces;
        a.edgecolors = edges;
        // get_hatchcolor(): 'edge' (hatch.color) follows the edges, or the default edge colour
        if (edges.empty()) a.hatchcolors = {*colors::toRgba(rc_.text("patch.edgecolor"), alpha, rc_, nullptr)};
        else a.hatchcolors = edges;
        a.linewidths = linewidths_;
        a.dashes = linestyles_;
        a.antialiaseds = antialiaseds_;
        a.urls = {url};
        renderer.drawPathCollection(gc, a);  // the probe for the hatchcolors argument (no paths)
        a.paths = paths;
        renderer.drawPathCollection(gc, a);
    }
    renderer.closeGroup(groupName);
}

void Collection::updateScalarMappable() {
    // _set_mappable_flags: faces are mapped unless 'none'; edges when they are 'face', or when the
    // face is 'none' and no edge colour was given
    bool faceMapped = false, edgeMapped = false;
    if (array) {
        if (originalFacecolor_.kind != ColorArg::None) {
            faceMapped = true;
            if (originalEdgecolor_.kind == ColorArg::Face) edgeMapped = true;
        } else if (originalEdgecolor_.kind == ColorArg::Default) {
            edgeMapped = true;
        }
    }
    std::vector<colors::Rgba> mapped;
    if (array) mapped = colors::mapColors(*array, norm, *cmap, alpha);
    if (faceMapped) facecolors_ = mapped;
    else applyFacecolor();
    if (edgeMapped) {
        edgecolors_ = mapped;
        edgeIsFace_ = false;
    } else {
        applyEdgecolor();
    }
}

PathCollection::PathCollection(const rc::Params& rc, std::vector<Path> p, std::vector<double> sizes)
    : Collection(rc), sizes_(std::move(sizes)) {
    groupName = "PathCollection";
    paths = std::move(p);
    setSizes(sizes_, 72.0);
}

void PathCollection::setSizes(const std::vector<double>& sizes, double dpi) {
    sizes_ = sizes;
    transforms.clear();
    for (double s : sizes) {
        const double scale = std::sqrt(s) * dpi / 72.0 * 1.0;  // * _factor
        transforms.push_back(transforms::Matrix{scale, 0.0, 0.0, 0.0, scale, 0.0});
    }
}

void PathCollection::beforeDraw() { setSizes(sizes_, figure->dpi); }

}  // namespace calculate_plot::engine
