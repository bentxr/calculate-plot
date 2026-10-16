// Derived from Matplotlib 3.11.2: lib/matplotlib/collections.py (Collection, PathCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/collections.hpp"
#include "support/histogram.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <algorithm>
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
    hatchLinewidth = rc.number("hatch.linewidth");
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

void Collection::setHatchcolor(const ColorArg& c) { originalHatchcolor_ = c; }

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
        std::string named = st == "-" ? "solid" : st == "--" ? "dashed" : st == "-." ? "dashdot" : st == ":" ? "dotted" : st;
        if (named == "None" || named == "none" || named.empty() || named == " ") named = "solid";  // (undashed, still drawn)
        if (named != "solid" && named != "dashed" && named != "dashdot" && named != "dotted") return false;
        unscaledLinestyles_.push_back(dashPattern(named, rc_));  // mlines._get_dash_patterns
    }
    broadcastLinestyles();
    return true;
}

void Collection::broadcastLinestyles() {
    // _bcast_lwls: both lists repeated to a common length, the dashes scaled by the widths; in the
    // classic style they stay as they are
    std::vector<double> widths = unscaledLinewidths_;
    auto dashes = unscaledLinestyles_;
    if (rc_.boolean("_internal.classic_mode")) {
        linewidths_ = widths;
        linestyles_ = dashes;
        return;
    }
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
    if (hatch && !hatch->empty()) {
        gc.hatch = hatch;
        gc.hatchLinewidth = hatchLinewidth;
    }
    if (sketch) gc.sketch = sketch;
    if (capstyle) gc.capstyle = *capstyle;  // (only when set: the GC's own default otherwise)
    const std::vector<colors::Rgba> faces = facecolors_, edges = edgecolors();
    bool single = false;
    transforms::Matrix combined = master;
    bool solid = true;
    for (const auto& ls : linestyles_) solid = solid && !ls.second;
    if (drawPaths.size() == 1 && transforms.size() <= 1 && faces.size() == 1 && edges.size() == 1 &&
        linewidths_.size() == 1 && solid && antialiaseds_.size() == 1 && !hatch) {
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
        bool finite = false, notFinite = false;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            if (codes && !curved && (*codes)[i / 2] != MOVETO && (*codes)[i / 2] != LINETO) continue;
            const auto p = transforms::apply(combined, v[i], v[i + 1]);
            if (!std::isfinite(p[0]) || !std::isfinite(p[1])) {
                notFinite = true;
                continue;
            }
            finite = true;
            x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]), y0 = std::min(y0, p[1]), y1 = std::max(y1, p[1]);
        }
        const auto fb = figure->bbox->points();
        // A transform that is not finite (a size that is negative, nan or infinite): the extents of
        // straight lines are then nan, and no comparison holds; of a path with curves nothing is left
        // to take them from, and matplotlib breaks.
        if (curved && !finite) renderer.unsupported("need at least one array to concatenate");
        else if (!curved && notFinite) single = false;
        else if (x1 - x0 < fb[2] - fb[0] && y1 - y0 < fb[3] - fb[1]) single = true;
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
        a.hatchcolors = hatchcolors();
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

