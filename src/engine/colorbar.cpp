// Derived from Matplotlib 3.11.2: lib/matplotlib/colorbar.py (Colorbar, _ColorbarAxesLocator, _ColorbarSpine,
// make_axes, make_axes_gridspec) and lib/matplotlib/figure.py (Figure.colorbar).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/colorbar.hpp"

#include "axes/ticker.hpp"
#include "engine/axes.hpp"
#include "engine/collections.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/histogram.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::engine {

namespace {

using Box = std::array<double, 4>;  // x0, y0, x1, y1

// _normalize_location_orientation: where the colorbar goes and how it sits
struct LocSettings {
    std::string location, orientation;
    std::array<double, 2> anchor, panchor;
    double pad;
};

bool locationSettings(const std::optional<std::string>& location, const std::optional<std::string>& orientation,
                      LocSettings* out, std::string* error) {
    std::string loc;
    if (location) loc = *location;
    else loc = !orientation || *orientation == "vertical" ? "right" : "bottom";
    if (loc == "left") *out = {"left", "vertical", {1.0, 0.5}, {0.0, 0.5}, 0.10};
    else if (loc == "right") *out = {"right", "vertical", {0.0, 0.5}, {1.0, 0.5}, 0.05};
    else if (loc == "top") *out = {"top", "horizontal", {0.5, 0.0}, {0.5, 1.0}, 0.05};
    else if (loc == "bottom") *out = {"bottom", "horizontal", {0.5, 1.0}, {0.5, 0.0}, 0.15};
    else return *error = "'" + loc + "' is not a valid value for location; supported values are 'left', 'right', 'top', 'bottom'", false;
    if (orientation && *orientation != out->orientation) return *error = "location and orientation are mutually exclusive", false;
    return true;
}

// Bbox helpers: splitx / splity, shrunk, anchored
std::vector<Box> split(const Box& b, const std::vector<double>& fractions, bool x) {
    std::vector<double> f{0};
    f.insert(f.end(), fractions.begin(), fractions.end());
    f.push_back(1);
    std::vector<Box> out;
    const double w = b[2] - b[0], h = b[3] - b[1];
    for (std::size_t i = 0; i + 1 < f.size(); ++i)
        out.push_back(x ? Box{b[0] + f[i] * w, b[1], b[0] + f[i + 1] * w, b[3]} : Box{b[0], b[1] + f[i] * h, b[2], b[1] + f[i + 1] * h});
    return out;
}

Box shrunk(const Box& b, double mx, double my) {
    const double w = b[2] - b[0], h = b[3] - b[1];
    return {b[0], b[1], b[0] + mx * w, b[1] + my * h};
}

Box anchored(const Box& b, const std::array<double, 2>& c, const Box& container) {
    const double l = container[0], bo = container[1], w = container[2] - container[0], h = container[3] - container[1];
    const double L = b[0], B = b[1], W = b[2] - b[0], H = b[3] - b[1];
    const double dx = (l + c[0] * (w - W)) - L, dy = (bo + c[1] * (h - H)) - B;
    return {b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy};
}

// norm(values) of float64 values, masked values as NaN (np.ma.filled(..., np.nan))
std::vector<double> normed(colors::Norm& norm, const std::vector<double>& v, std::string* error) {
    colors::MaskedArray in, out;
    in.data = v;
    if (!norm(in, &out, error)) return {};
    for (std::size_t i = 0; i < out.data.size(); ++i)
        if (out.masked(i)) out.data[i] = std::numeric_limits<double>::quiet_NaN();
    return out.data;
}

std::vector<double> inverseOf(const colors::Norm& norm, const std::vector<double>& v, std::string* error) {
    colors::MaskedArray in, out;
    in.data = v;
    if (!norm.inverse(in, &out, error)) return {};
    return out.data;
}

// _forward_boundaries: the boundaries spread evenly over [0, 1]; values well outside them far out (-1 and 2)
double forwardBoundaries(const std::vector<double>& b, double x) {
    double y = support::npInterp({x}, b, support::npLinspace(0, 1, (long long)b.size()))[0];
    const double eps = (b.back() - b.front()) * 1e-6;
    if (x < b.front() - eps) y = -1;
    if (x > b.back() + eps) y = 2;
    return y;
}

class Colorbar {
public:
    Colorbar(Axes& cax, const Mappable& m, const ColorbarOptions& o, std::optional<double> infoAspect)
        : ax_(cax), m_(m), o_(o), infoAspect_(infoAspect) {}

