// Derived from Matplotlib 3.11.2: lib/matplotlib/collections.py (Collection, PathCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/collections.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <cmath>
#include <limits>
#include <tuple>

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

Collection::Collection(const rc::Params& rc, CollectionDefaults defaults) : Artist(rc), defaults_(std::move(defaults)) {
    zorder = defaults_.zorder;
    setFacecolor(ColorArg{});
    setEdgecolor(ColorArg{});
    setLinewidths({});
    antialiaseds_ = {rc.boolean(defaults_.antialiasedKey)};
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
    if (c.kind == ColorArg::Default)  // _get_default_facecolor
        c = defaults_.facecolorKey.empty() ? ColorArg::of(std::string("none")) : ColorArg::of(rc_.text(defaults_.facecolorKey));
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
        if (rc_.boolean("patch.force_edgecolor") || defaults_.edgeDefault || originalFacecolor_.kind == ColorArg::None)
            c = ColorArg::of(rc_.text(defaults_.edgecolorKey));
        else
            c.kind = ColorArg::None;
    }
    edgeIsFace_ = c.kind == ColorArg::Face;
    edgecolors_ = edgeIsFace_ ? std::vector<colors::Rgba>{} : toRgbaArray(c);
}

std::vector<colors::Rgba> Collection::edgecolors() const { return edgeIsFace_ ? facecolors_ : edgecolors_; }

void Collection::setLinewidths(std::vector<double> lw) {
    if (lw.empty()) lw = {rc_.number(defaults_.linewidthKey)};  // _get_default_linewidth
    unscaledLinewidths_ = lw;
    broadcastLinestyles();
}

bool Collection::setLinestyles(const std::vector<std::string>& styles) {
    unscaledLinestyles_.clear();
    for (const std::string& st : styles) {
        const std::string named = st == "-" ? "solid" : st == "--" ? "dashed" : st == "-." ? "dashdot" : st == ":" ? "dotted" : st;
        if (named != "solid" && named != "dashed" && named != "dashdot" && named != "dotted") return false;
        unscaledLinestyles_.push_back(dashPattern(named, rc_));  // mlines._get_dash_patterns
    }
    broadcastLinestyles();
    return true;
}

void Collection::broadcastLinestyles() {
    // _bcast_lwls: both lists repeated to a common length, the dashes scaled by the widths
    std::vector<double> widths = unscaledLinewidths_;
    auto dashes = unscaledLinestyles_;
    if (dashes.size() != widths.size()) {
        std::size_t a = dashes.size(), b = widths.size(), g = a;
        for (std::size_t r = b; r;) std::tie(g, r) = std::make_pair(r, g % r);  // gcd
        std::vector<double> w2;
        for (std::size_t k = 0; k < a / g; ++k) w2.insert(w2.end(), widths.begin(), widths.end());
        decltype(dashes) d2;
        for (std::size_t k = 0; k < b / g; ++k) d2.insert(d2.end(), dashes.begin(), dashes.end());
        widths = w2, dashes = d2;
    }
    linewidths_ = widths;
    linestyles_.clear();
    for (std::size_t k = 0; k < dashes.size(); ++k) linestyles_.push_back(scaleDashes(dashes[k], widths[k], rc_));
}

void Collection::setAlpha(std::optional<double> a) {
    alpha = a;
    applyFacecolor();
    applyEdgecolor();
}

