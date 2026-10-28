// Derived from Matplotlib 3.11.2: lib/matplotlib/contour.py (ContourLabeler) and lib/matplotlib/path.py
// (Path._iter_connected_components, Path.make_compound_path).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/contour_labels.hpp"

#include "axes/ticker.hpp"
#include "engine/axes.hpp"
#include "engine/contour_set.hpp"
#include "engine/figure.hpp"
#include "engine/text.hpp"
#include "rc/rc_params.hpp"
#include "support/histogram.hpp"
#include "support/numpy_str.hpp"
#include "support/pynum.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>

namespace calculate_plot::engine {

namespace {

using XY = std::array<double, 2>;

// A connected component of a path: its vertices and codes
struct Component {
    std::vector<XY> xys;
    std::vector<unsigned char> codes;
};

// Path._iter_connected_components: the path split at its MOVETOs; a path without codes (a level without lines:
// empty) is its one component
std::vector<Component> connectedComponents(const Path& p) {
    std::vector<Component> out;
    const std::size_t n = p.size();
    if (!p.codes) {
        out.emplace_back();
        for (std::size_t i = 0; i < n; ++i) {
            out.back().xys.push_back({(*p.vertices)[2 * i], (*p.vertices)[2 * i + 1]});
            out.back().codes.push_back(i == 0 ? MOVETO : LINETO);
        }
        return out;
    }
    const auto& v = *p.vertices;
    const auto& c = *p.codes;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c[i] == MOVETO || out.empty()) out.emplace_back();
        out.back().xys.push_back({v[2 * i], v[2 * i + 1]});
        out.back().codes.push_back(c[i]);
    }
    return out;
}

// Path.make_compound_path: the components one after the other, without STOP codes; none: an empty path
Path compoundPath(const std::vector<Component>& parts, const rc::Params& rc) {
    std::vector<double> vertices;
    std::vector<unsigned char> codes;
    for (const Component& part : parts)
        for (std::size_t i = 0; i < part.xys.size(); ++i) {
            if (part.codes[i] == STOP) continue;
            vertices.push_back(part.xys[i][0]);
            vertices.push_back(part.xys[i][1]);
            codes.push_back(part.codes[i]);
        }
    Path p = Path::make(std::move(vertices), std::move(codes), rc);
    if (!parts.empty() && !p.codes) p.codes = std::make_shared<std::vector<unsigned char>>();  // (codes of no vertices)
    return p;
}

// np.allclose(a, b): |a - b| <= 1e-8 + 1e-5 * |b| for each coordinate
bool allClose(const XY& a, const XY& b) {
    for (int k = 0; k < 2; ++k)
        if (!(std::fabs(a[std::size_t(k)] - b[std::size_t(k)]) <= 1e-8 + 1e-5 * std::fabs(b[std::size_t(k)]))) return false;
    return true;
}

class Labeler {
public:
    Labeler(ContourSet& cs, Axes& ax, Renderer& renderer, const ClabelOptions& o)
        : cs_(cs), ax_(ax), renderer_(renderer), o_(o), rc_(ax.rc()) {}