    bool build(std::string* error) {
        // __init__: the orientation from the location or as given
        const std::optional<std::string> locOrientation =
            !o_.location ? std::nullopt : std::optional<std::string>(*o_.location == "left" || *o_.location == "right" ? "vertical" : "horizontal");
        if (o_.orientation && *o_.orientation != "vertical" && *o_.orientation != "horizontal")
            return *error = "'" + *o_.orientation + "' is not a valid value for orientation; supported values are None, 'vertical', 'horizontal'", false;
        if (o_.spacing != "uniform" && o_.spacing != "proportional")
            return *error = "'" + o_.spacing + "' is not a valid value for spacing; supported values are 'uniform', 'proportional'", false;
        if (locOrientation && o_.orientation && *locOrientation != *o_.orientation)
            return *error = "location and orientation are mutually exclusive", false;
        orientation_ = o_.orientation ? *o_.orientation : locOrientation ? *locOrientation : "vertical";
        vertical_ = orientation_ == "vertical";
        const std::string kind = m_.norm->spec().kind;
        boundaryNorm_ = kind == "BoundaryNorm";
        boundariesGiven_ = o_.boundaries.has_value();
        noNorm_ = kind == "NoNorm";
        extend_ = o_.extend ? *o_.extend : boundaryNorm_ ? m_.norm->spec().extend : "neither";  // (norm.extend)
        if (extend_ != "neither" && extend_ != "both" && extend_ != "min" && extend_ != "max")
            return *error = "'" + extend_ + "' is not a valid value for extend; supported values are 'neither', 'both', 'min', 'max'", false;
        if (kind == "AsinhNorm" || kind == "scale:asinh") return *error = "colorbars of an asinh norm (the asinh scale) are not supported yet", false;
        // the four spines hidden; the outline spine; the dividers (no segments yet)
        for (auto& s : ax_.spines) s->visible = false;
        auto outline = std::make_unique<Polygon>(ax_.rc(), std::vector<double>{0, 0, 0, 0}, true);
        outline->setFacecolor(colors::Spec(std::string("none")));
        outline->setEdgecolor(colors::Spec(ax_.rc().text("axes.edgecolor")));
        outline->setLinewidth(ax_.rc().number("axes.linewidth"));
        outline->capstyle = "projecting";
        outline->zorder = 2.5;
        outline->transform = ax_.transAxes;
        outline->transformSet = true;
        outline_ = outline.get();
        ax_.extraSpines.push_back(std::move(outline));
        auto dividers = std::make_unique<LineCollection>(ax_.rc(), std::vector<std::vector<std::array<double, 2>>>{});
        dividers->setEdgecolor(ColorArg::of(colors::Spec(ax_.rc().text("axes.edgecolor"))));
        dividers->setLinewidths({0.5 * ax_.rc().number("axes.linewidth")});
        dividers->clipOn = false;
        dividers_ = static_cast<LineCollection*>(&ax_.addCollection(std::move(dividers), false));
        // ticklocation, the label, then the scale (_reset_locator_formatter_scale)
        ticklocation_ = o_.ticklocation != "auto" ? o_.ticklocation : o_.location ? *o_.location : vertical_ ? "right" : "bottom";
        longAxis().setLabelText(o_.label, ax_.rc().text(vertical_ ? "yaxis.labellocation" : "xaxis.labellocation"));
        if (!processValues(error)) return false;
        if (boundaryNorm_ || boundariesGiven_) {
            // uniform spacing: the boundaries spread evenly along the axis (a function scale of them)
            if (o_.spacing == "uniform") {
                AxisScaleOptions f;
                const std::shared_ptr<std::vector<double>> b = boundariesShared_;
                f.forwardFn = [b](double x) { return forwardBoundaries(*b, x); };
                f.inverseFn = [b](double x) {
                    return support::npInterp({x}, support::npLinspace(0, 1, (long long)b->size()), *b)[0];
                };
                ax_.setScale(!vertical_, "function", f);
            } else {
                ax_.setScale(!vertical_, "linear");
            }
        } else if (kind == "LogNorm" || kind == "scale:log") {
            AxisScaleOptions log;
            log.logClip = false;  // LogScale(nonpositive='mask')
            ax_.setScale(vertical_ ? false : true, "log", log);
        } else if (kind == "SymLogNorm" || kind == "scale:symlog") {
            AxisScaleOptions s;
            s.base = m_.norm->spec().base;
            s.linthresh = *m_.norm->spec().linthresh;
            s.linscale = m_.norm->spec().linscale;
            ax_.setScale(!vertical_, "symlog", s);
        } else if (kind == "scale:logit") {
            AxisScaleOptions s;
            s.clip = false;
            ax_.setScale(!vertical_, "logit", s);
        } else if (kind == "Normalize" || kind == "scale:linear") {
            ax_.setScale(!vertical_, "linear");
        } else {
            // a norm without a scale of its own: the norm and its inverse as the axis' functions
            AxisScaleOptions f;
            const std::shared_ptr<colors::Norm> norm = m_.norm;
            f.forwardFn = [norm](double x) {
                std::string ignored;
                const auto y = normed(*norm, {x}, &ignored);
                return y.empty() ? std::numeric_limits<double>::quiet_NaN() : y[0];
            };
            f.inverseFn = [norm](double x) {
                std::string ignored;
                const auto y = inverseOf(*norm, {x}, &ignored);
                return y.empty() ? std::numeric_limits<double>::quiet_NaN() : y[0];
            };
            ax_.setScale(!vertical_, "function", f);
        }
        // _draw_all
        if (ax_.rc().boolean(vertical_ ? "ytick.minor.visible" : "xtick.minor.visible"))
            return *error = "colorbars with minor ticks are not supported yet", false;
        if (!longAxis().setLabelPosition(ticklocation_, error) || !longAxis().setTicksPosition(ticklocation_, error)) return false;
        if (!shortAxis().setTicks({}, std::nullopt, error)) return false;
        if (!processValues(error)) return false;
        const auto inside = insideRange();
        vmin_ = boundaries_[inside.first];
        vmax_ = boundaries_[inside.second - 1];
        if (!mesh(error)) return false;
        if (!doExtends(error)) return false;
        double lower = vmin_, upper = vmax_;
        if (inverted()) std::swap(lower, upper);
        if (vertical_) {
            ax_.xaxis->setLim(0, 1, false);
            ax_.yaxis->setLim(lower, upper, false);
        } else {
            ax_.yaxis->setLim(0, 1, false);
            ax_.xaxis->setLim(lower, upper, false);
        }
        // update_ticks: the ticks given, the boundaries (BoundaryNorm, minor ticks there too), the integers between
        // NoNorm's boundaries, or the scale's own locator; the scale's formatter
        Axis& owner = longAxis().tickerAxis();
        if (o_.ticks) {
            owner.fixedLocs = *o_.ticks;
        } else if (boundaryNorm_) {
            owner.fixedLocs = m_.norm->spec().boundaries;
            owner.fixedNbins = 10;
        } else if (noNorm_) {
            owner.indexLocator = std::array<double, 2>{double(1 + int(double(values_.size()) / 10)), 0.5};
        } else if (boundariesGiven_) {
            owner.fixedLocs = std::vector<double>(boundaries_.begin() + long(inside.first), boundaries_.begin() + long(inside.second));
            owner.fixedNbins = 10;
        }
        if (boundaryNorm_) owner.fixedMinorLocs = m_.norm->spec().boundaries;
        if (o_.format) {
            // FormatStrFormatter unless it cannot format 0, then StrMethodFormatter
            const auto probe = support::percentFormat(*o_.format, 0, true);
            if (probe.kind == support::Formatted::Unsupported) return *error = "this colorbar format is not supported yet", false;
            const bool method = probe.kind == support::Formatted::Raised &&
                                (probe.text.rfind("TypeError", 0) == 0 || probe.text.rfind("ValueError", 0) == 0);
            if (probe.kind == support::Formatted::Raised && !method) return *error = probe.text, false;
            owner.strFormat = *o_.format;
            owner.strFormatMethod = method;
        }
        // the solids: pcolormesh of the values between the extensions
        std::size_t first = 0, last = values_.size();
        if (extendLower()) ++first;
        if (extendUpper()) --last;
        if (!addSolids(first, last, error)) return false;
        installLocator(error);
        return error->empty();
    }