Collection::DataLim Collection::dataLim(const transforms::TransformPtr&) const {
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
    // _prepare_points: non-affine parts of the transforms apply to the paths and offsets here
    const transforms::Matrix master = transform->matrix();
    std::vector<Path> drawPaths = paths;
    if (!transform->isAffine())
        for (auto& p : drawPaths) p = transformPathNonAffine(p, *transform);
    transforms::Matrix offsetTrf = offsetTransform->matrix();
    std::vector<std::array<double, 2>> offs = offsets;
    if (!offsetTransform->isAffine())
        for (auto& o : offs) {
            const auto p = offsetTransform->transformNonAffine(o[0], o[1]);
            // The offsets in data coordinates are scatter's, a masked array: NumPy masks the logarithm
            // of a non-positive number, and the mask outlives the -1000 written over it;
            // filled(np.nan) then makes it NaN. (Offsets that are plain arrays would keep -1000.)
            for (std::size_t k = 0; k < 2; ++k)
                o[k] = o[k] <= 0 && p[k] == -1000 ? std::numeric_limits<double>::quiet_NaN() : p[k];
        }
    GC gc = renderer.newGc();
    setGcClip(gc);
    gc.snap = snap;
    if (sketch) gc.sketch = sketch;
    const std::vector<colors::Rgba> faces = facecolors_, edges = edgecolors();
    bool single = false;
    transforms::Matrix combined = master;
    bool solid = true;
    for (const auto& ls : linestyles_) solid = solid && !ls.second;
    if (drawPaths.size() == 1 && transforms.size() <= 1 && faces.size() == 1 && edges.size() == 1 &&
        linewidths_.size() == 1 && solid && antialiaseds_.size() == 1) {
        // Affine2D(trans[0]) + transform
        if (!transforms.empty()) combined = transform->isIdentity() ? transforms[0] : transforms::dot(master, transforms[0]);
        // Path.get_extents(combined_transform) against the figure's box. Of a path of straight
        // lines the extents are those of its MOVETO and LINETO vertices; of one with curves they lie
        // within the hull of its control points, which settles it when the hull fits.
        const auto& v = *drawPaths[0].vertices;
        const auto* codes = drawPaths[0].codes.get();
        bool curved = false;
        if (codes)
            for (unsigned char c : *codes) curved = curved || c == CURVE3 || c == CURVE4;
        double x0 = std::numeric_limits<double>::infinity(), x1 = -x0, y0 = x0, y1 = -x0;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            if (codes && !curved && (*codes)[i / 2] != MOVETO && (*codes)[i / 2] != LINETO) continue;
            const auto p = transforms::apply(combined, v[i], v[i + 1]);
            x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]), y0 = std::min(y0, p[1]), y1 = std::max(y1, p[1]);
        }
        const auto fb = figure->bbox->points();
        if (x1 - x0 < fb[2] - fb[0] && y1 - y0 < fb[3] - fb[1]) single = true;
        else if (curved) renderer.unsupported("collections of one curved path larger than the figure are not supported yet");
    }
    if (single) {
        gc.setForeground(std::vector<double>(edges[0].begin(), edges[0].end()));
        gc.linewidth = linewidths_[0];
        gc.setDashes(linestyles_[0].first, linestyles_[0].second);  // (solid: one path is drawn alone only then)
        gc.antialiased = antialiaseds_[0];
        gc.url = url;
        std::vector<double> xy;
        for (const auto& o : offs) xy.push_back(o[0]), xy.push_back(o[1]);
        renderer.drawMarkers(gc, drawPaths[0], combined, Path::make(std::move(xy), {}, rc_), offsetTrf,
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
        a.paths = drawPaths;
        renderer.drawPathCollection(gc, a);
    }
    renderer.closeGroup(groupName);
}

void Collection::updateFrom(const Collection& o) {
    transform = o.transform;
    transformSet = o.transformSet;
    visible = o.visible;
    alpha = o.alpha;
    clipBox = o.clipBox;
    clipOn = o.clipOn;
    clipPath = o.clipPath;
    label = o.label;
    sketch = o.sketch;
    antialiaseds_ = o.antialiaseds_;
    originalEdgecolor_ = o.originalEdgecolor_;
    edgecolors_ = o.edgecolors_;
    edgeIsFace_ = o.edgeIsFace_;
    originalFacecolor_ = o.originalFacecolor_;
    facecolors_ = o.facecolors_;
    linewidths_ = o.linewidths_;
    unscaledLinewidths_ = o.unscaledLinewidths_;
    linestyles_ = o.linestyles_;
    unscaledLinestyles_ = o.unscaledLinestyles_;
    array = o.array;
    norm = o.norm;
    cmap = o.cmap;
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

PolyCollection::PolyCollection(const rc::Params& rc, const std::vector<std::vector<std::array<double, 2>>>& verts)
    : Collection(rc) {
    groupName = "PolyCollection";
    offsets = {{0.0, 0.0}};  // get_offsets() without offsets: one at the origin
    // set_verts(closed=True): each region closed with its first vertex (Path._create_closed)
    for (const auto& xy : verts) {
        std::vector<double> v;
        for (const auto& p : xy) v.push_back(p[0]), v.push_back(p[1]);
        if (xy.empty()) {
            paths.push_back(Path::make({}, {}, rc));
            continue;
        }
        v.push_back(xy[0][0]), v.push_back(xy[0][1]);
        paths.push_back(Path::make(std::move(v), {}, rc, true));
    }
}

FillBetweenPolyCollection::FillBetweenPolyCollection(const rc::Params& rc, const std::vector<double>& t,
                                                     const std::vector<double>& f1, const std::vector<double>& f2,
                                                     const std::vector<bool>& where)
    : PolyCollection(rc, makeVerts(t, f1, f2, where)) {
    groupName = "FillBetweenPolyCollection";
    // _make_verts' box: the points (t, f1) and (t, f2) where drawn, and their minpos
    const double inf = std::numeric_limits<double>::infinity();
    bbox_ = DataLim{{inf, inf, -inf, -inf}, {inf, inf}};
    std::vector<std::array<double, 2>> pts;
    for (std::size_t i = 0; i < t.size(); ++i)
        if (where[i]) pts.push_back({t[i], f1[i]});
    for (std::size_t i = 0; i < t.size(); ++i)
        if (where[i]) pts.push_back({t[i], f2[i]});
    for (const auto& p : pts) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1])) continue;
        bbox_.points[0] = std::min(bbox_.points[0], p[0]), bbox_.points[2] = std::max(bbox_.points[2], p[0]);
        bbox_.points[1] = std::min(bbox_.points[1], p[1]), bbox_.points[3] = std::max(bbox_.points[3], p[1]);
        if (p[0] > 0) bbox_.minpos[0] = std::min(bbox_.minpos[0], p[0]);
        if (p[1] > 0) bbox_.minpos[1] = std::min(bbox_.minpos[1], p[1]);
    }
}