    bool run(std::string* error) {
        if (!o_.fmt) {
            // ScalarFormatter(useOffset=False) with a dummy axis (its view interval (0, 1))
            axes::ScalarFormatter::Settings settings;
            settings.useOffset = false;
            settings.offsetThreshold = int(rc_.number("axes.formatter.offset_threshold"));
            const auto limits = rc_.numbers("axes.formatter.limits");
            settings.powerLimits = {int(limits[0]), int(limits[1])};
            settings.useMathText = rc_.boolean("axes.formatter.use_mathtext");
            settings.unicodeMinus = rc_.boolean("axes.unicode_minus");
            formatter_ = axes::ScalarFormatter(settings);
            formatter_.setViewInterval(0, 1);
        }
        zorder_ = o_.zorder ? *o_.zorder : 2 + cs_.zorder;
        // the levels to label and their indices
        if (!o_.levels) {
            levels_ = cs_.levels;
            for (std::size_t i = 0; i < cs_.array->data.size(); ++i) indices_.push_back(i);
        } else {
            for (std::size_t i = 0; i < cs_.levels.size(); ++i)
                if (std::find(o_.levels->begin(), o_.levels->end(), cs_.levels[i]) != o_.levels->end())
                    indices_.push_back(i), levels_.push_back(cs_.levels[i]);
            if (levels_.size() < o_.levels->size()) {
                std::string available = support::numpyStr({cs_.levels});
                available = available.substr(1, available.size() - 2);  // (one dimension)
                return *error = "Specified levels " + o_.levelsText + " don't match available levels " + available, false;
            }
        }
        // the labels' colours: the set's, or the colours given through a ListedColormap and NoNorm
        colors::MaskedArray cvalues;
        cvalues.dtype = cs_.array->dtype;
        if (!o_.colors) {
            for (std::size_t i : indices_) cvalues.data.push_back(cs_.array->data[i]);
            if (!colors::mapColors(cvalues, *cs_.norm, *cs_.cmap, cs_.alpha, &labelColors_, error)) return false;
        } else {
            std::vector<colors::Rgba> seq;
            for (std::size_t i = 0; i < levels_.size(); ++i) seq.push_back((*o_.colors)[i % o_.colors->size()]);
            colors::MaskedArray indices;
            indices.dtype = colors::DType::Int64;
            for (std::size_t i = 0; i < levels_.size(); ++i) indices.data.push_back(double(i));
            colors::NormSpec spec;
            spec.kind = "NoNorm";
            colors::Norm noNorm;
            if (!colors::Norm::make(spec, &noNorm, error)) return false;
            if (!colors::mapColors(indices, noNorm, colors::Colormap::listed(seq, "from_list"), cs_.alpha, &labelColors_, error))
                return false;
        }
        return labels(error);
    }

private:
    // get_text: the formatter's text of the level among all labelled levels, or fmt % level
    bool text(double level, std::string* out, std::string* error) {
        if (o_.fmt) {
            const auto f = support::percentFormat(*o_.fmt, level, false, true);
            if (f.kind == support::Formatted::Unsupported) return *error = "this label format is not supported yet", false;
            if (f.kind == support::Formatted::Raised) return *error = f.text, false;
            return *out = f.text, true;
        }
        std::vector<double> locs = levels_;
        locs.push_back(level);
        formatter_.setLocs(locs);  // format_ticks([*levels, level])[-1]
        *out = formatter_(level);
        return true;
    }

    // _get_nth_label_width: the width of the label's text in pixels, unrotated
    bool labelWidth(std::size_t nth, double* width, std::string* error) {
        std::string s;
        if (!text(levels_[nth], &s, error)) return false;
        Text t(rc_, 0, 0, s);
        t.figure = &ax_.figure;
        if (o_.fontsize) t.font.size = *o_.fontsize;
        *width = t.windowExtent(renderer_).width();
        return true;
    }

    // print_label: whether the contour is long enough to hold the label
    static bool printLabel(const std::vector<XY>& c, double lw) {
        if (double(c.size()) > 10 * lw) return true;
        if (c.empty()) return false;
        for (int k = 0; k < 2; ++k) {
            double lo = c[0][std::size_t(k)], hi = lo;
            for (const XY& p : c) lo = std::min(lo, p[std::size_t(k)]), hi = std::max(hi, p[std::size_t(k)]);
            if (hi - lo > 1.2 * lw) return true;  // np.ptp
        }
        return false;
    }

    // too_close: whether a label is already near
    bool tooClose(double x, double y, double lw) const {
        const double thresh = (1.2 * lw) * (1.2 * lw);
        for (const XY& l : labelXYs_)
            if ((x - l[0]) * (x - l[0]) + (y - l[1]) * (y - l[1]) < thresh) return true;
        return false;
    }