    // _ColorbarAxesLocator: the colorbar's box shrunk to leave room for its extensions, with the box aspect of
    // the colorbar made beside its parents scaled alike
    void installLocator(std::string* error) {
        std::vector<double> y;
        std::array<double, 2> extendlen;
        if (extend_ == "neither" || !proportionalY(&y, &extendlen, error)) return;
        if (!extendLower()) extendlen[0] = 0;
        if (!extendUpper()) extendlen[1] = 0;
        const double len = (extendlen[0] + extendlen[1]) + 1;
        const double shrink = 1 / len, offset = extendlen[0] / len;
        Axes* ax = &ax_;
        const bool vertical = vertical_;
        const std::optional<double> aspect = infoAspect_;
        ax_.colorbarLocator = [ax, vertical, aspect, shrink, offset](const std::array<double, 4>& pos) {
            const double w = pos[2] - pos[0], h = pos[3] - pos[1];
            if (vertical) {
                if (aspect) ax->boxAspect = *aspect * shrink;
                const std::array<double, 4> s{pos[0], pos[1], pos[0] + 1 * w, pos[1] + shrink * h};
                return std::array<double, 4>{s[0], s[1] + offset * h, s[2], s[3] + offset * h};
            }
            if (aspect) ax->boxAspect = 1 / (*aspect * shrink);
            const std::array<double, 4> s{pos[0], pos[1], pos[0] + shrink * w, pos[1] + 1 * h};
            return std::array<double, 4>{s[0] + offset * w, s[1], s[2] + offset * w, s[3]};
        };
    }

private:
    Axis& longAxis() { return vertical_ ? *ax_.yaxis : *ax_.xaxis; }
    Axis& shortAxis() { return vertical_ ? *ax_.xaxis : *ax_.yaxis; }
    bool inverted() {  // long_axis.get_inverted()
        const auto v = longAxis().viewInterval();
        return v[1] < v[0];
    }
    bool extendLower() { return extend_ == "both" || extend_ == (inverted() ? "max" : "min"); }
    bool extendUpper() { return extend_ == "both" || extend_ == (inverted() ? "min" : "max"); }
    // _inside: the boundaries without those of the extensions
    std::pair<std::size_t, std::size_t> insideRange() const {
        std::size_t a = 0, b = boundaries_.size();
        if (extend_ == "both" || extend_ == "min") a = 1;
        if (extend_ == "both" || extend_ == "max") b -= 1;
        return {a, b};
    }