std::vector<std::vector<std::array<double, 2>>> FillBetweenPolyCollection::makeVerts(const std::vector<double>& t,
                                                                                    const std::vector<double>& f1,
                                                                                    const std::vector<double>& f2,
                                                                                    const std::vector<bool>& where) {
    std::vector<std::vector<std::array<double, 2>>> regions;
    // cbook.contiguous_regions(where), then _make_verts_for_region without step or interpolation
    std::size_t i = 0;
    while (i < t.size()) {
        if (!where[i]) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < t.size() && where[j]) ++j;
        std::vector<std::array<double, 2>> pts;
        pts.push_back({t[i], f2[i]});                              // start
        for (std::size_t k = i; k < j; ++k) pts.push_back({t[k], f1[k]});
        pts.push_back({t[j - 1], f2[j - 1]});                      // end
        for (std::size_t k = j; k-- > i;) pts.push_back({t[k], f2[k]});
        regions.push_back(std::move(pts));
        i = j;
    }
    return regions;
}

LineCollection::LineCollection(const rc::Params& rc, const std::vector<std::vector<std::array<double, 2>>>& segments)
    : Collection(rc, CollectionDefaults{"", "lines.color", true, "lines.linewidth", "lines.antialiased", 2}) {
    groupName = "LineCollection";
    setFacecolor(ColorArg::of(std::string("none")));  // kwargs.setdefault('facecolors', 'none')
    offsets = {{0.0, 0.0}};
    for (const auto& seg : segments) {
        std::vector<double> v;
        for (const auto& p : seg) v.push_back(p[0]), v.push_back(p[1]);
        paths.push_back(Path::make(std::move(v), {}, rc));
    }
}

Collection::DataLim LineCollection::dataLim(const transforms::TransformPtr& transData) const {
    // Collection.get_datalim: the paths through the non-affine part of their transform, then
    // through data_trf = transform.get_affine() - transData, and get_path_collection_extents of
    // that: the finite vertices' box. On linear scales data_trf
    // is the identity; on a log scale the vertices go to the scale's space and come back, which
    // changes their last bits and leaves what is not positive at 0.
    // (data_trf's own affine part is the identity for a collection in data coordinates, as all are here)
    const auto dataTrf = transforms::subtract(std::make_shared<transforms::Affine2D>(transform->matrix()), transData);
    const double inf = std::numeric_limits<double>::infinity();
    DataLim d{{inf, inf, -inf, -inf}, {inf, inf}};
    for (const auto& p : paths) {
        const auto& v = *p.vertices;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            std::array<double, 2> q = transform->transformNonAffine(v[i], v[i + 1]);
            q = dataTrf->transformNonAffine(q[0], q[1]);
            if (!std::isfinite(q[0]) || !std::isfinite(q[1])) continue;
            d.points[0] = std::min(d.points[0], q[0]), d.points[2] = std::max(d.points[2], q[0]);
            d.points[1] = std::min(d.points[1], q[1]), d.points[3] = std::max(d.points[3], q[1]);
        }
    }
    // (vlines and hlines give update_datalim the corners alone: the smallest positive values of
    // the box are not used, and not worked out)
    return d;
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