    // locate_label: the middle of the straightest block of the contour (in label widths) not too close to another
    // label, or of the straightest one
    bool locateLabel(const std::vector<XY>& c, double lw, double* x, double* y, std::size_t* index, std::string* error) {
        const std::size_t size = c.size();
        const std::size_t nBlocks = lw > 1 ? std::size_t(std::ceil(double(size) / lw)) : 1;
        const std::size_t blockSize = nBlocks == 1 ? size : std::size_t(lw);
        // np.resize: the contour cycled to fill the blocks
        const auto at = [&](std::size_t b, std::size_t i) { return c[(b * blockSize + i) % size]; };
        std::vector<double> distances(nBlocks);
        for (std::size_t b = 0; b < nBlocks; ++b) {
            const XY first = at(b, 0), last = at(b, blockSize - 1);
            const double l = std::hypot(last[0] - first[0], last[1] - first[1]);
            std::vector<double> d(blockSize);
            for (std::size_t i = 0; i < blockSize; ++i) {
                const XY p = at(b, i);
                const double s = (first[1] - p[1]) * (last[0] - first[0]) - (first[0] - p[0]) * (last[1] - first[1]);
                d[i] = std::fabs(s) / l;
            }
            distances[b] = support::npSum(d);
        }
        // np.argsort: ascending, NaN last. NumPy's sort (x86-simd-sort here) does not keep the order of equal
        // distances, so a choice among equal ones is refused.
        std::vector<std::size_t> order(nBlocks);
        std::iota(order.begin(), order.end(), std::size_t(0));
        const auto key = [&](std::size_t i) { return std::isnan(distances[i]) ? std::numeric_limits<double>::infinity() : distances[i]; };
        const auto same = [&](std::size_t i, std::size_t j) {
            return (std::isnan(distances[i]) && std::isnan(distances[j])) || distances[i] == distances[j];
        };
        std::stable_sort(order.begin(), order.end(), [&](std::size_t i, std::size_t j) {
            if (std::isnan(distances[i]) != std::isnan(distances[j])) return std::isnan(distances[j]);
            return key(i) < key(j);
        });
        const std::size_t half = blockSize / 2;
        const auto place = [&](std::size_t b) {
            const XY p = at(b, half);
            *x = p[0], *y = p[1];
            *index = (b * blockSize + half) % size;
        };
        const char* tie = "contour labels where blocks of the contour are equally straight are not supported yet";
        for (std::size_t g = 0; g < order.size();) {
            std::size_t end = g + 1;
            while (end < order.size() && same(order[g], order[end])) ++end;
            std::vector<std::size_t> free;
            for (std::size_t k = g; k < end; ++k) {
                const XY p = at(order[k], half);
                if (!tooClose(p[0], p[1], lw)) free.push_back(order[k]);
            }
            if (free.size() > 1) return *error = tie, false;
            if (free.size() == 1) return place(free[0]), true;
            g = end;
        }
        // all too close: the straightest block
        if (order.size() > 1 && same(order[0], order[1])) return *error = tie, false;
        place(order[0]);
        return true;
    }

    // _split_path_and_get_label_rotation: the label's angle on the screen, and the component broken around it
    bool splitPath(const Component& path, std::size_t idx, const XY& screen, double lw, double spacing, double* angleOut,
                   Component* out) {
        std::vector<XY> xys = path.xys;
        std::vector<unsigned char> codes = path.codes;
        const transforms::TransformPtr inverse = ax_.transData->inverted();
        const XY pos = inverse->transform(screen[0], screen[1]);
        if (!allClose(pos, xys[idx])) {
            xys.insert(xys.begin() + long(idx), pos);
            codes.insert(codes.begin() + long(idx), LINETO);
        }
        std::size_t start = 0, stop = codes.size();
        for (std::size_t i = 0; i <= idx; ++i)
            if (codes[i] == MOVETO) start = i;
        for (std::size_t i = codes.size(); i-- > idx + 1;)
            if (codes[i] == MOVETO) stop = i;
        std::vector<XY> cc(xys.begin() + long(start), xys.begin() + long(stop));
        idx -= start;
        const bool closed = codes[stop - 1] == CLOSEPOLY;
        if (closed) {  // rotated to start at the label
            std::vector<XY> r(cc.begin() + long(idx), cc.end() - 1);
            r.insert(r.end(), cc.begin(), cc.begin() + long(idx) + 1);
            cc = r;
            idx = 0;
        }
        // cumulative path lengths on the screen as the coordinate along the contour
        std::vector<double> sx, sy, cx, cy, cpl{0.0};
        for (const XY& p : cc) {
            const XY s = ax_.transData->transform(p[0], p[1]);
            sx.push_back(s[0]), sy.push_back(s[1]), cx.push_back(p[0]), cy.push_back(p[1]);
        }
        for (std::size_t i = 1; i < sx.size(); ++i) cpl.push_back(cpl.back() + std::hypot(sx[i] - sx[i - 1], sy[i] - sy[i - 1]));
        const double origin = cpl[idx];
        for (double& v : cpl) v -= origin;
        std::vector<double> target{-lw / 2, lw / 2};
        if (closed) target[0] += cpl.back() - cpl.front();
        const auto ex = support::npInterp(target, cpl, sx), ey = support::npInterp(target, cpl, sy);
        double angle = std::atan2(ey[1] - ey[0], ex[1] - ex[0]) * (180.0 / M_PI);  // np.rad2deg
        if (o_.rightsideUp) angle = support::divmod(angle + 90, 180).second - 90;
        *angleOut = angle;
        target[0] -= spacing, target[1] += spacing;
        std::vector<double> positions(cpl.size());
        std::iota(positions.begin(), positions.end(), 0.0);
        const auto at = support::npInterp(target, cpl, positions, -1.0, -1.0);
        const long i0 = long(std::floor(at[0])), i1 = long(std::ceil(at[1]));
        const auto x01 = support::npInterp(target, cpl, cx), y01 = support::npInterp(target, cpl, cy);
        const long n = long(cc.size());
        // the component broken (zero-length parts dropped)
        Component broken;
        const auto add = [&](const XY& p, unsigned char code) { broken.xys.push_back(p), broken.codes.push_back(code); };
        const auto slice = [&](long a, long b) {  // cc[a:b] as Python slices it (a, b >= 0)
            std::vector<XY> s;
            for (long k = std::min(a, n); k < std::min(b, n); ++k) s.push_back(cc[std::size_t(k)]);
            return s;
        };
        if (closed) {
            if (i0 != -1 && i1 != -1) {
                add({x01[1], y01[1]}, MOVETO);
                for (const XY& p : slice(i1, i0 + 1)) add(p, LINETO);
                add({x01[0], y01[0]}, LINETO);
            }
        } else {
            if (i0 != -1) {  // (i0 + 1 <= len(cc): as many codes as vertices)
                const auto head = slice(0, i0 + 1);
                for (std::size_t k = 0; k < head.size(); ++k) add(head[k], k == 0 ? MOVETO : LINETO);
                add({x01[0], y01[0]}, LINETO);
            }
            if (i1 != -1) {
                add({x01[1], y01[1]}, MOVETO);
                for (const XY& p : slice(i1, n)) add(p, LINETO);
            }
        }
        out->xys.assign(xys.begin(), xys.begin() + long(start));
        out->codes.assign(codes.begin(), codes.begin() + long(start));
        out->xys.insert(out->xys.end(), broken.xys.begin(), broken.xys.end());
        out->codes.insert(out->codes.end(), broken.codes.begin(), broken.codes.end());
        out->xys.insert(out->xys.end(), xys.begin() + long(stop), xys.end());
        out->codes.insert(out->codes.end(), codes.begin() + long(stop), codes.end());
        return true;
    }