    // _get_extension_lengths(frac, automin, automax, default=0.05)
    std::array<double, 2> extensionLengths(double automin, double automax) const {
        if (o_.extendfracAuto) return {automin, automax};
        if (o_.extendfrac) return *o_.extendfrac;
        return {0.05, 0.05};
    }

    // _process_values: the boundaries of the colours (uniform in the norm's space) and their values
    bool processValues(std::string* error) {
        if (o_.values) {
            // values given: the boundaries halfway between them (and extrapolated at the ends), or as given
            values_ = *o_.values;
            if (o_.boundaries) {
                boundaries_ = *o_.boundaries;
            } else {
                const std::size_t n = values_.size();
                boundaries_.assign(n + 1, 0.0);
                for (std::size_t i = 1; i < n; ++i) boundaries_[i] = 0.5 * (values_[i - 1] + values_[i]);
                boundaries_[0] = 2.0 * boundaries_[1] - boundaries_[2];
                boundaries_[n] = 2.0 * boundaries_[n - 1] - boundaries_[n - 2];
            }
            *boundariesShared_ = boundaries_;
            return true;
        }
        const long long N = (long long)m_.cmap->size() + 1;
        std::vector<double> b;
        if (boundariesGiven_ && !boundaryNorm_ && !noNorm_) {
            b = *o_.boundaries;
        } else if (boundaryNorm_) {
            b = m_.norm->spec().boundaries;
        } else if (noNorm_) {
            for (long long i = 0; i < N; ++i) b.push_back(double(i) - .5);  // N blocks centred on integers
        } else {
            b = support::npLinspace(0, 1, N);  // _uniform_y(N)
        }
        if (extendLower()) b.insert(b.begin(), b.front() - 1);
        if (extendUpper()) b.push_back(b.back() + 1);
        colors::Norm& norm = *m_.norm;
        if (m_.array) norm.autoscaleNone(colors::processValue(*m_.array));
        if (!norm.scaled()) norm.setVmin(0), norm.setVmax(1);
        const auto ns = axes::nonsingular(*norm.vmin(), *norm.vmax(), 0.1);
        norm.setVmin(ns.first);
        norm.setVmax(ns.second);
        boundaries_ = boundaryNorm_ || boundariesGiven_ ? b : inverseOf(norm, b, error);
        if (boundaries_.empty()) return false;
        *boundariesShared_ = boundaries_;
        values_.clear();
        for (std::size_t i = 0; i + 1 < boundaries_.size(); ++i) values_.push_back(0.5 * (boundaries_[i] + boundaries_[i + 1]));
        if (noNorm_)
            for (double& v : values_) v = std::trunc(v + 0.00001);  // .astype(np.int16)
        return true;
    }