std::vector<colors::Rgba> Collection::hatchcolors() const {
    // get_hatchcolor(): the colour given (set_color), else hatch.color; 'edge' follows the edges, or
    // the kind's default edge colour without them (a colour takes the collection's alpha; 'none' is no colour)
    ColorArg h = originalHatchcolor_;
    bool followsEdge = false;
    if (h.kind == ColorArg::Default) {
        const std::string named = rc_.text("hatch.color");
        followsEdge = named == "edge";
        if (!followsEdge) h = ColorArg::of(named);
    }
    if (!followsEdge) return toRgbaArray(h);
    const auto edges = edgecolors();
    if (edges.empty()) return {*colors::toRgba(rc_.text(defaults_.edgecolorKey), alpha, rc_, nullptr)};
    return edges;
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
    hatch = o.hatch;
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
                                                     const std::vector<bool>& where, const std::string& step,
                                                     bool yDirection, bool interpolate)
    : PolyCollection(rc, makeVerts(t, f1, f2, where, step, yDirection, interpolate)) {
    groupName = "FillBetweenPolyCollection";
    // _make_verts' box: the points (t, f1) and (t, f2) where drawn, and their minpos
    const double inf = std::numeric_limits<double>::infinity();
    bbox_ = DataLim{{inf, inf, -inf, -inf}, {inf, inf}};
    std::vector<std::array<double, 2>> pts;
    for (std::size_t i = 0; i < t.size(); ++i)
        if (where[i]) pts.push_back({t[i], f1[i]});
    for (std::size_t i = 0; i < t.size(); ++i)
        if (where[i]) pts.push_back({t[i], f2[i]});
    if (yDirection)  // _fix_pts_xy_order
        for (auto& p : pts) std::swap(p[0], p[1]);
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
                                                                                    const std::vector<bool>& where,
                                                                                    const std::string& step, bool yDirection,
                                                                                    bool interpolate) {
    std::vector<std::vector<std::array<double, 2>>> regions;
    // _get_interpolating_points(t, f1, f2, idx): where f1 - f2 is 0 between the points idx - 1 and idx
    // (np.interp of the two, ordered by argsort). t, f1 and f2 are masked where they are not finite: a
    // masked argument sorts as NaN, np.interp takes its data, and a masked difference holds f1's.
    const auto interpolatingPoint = [&](std::size_t idx) -> std::array<double, 2> {
        const std::size_t im1 = idx > 0 ? idx - 1 : 0, end = std::min(idx + 1, t.size());
        const auto masked = [](double v) { return !std::isfinite(v); };
        if (end - im1 == 2) {
            if (masked(f1[idx]) || masked(f2[idx])) return {t[im1], f1[im1]};
            if (masked(f1[im1]) || masked(f2[im1])) return {t[idx], f1[idx]};
        }
        const auto root = [&](double x, const std::vector<double>& xp, const std::vector<bool>& xpMasked,
                              const std::vector<double>& fp) {
            // xp.argsort() (MaskedArray: filled with NaN, which sorts last), then np.interp
            std::vector<std::size_t> order(xp.size());
            for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
            const auto key = [&](std::size_t k) { return xpMasked[k] ? std::nan("") : xp[k]; };
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                const double ka = key(a), kb = key(b);
                return std::isnan(kb) ? !std::isnan(ka) : ka < kb;
            });
            std::vector<double> xs, fs;
            for (std::size_t k : order) xs.push_back(xp[k]), fs.push_back(fp[k]);
            return support::npInterp({x}, xs, fs)[0];
        };
        std::vector<double> tv, diff, f1v;
        std::vector<bool> tMasked, diffMasked;
        for (std::size_t k = im1; k < end; ++k) {
            const bool m = masked(f1[k]) || masked(f2[k]);
            tv.push_back(t[k]), f1v.push_back(f1[k]), diff.push_back(m ? f1[k] : f1[k] - f2[k]);
            tMasked.push_back(masked(t[k])), diffMasked.push_back(m);
        }
        const double rootT = root(0.0, diff, diffMasked, tv);
        return {rootT, root(rootT, tv, tMasked, f1v)};
    };
    // cbook.contiguous_regions(where), then _make_verts_for_region
    std::size_t i = 0;
    while (i < t.size()) {
        if (!where[i]) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < t.size() && where[j]) ++j;
        std::vector<double> ts(t.begin() + std::ptrdiff_t(i), t.begin() + std::ptrdiff_t(j));
        std::vector<double> f1s(f1.begin() + std::ptrdiff_t(i), f1.begin() + std::ptrdiff_t(j));
        std::vector<double> f2s(f2.begin() + std::ptrdiff_t(i), f2.begin() + std::ptrdiff_t(j));
        if (!step.empty()) {  // STEP_LOOKUP_MAP["steps-" + step](t, f1, f2)
            const std::size_t n = ts.size();
            std::vector<double> st, s1, s2;
            for (std::size_t k = 0; k < n; ++k) {
                if (step == "mid") {
                    // pts_to_midstep: x0, the midpoints twice each, the last x; each y twice
                    st.push_back(k == 0 ? ts[0] : (ts[k - 1] + ts[k]) / 2);
                    st.push_back(k + 1 == n ? ts[n - 1] : (ts[k] + ts[k + 1]) / 2);
                    s1.insert(s1.end(), {f1s[k], f1s[k]});
                    s2.insert(s2.end(), {f2s[k], f2s[k]});
                    continue;
                }
                st.push_back(ts[k]), s1.push_back(f1s[k]), s2.push_back(f2s[k]);
                if (k + 1 < n) {  // pts_to_prestep: the y step first; pts_to_poststep: the x step first
                    const bool pre = step == "pre";
                    st.push_back(pre ? ts[k] : ts[k + 1]);
                    s1.push_back(pre ? f1s[k + 1] : f1s[k]);
                    s2.push_back(pre ? f2s[k + 1] : f2s[k]);
                }
            }
            ts = std::move(st), f1s = std::move(s1), f2s = std::move(s2);
        }
        std::vector<std::array<double, 2>> pts;
        // start and end: where the curves cross, or (f2 down to its first and last points) on f2
        pts.push_back(interpolate ? interpolatingPoint(i) : std::array<double, 2>{ts.front(), f2s.front()});
        for (std::size_t k = 0; k < ts.size(); ++k) pts.push_back({ts[k], f1s[k]});
        pts.push_back(interpolate ? interpolatingPoint(j) : std::array<double, 2>{ts.back(), f2s.back()});
        for (std::size_t k = ts.size(); k-- > 0;) pts.push_back({ts[k], f2s[k]});
        if (yDirection)  // _fix_pts_xy_order
            for (auto& p : pts) std::swap(p[0], p[1]);
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