    // add_label: a Text at the screen position, in data coordinates, centred and rotated
    bool addLabel(double x, double y, double rotation, double level, std::size_t nth, std::string* error) {
        const XY data = ax_.transData->inverted()->transform(x, y);
        std::string s;
        if (!text(level, &s, error)) return false;
        auto t = std::make_unique<Text>(rc_, data[0], data[1], s);
        t->setRotation(rotation);
        t->horizontalAlignment = "center";
        t->verticalAlignment = "center";
        t->zorder = zorder_;
        t->color = labelColors_[nth];
        if (o_.fontsize) t->font.size = *o_.fontsize;
        labelXYs_.push_back({x, y});
        ax_.addText(std::move(t));
        return true;
    }

    // labels: each labelled level's components long enough get a label at their straightest place
    bool labels(std::string* error) {
        for (std::size_t nth = 0; nth < indices_.size(); ++nth) {
            double lw = 0;
            if (!labelWidth(nth, &lw, error)) return false;
            std::vector<Component> additions;
            Path& path = cs_.paths[indices_[nth]];
            for (const Component& sub : connectedComponents(path)) {
                std::vector<XY> screen;
                for (const XY& p : sub.xys) screen.push_back(ax_.transData->transform(p[0], p[1]));
                if (!printLabel(screen, lw)) {
                    additions.push_back(sub);
                    continue;
                }
                double x = 0, y = 0, rotation = 0;
                std::size_t idx = 0;
                if (!locateLabel(screen, lw, &x, &y, &idx, error)) return false;
                Component broken;
                if (!splitPath(sub, idx, {x, y}, lw, o_.inlineSpacing, &rotation, &broken)) return false;
                if (!addLabel(x, y, rotation, levels_[nth], nth, error)) return false;
                if (o_.inlineLabels) additions.push_back(broken);
            }
            if (o_.inlineLabels) path = compoundPath(additions, rc_);
        }
        return true;
    }

    ContourSet& cs_;
    Axes& ax_;
    Renderer& renderer_;
    const ClabelOptions& o_;
    const rc::Params& rc_;
    axes::ScalarFormatter formatter_;
    double zorder_ = 0;
    std::vector<double> levels_;
    std::vector<std::size_t> indices_;
    std::vector<colors::Rgba> labelColors_;
    std::vector<XY> labelXYs_;
};

}  // namespace

bool clabel(ContourSet& cs, Axes& ax, Renderer& renderer, const ClabelOptions& options, std::string* error) {
    ax.unstaleViewLim();  // (the set's tight autoscaling, which matplotlib does when the set is made)
    Labeler labeler(cs, ax, renderer, options);
    return labeler.run(error);
}

}  // namespace calculate_plot::engine