    // _proportional_y: the inner boundaries normalised to [0, 1], and the extensions' lengths
    bool proportionalY(std::vector<double>* y, std::array<double, 2>* extendlen, std::string* error) {
        const auto inside = insideRange();
        std::vector<double> all, scaled;
        if (boundaryNorm_ || boundariesGiven_) {
            const double b0 = boundaries_[inside.first], b1 = boundaries_[inside.second - 1];
            for (double b : boundaries_) all.push_back((b - b0) / (b1 - b0));
            if (o_.spacing == "uniform")
                for (double b : boundaries_) scaled.push_back(forwardBoundaries(boundaries_, b));
            else
                scaled = all;
        } else {
            all = normed(*m_.norm, boundaries_, error);
            if (all.empty() && !boundaries_.empty()) return false;
            scaled = all;
        }
        // each normalised from 0 to 1 over the inner boundaries
        const auto unit = [&](const std::vector<double>& v, std::vector<double>* out) {
            std::vector<double> in(v.begin() + long(inside.first), v.begin() + long(inside.second));
            colors::NormSpec s;
            s.vmin = in.front();
            s.vmax = in.back();
            colors::Norm n;
            if (!colors::Norm::make(s, &n, error)) return false;
            *out = normed(n, in, error);
            return true;
        };
        std::vector<double> yscaled;
        if (!unit(all, y) || !unit(scaled, &yscaled)) return false;
        const double automin = yscaled[1] - yscaled[0], automax = yscaled.back() - yscaled[yscaled.size() - 2];
        *extendlen = {0, 0};
        if (extendLower() || extendUpper()) *extendlen = extensionLengths(automin, automax);
        return true;
    }

    // _mesh: the corners of the colour cells, the long axis in data
    bool mesh(std::string* error) {
        std::vector<double> y;
        std::array<double, 2> extendlen;
        if (!proportionalY(&y, &extendlen, error)) return false;
        if (boundaryNorm_ || noNorm_ || boundariesGiven_) {  // (not using a norm)
            y_.clear();
            for (double v : y) y_.push_back(v * (vmax_ - vmin_) + vmin_);
            return true;
        }
        colors::Norm n = *m_.norm;  // (vmin and vmax set for a while to the colorbar's)
        n.setVmin(vmin_);
        n.setVmax(vmax_);
        y_ = inverseOf(n, y, error);
        return !y_.empty();
    }

    // _do_extends: the outline, and the extension triangles (or rectangles) as patches in Axes coordinates
    bool doExtends(std::string* error) {
        std::vector<double> y;
        std::array<double, 2> extendlen;
        if (!proportionalY(&y, &extendlen, error)) return false;
        const double bot = 0 - (extendLower() ? extendlen[0] : 0), top = 1 + (extendUpper() ? extendlen[1] : 0);
        std::vector<std::array<double, 2>> xyout =
            o_.extendrect ? std::vector<std::array<double, 2>>{{0, 0}, {0, bot}, {1, bot}, {1, 0}, {1, 1}, {1, top}, {0, top}, {0, 1}, {0, 0}}
                          : std::vector<std::array<double, 2>>{{0, 0}, {0.5, bot}, {1, 0}, {1, 1}, {0.5, top}, {0, 1}, {0, 0}};
        const auto flat = [&](std::vector<std::array<double, 2>> pts) {
            std::vector<double> v;
            for (auto& p : pts) {
                if (!vertical_) std::swap(p[0], p[1]);
                v.push_back(p[0]);
                v.push_back(p[1]);
            }
            return v;
        };
        outline_->setXY(flat(xyout));
        // the extension patches (filled colorbars), just behind the Axes patch's zorder
        const double z = std::nextafter(ax_.patch->zorder, -std::numeric_limits<double>::infinity());
        const auto patch = [&](std::vector<std::array<double, 2>> xy, double value) -> bool {
            auto p = std::make_unique<Polygon>(ax_.rc(), flat(std::move(xy)), false);
            colors::MaskedArray v, out;
            v.data = {value};
            if (noNorm_) v.dtype = colors::DType::Int16;
            if (!(*m_.norm)(v, &out, error)) return false;
            std::vector<colors::Rgba> rgba;
            if (!(*m_.cmap)(out, nullptr, false, &rgba, error)) return false;
            p->setFacecolor(colors::Spec(std::vector<double>(rgba[0].begin(), rgba[0].end())));
            p->setEdgecolor(colors::Spec(ax_.rc().text("patch.edgecolor")));  // (PathPatch's _edge_default)
            p->setAlpha(m_.alpha);
            p->setLinewidth(0.0);
            p->antialiased = false;
            p->transform = ax_.transAxes;
            p->transformSet = true;
            p->clipOn = false;
            p->zorder = z;
            ax_.addPatch(std::move(p));
            return true;
        };
        if (extendLower()) {
            auto xy = o_.extendrect ? std::vector<std::array<double, 2>>{{0, 0}, {0, bot}, {1., bot}, {1, 0}}
                                    : std::vector<std::array<double, 2>>{{0, 0}, {0.5, bot}, {1, 0}};
            if (!patch(xy, inverted() ? values_.back() : values_.front())) return false;
        }
        if (extendUpper()) {
            auto xy = o_.extendrect ? std::vector<std::array<double, 2>>{{0, 1}, {0, top}, {1, top}, {1, 1}}
                                    : std::vector<std::array<double, 2>>{{0, 1}, {0.5, top}, {1, 1}};
            if (!patch(xy, inverted() ? values_.front() : values_.back())) return false;
        }
        return true;
    }

    // _update_dividers: the lines between the colours (drawedges), and at the extensions
    void updateDividers() {
        std::vector<std::vector<std::array<double, 2>>> segments;
        if (o_.drawedges) {
            const auto lims = longAxis().viewInterval();
            std::vector<double> y;
            for (double v : y_)
                if (lims[0] < v && v < lims[1]) y.push_back(v);
            if (extendLower()) y.insert(y.begin(), lims[0]);
            if (extendUpper()) y.push_back(lims[1]);
            for (double v : y)
                segments.push_back(vertical_ ? std::vector<std::array<double, 2>>{{0, v}, {1, v}}
                                             : std::vector<std::array<double, 2>>{{v, 0}, {v, 1}});
        }
        dividers_->setSegments(segments);
    }

    // _add_solids: pcolormesh(X, Y, C, cmap, norm, alpha, edgecolors='none', shading='flat'), rasterized when the
    // edges are not drawn and there are many colours
    bool addSolids(std::size_t first, std::size_t last, std::string* error) {
        const int rows = int(y_.size()), cols = 2;
        std::vector<std::array<double, 2>> coords;
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) coords.push_back(vertical_ ? std::array<double, 2>{double(j), y_[std::size_t(i)]}
                                                                      : std::array<double, 2>{y_[std::size_t(i)], double(j)});
        // (a horizontal colorbar is pcolormesh(Y, X, C): the same grid, x and y swapped)
        auto mesh = std::make_unique<QuadMesh>(ax_.rc(), rows, cols, coords, false, false);
        colors::MaskedArray c;
        for (std::size_t i = first; i < last; ++i) c.data.push_back(values_[i]);
        if (noNorm_) c.dtype = colors::DType::Int16;  // (NoNorm's values are int16: the colormap's indices)
        mesh->array = c;
        mesh->cmap = *m_.cmap;
        mesh->norm = m_.norm;
        mesh->setAlpha(m_.alpha);
        mesh->setEdgecolor(ColorArg{ColorArg::None, {}});
        mesh->snap = ax_.rc().boolean("pcolormesh.snap");
        if (!o_.drawedges && y_.size() >= 50) mesh->rasterized = true;  // n_rasterize
        (void)error;
        double minx = coords[0][0], maxx = minx, miny = coords[0][1], maxy = miny;
        for (const auto& p : coords) minx = std::min(minx, p[0]), maxx = std::max(maxx, p[0]), miny = std::min(miny, p[1]), maxy = std::max(maxy, p[1]);
        mesh->stickyX = {minx, maxx};
        mesh->stickyY = {miny, maxy};
        ax_.addCollection(std::move(mesh), false);
        ax_.updateDatalim(coords);
        ax_.requestAutoscaleView(true, true);
        updateDividers();
        return true;
    }

    Axes& ax_;
    Mappable m_;
    ColorbarOptions o_;
    std::string orientation_, extend_, ticklocation_;
    bool vertical_ = true;
    std::vector<double> boundaries_, values_, y_;
    std::shared_ptr<std::vector<double>> boundariesShared_ = std::make_shared<std::vector<double>>();  // (for the scale)
    bool boundaryNorm_ = false, noNorm_ = false, boundariesGiven_ = false;
    double vmin_ = 0, vmax_ = 1;
    std::optional<double> infoAspect_;
    Polygon* outline_ = nullptr;
    LineCollection* dividers_ = nullptr;
};

}  // namespace

bool makeColorbar(Figure& fig, const Mappable& mappable, Axes* cax, const std::vector<Axes*>& parentsIn, bool useGridspec,
                  const ColorbarOptions& optionsIn, std::string* error) {
    ColorbarOptions o = optionsIn;
    if (!mappable.cmap || !mappable.norm) return *error = "the mappable has no colormap", false;
    if (fig.layoutEngine) return *error = "colorbars with a layout engine are not supported yet", false;
    std::vector<Axes*> parents = parentsIn;
    std::optional<double> infoAspect;
    if (!cax) {
        if (parents.empty()) return *error = "Unable to determine Axes to steal space for Colorbar.", false;
        LocSettings loc;
        if (!locationSettings(o.location, o.orientation, &loc, error)) return false;
        o.orientation = loc.orientation;
        o.ticklocation = loc.location;
        const auto anchor = o.anchor ? *o.anchor : loc.anchor;
        const auto panchor = o.panchor ? *o.panchor : loc.panchor;
        const bool horizontal = loc.location == "top" || loc.location == "bottom";
        Axes* parent = parents.size() == 1 ? parents[0] : nullptr;
        if (useGridspec && parent && parent->subplotSpec) {
            // make_axes_gridspec: the parent's cell split into the parent's part and the colorbar's
            const double pad = o.pad ? *o.pad : loc.pad;
            const double whSpace = 2 * pad / (1 - pad);
            auto gs = std::make_unique<GridSpec>(horizontal ? 2 : 3, horizontal ? 3 : 2);
            gs->parent = *parent->subplotSpec;
            const double f = o.fraction, s = o.shrink;
            if (!horizontal) {
                gs->wspace = whSpace;
                gs->hspace = 0;
                gs->heightRatios = {(1 - anchor[1]) * (1 - s), s, anchor[1] * (1 - s)};
                gs->widthRatios = loc.location == "left" ? std::vector<double>{f, 1 - f - pad} : std::vector<double>{1 - f - pad, f};
            } else {
                gs->hspace = whSpace;
                gs->wspace = 0;
                gs->widthRatios = {anchor[0] * (1 - s), s, (1 - anchor[0]) * (1 - s)};
                gs->heightRatios = loc.location == "top" ? std::vector<double>{f, 1 - f - pad} : std::vector<double>{1 - f - pad, f};
            }
            gs->widthRatiosInt.assign(gs->widthRatios.size(), false);
            gs->heightRatiosInt.assign(gs->heightRatios.size(), false);
            const GridSpec& g = *gs;
            fig.gridspecs.push_back(std::move(gs));
            SubplotSpec main, cb;
            if (loc.location == "left") main = g.span(0, 3, 1, 2), cb = g.cell(1, 0);
            else if (loc.location == "right") main = g.span(0, 3, 0, 1), cb = g.cell(1, 1);
            else if (loc.location == "top") main = g.span(1, 2, 0, 3), cb = g.cell(0, 1);
            else main = g.span(0, 1, 0, 3), cb = g.cell(1, 1);
            parent->setSubplotspec(main);
            if (!o.panchorFalse) parent->anchor = panchor;
            cax = fig.addSubplot(cb);
        } else {
            // make_axes: the parents' box split; the parents shrunk into their part
            const double pad = o.pad ? *o.pad : loc.pad;
            std::vector<transforms::Bbox> boxes;
            for (Axes* a : parents) boxes.push_back(transforms::Bbox(a->originalPosition->points()));
            const auto u = transforms::unionOf(boxes).points();
            const Box pb{u[0], u[1], u[2], u[3]};
            Box pbcb, pb1;
            const double f = o.fraction;
            if (loc.location == "left") {
                const auto parts = split(pb, {f, f + pad}, true);
                pbcb = parts[0], pb1 = parts[2];
            } else if (loc.location == "right") {
                const auto parts = split(pb, {1 - f - pad, 1 - f}, true);
                pb1 = parts[0], pbcb = parts[2];
            } else if (loc.location == "bottom") {
                const auto parts = split(pb, {f, f + pad}, false);
                pbcb = parts[0], pb1 = parts[2];
            } else {
                const auto parts = split(pb, {1 - f - pad, 1 - f}, false);
                pb1 = parts[0], pbcb = parts[2];
            }
            pbcb = horizontal ? anchored(shrunk(pbcb, o.shrink, 1.0), anchor, pbcb) : anchored(shrunk(pbcb, 1.0, o.shrink), anchor, pbcb);
            const transforms::BboxTransform shrinking(std::make_shared<transforms::Bbox>(pb), std::make_shared<transforms::Bbox>(pb1));
            const auto m = shrinking.matrix();
            for (Axes* a : parents) {
                const auto p = a->originalPosition->points();
                const auto q0 = transforms::apply(m, p[0], p[1]), q1 = transforms::apply(m, p[2], p[3]);
                a->setPosition(transforms::Bbox({q0[0], q0[1], q1[0], q1[1]}));
                if (!o.panchorFalse) a->anchor = panchor;
            }
            cax = fig.addAxesBox(pbcb);
        }
        cax->anchor = anchor;
        cax->boxAspect = horizontal ? 1.0 / o.aspect : o.aspect;
        cax->aspect.reset();
        infoAspect = o.aspect;  // cax._colorbar_info['aspect']
        cax->xaxis->grid(false, "both", {}, false);
        cax->yaxis->grid(false, "both", {}, false);
    }
    Colorbar cb(*cax, mappable, o, infoAspect);
    if (!cb.build(error)) return false;
    return true;
}

}  // namespace calculate_plot::engine
