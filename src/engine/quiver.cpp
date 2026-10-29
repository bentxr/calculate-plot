// Derived from Matplotlib 3.11.2: lib/matplotlib/quiver.py (Quiver, QuiverKey, _parse_args).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/quiver.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "engine/lines.hpp"
#include "support/histogram.hpp"
#include "support/npy_complex.hpp"
#include "support/pynum.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::engine {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// np.clip of a float: NaN stays, else between the bounds (NumPy's _NPY_MAX / _NPY_MIN: -0.0 clipped at 0 is 0.0)
double clip(double x, double lo, double hi) {
    if (std::isnan(x)) return x;
    x = x > lo ? x : lo;
    return x < hi ? x : hi;
}

using Complex = support::NpyComplex;
Complex mul(Complex a, Complex b) { return support::npyMultiply(a, b); }  // (NumPy's complex multiplication)
Complex add(double a, Complex b) { return {a + b.re, 0.0 + b.im}; }  // (a float taken as a + 0j)
Complex times1j(double x) { return mul({x, 0.0}, {0.0, 1.0}); }      // x * 1j
Complex exp(Complex z) {
    // cexp (glibc's): exp(re) times the sine and cosine of im (sin = im and cos = 1 for a tiny im)
    double s = z.im, c = 1;
    if (std::fabs(z.im) > std::numeric_limits<double>::min()) s = std::sin(z.im), c = std::cos(z.im);
    const double e = std::exp(z.re);
    return {e * c, e * s};
}

}  // namespace

Quiver::Quiver(const rc::Params& rc, Axes& ax) : PolyCollection(rc, {}), ax_(ax) { groupName = "Quiver"; }

std::unique_ptr<Quiver> Quiver::make(const rc::Params& rc, Axes& ax, std::vector<double> x, std::vector<double> y,
                                     std::vector<double> u, std::vector<double> v, std::optional<std::vector<double>> c,
                                     const QuiverOptions& options, std::string* error) {
    std::unique_ptr<Quiver> q(new Quiver(rc, ax));
    q->o = options;
    std::string pivot = options.pivot;
    for (char& ch : pivot) ch = char(std::tolower((unsigned char)ch));
    if (pivot == "mid") pivot = "middle";
    if (pivot != "tail" && pivot != "middle" && pivot != "tip")
        return *error = "'" + pivot + "' is not a valid value for pivot. Supported values are 'tail', 'middle', 'tip'", nullptr;
    q->o.pivot = pivot;
    const std::size_t n = x.size();
    for (std::size_t i = 0; i < n; ++i) q->xy.push_back({x[i], y[i]});
    // set_UVC: U, V and C masked where not finite; the arrows masked where any of them is
    const auto checkSize = [&](const char* name, std::size_t size) {
        if (size == n) return true;
        if (size == 1) return *error = std::string("one ") + name + " for several arrows is not supported yet", false;
        return *error = std::string("Argument ") + name + " has a size " + std::to_string(size) + " which does not match " +
                        std::to_string(n) + ", the number of arrow positions", false;
    };
    if (!checkSize("U", u.size()) || !checkSize("V", v.size()) || (c && !checkSize("C", c->size()))) return nullptr;
    std::vector<bool> mask(n, false);
    bool masked = false;
    for (std::size_t i = 0; i < n; ++i) {
        mask[i] = !std::isfinite(u[i]) || !std::isfinite(v[i]) || (c && !std::isfinite((*c)[i]));
        masked = masked || mask[i];
        if (!std::isfinite(u[i])) u[i] = 1;  // filled(1)
        if (!std::isfinite(v[i])) v[i] = 1;
    }
    if (masked) q->umask = mask;
    q->u = u, q->v = v;
    if (c) {
        colors::MaskedArray a;
        a.data = *c;
        if (masked) a.mask = mask;
        q->array = a;
    }
    // PolyCollection([], offsets=XY, offset_transform=transData, closed=False, facecolors=color, linewidths=(0,))
    q->offsets = q->xy;
    q->offsetTransform = ax.transData;
    q->setLinewidths({0.0});
    return q;
}

double Quiver::dotsPerUnit(const std::string& units) const {
    // _dots_per_unit: pixels per unit of the Axes' box, view limits, figure
    const auto bb = ax_.bbox->points(), vl = ax_.viewLim->points();
    const double bw = bb[2] - bb[0], bh = bb[3] - bb[1], vw = vl[2] - vl[0], vh = vl[3] - vl[1];
    if (units == "x") return bw / vw;
    if (units == "y") return bh / vh;
    if (units == "xy") return std::hypot(bw, bh) / std::hypot(vw, vh);
    if (units == "width") return bw;
    if (units == "height") return bh;
    if (units == "dots") return 1.;
    return ax_.figure.dpi;  // inches
}

void Quiver::init() {
    // _set_transform: arrow width units to pixels
    const double dx = dotsPerUnit(o.units);
    transScale = dx;
    auto trans = std::make_shared<transforms::Affine2D>();
    trans->scale(dx);
    transform = trans;
    transformSet = true;
    // the span of the Axes' box in width units: trans.inverted().transform_bbox(axes.bbox).width
    const transforms::Matrix inv = transforms::inverse(trans->matrix());
    const auto bb = ax_.bbox->points();
    span_ = transforms::apply(inv, bb[2], bb[3])[0] - transforms::apply(inv, bb[0], bb[1])[0];
    if (!o.width) {
        const double sn = clip(std::sqrt(double(xy.size())), 8, 25);
        o.width = 0.06 * span_ / sn;
    }
    // _make_verts sets the scale when not given
    if (dpiAtLastInit != ax_.figure.dpi && !o.scale) makeVerts(xy, u, v, o.anglesArray ? "" : o.angles, umask);
    dpiAtLastInit = ax_.figure.dpi;
}

std::vector<std::vector<std::array<double, 2>>> Quiver::makeVerts(const std::vector<std::array<double, 2>>& xyIn,
                                                                  const std::vector<double>& uIn, const std::vector<double>& vIn,
                                                                  const std::string& strAngles, const std::vector<bool>& mask) {
    const std::size_t n = uIn.size();
    // uv = U + V * 1j
    std::vector<Complex> uv(n);
    for (std::size_t i = 0; i < n; ++i) uv[i] = add(uIn[i], times1j(vIn[i]));
    // _angles_lengths: the segments from (x, y) to (x, y) + eps (u, v) on the screen
    std::vector<double> angles, lengths;
    const auto anglesLengths = [&](double eps) {
        for (std::size_t i = 0; i < n; ++i) {
            const auto p = ax_.transData->transform(xyIn[i][0], xyIn[i][1]);
            const auto q = ax_.transData->transform(xyIn[i][0] + eps * uIn[i], xyIn[i][1] + eps * vIn[i]);
            const double dx = q[0] - p[0], dy = q[1] - p[1];
            angles.push_back(std::atan2(dy, dx));
            lengths.push_back(std::hypot(dx, dy) / eps);
        }
    };
    const bool xyUnits = o.scaleUnits && *o.scaleUnits == "xy";
    if (strAngles == "xy" && xyUnits) {
        anglesLengths(1);
    } else if (strAngles == "xy" || xyUnits) {
        const auto e = ax_.dataLim->points();
        double m = 0;
        for (double d : e) m = std::max(m, std::fabs(d));
        anglesLengths(m * 0.001);
    }
    std::vector<double> a(n);
    for (std::size_t i = 0; i < n; ++i) a[i] = !strAngles.empty() && xyUnits ? lengths[i] : support::npyAbsolute(uv[i]);
    double autoScale = 0;
    if (!o.scale) {
        // crude auto-scaling: the typical arrow length as a multiple of the arrow width
        const double sn = std::max(10.0, std::sqrt(double(xy.size())));
        std::vector<double> shown;
        for (std::size_t i = 0; i < n; ++i)
            if (mask.empty() || !mask[i]) shown.push_back(a[i]);
        const double amean = support::npMean(shown);
        autoScale = 1.8 * amean * sn / span_;
    }
    double widthuPerLenu = 1.0;
    if (!o.scaleUnits) {
        if (!o.scale) o.scale = autoScale;
    } else {
        const double dx = xyUnits ? 1 : dotsPerUnit(*o.scaleUnits);
        widthuPerLenu = dx / transScale;
        if (!o.scale) o.scale = autoScale * widthuPerLenu;
    }
    std::vector<double> length(n);
    for (std::size_t i = 0; i < n; ++i) length[i] = a[i] * (widthuPerLenu / (*o.scale * *o.width));
    // _h_arrows: each arrow's 8 points in width units, pointing along x
    const double minsh = o.minshaft * o.headlength;
    static const int ii[8] = {0, 1, 2, 3, 2, 1, 0, 0};
    const double y4[4] = {0.5 * 1, 0.5 * 1, 0.5 * o.headwidth, 0.5 * 0};
    const double x04[4] = {0, minsh - o.headaxislength, minsh - o.headlength, minsh};
    std::vector<std::vector<std::array<double, 2>>> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double L = clip(length[i], 0, 65536.0);
        const double x4[4] = {0 + 0 * L, -o.headaxislength + 1 * L, -o.headlength + 1 * L, 0 + 1 * L};
        double X[8], Y[8];
        const double shrink = minsh != 0. ? L / minsh : 0.;
        for (int k = 0; k < 8; ++k) {
            X[k] = x4[ii[k]];
            Y[k] = y4[ii[k]];
            double y0 = y4[ii[k]];
            if (k >= 3 && k < 7) Y[k] *= -1, y0 *= -1;
            if (L < minsh) X[k] = shrink * x04[ii[k]], Y[k] = shrink * y0;
        }
        if (o.pivot == "middle") {
            const double half = 0.5 * X[3];
            for (double& xk : X) xk -= half;
        } else if (o.pivot == "tip") {
            const double tip = X[3];
            for (double& xk : X) xk = xk - tip;
        }
        if (L < o.minlength) {  // a heptagonal dot
            for (int k = 0; k < 8; ++k) {
                const double th = double(k) * (M_PI / 3.0);
                X[k] = std::cos(th) * o.minlength * 0.5;
                Y[k] = std::sin(th) * o.minlength * 0.5;
            }
        }
        // the arrow turned to its angle and scaled by the width: (X + Y * 1j) * np.exp(1j * theta) * width
        double theta;
        if (strAngles == "xy") theta = angles[i];
        else if (strAngles == "uv") theta = std::atan2(uv[i].im, uv[i].re);  // np.angle
        else {
            const double t = (*o.anglesArray)[i] * (M_PI / 180.0);  // np.deg2rad, invalid as 0
            theta = std::isfinite(t) ? t : 0;
        }
        const Complex turn = exp(mul({0.0, 1.0}, {theta, 0.0}));
        for (int k = 0; k < 8; ++k) {
            const Complex p = mul(mul(add(X[k], times1j(Y[k])), turn), {*o.width, 0.0});
            const bool hidden = !mask.empty() && mask[i];
            out[i].push_back({hidden ? kNaN : p.re, hidden ? kNaN : p.im});
        }
    }
    return out;
}

void Quiver::beforeDraw() {
    init();
    // set_verts(verts, closed=False): one open path of 8 points per arrow
    const auto verts = makeVerts(xy, u, v, o.anglesArray ? "" : o.angles, umask);
    paths.clear();
    for (const auto& arrow : verts) {
        std::vector<double> flat;
        for (const auto& p : arrow) flat.push_back(p[0]), flat.push_back(p[1]);
        paths.push_back(Path::make(std::move(flat), {}, rc_));
    }
}

QuiverKey::QuiverKey(const rc::Params& rc, Quiver& q, double x, double y, double u, std::string labelText)
    : Artist(rc), q_(q), x_(x), y_(y), u_(u) {
    vector = std::make_unique<PolyCollection>(rc, std::vector<std::vector<std::array<double, 2>>>{});
    text = std::make_unique<Text>(rc, 0, 0, std::move(labelText));
    zorder = q.zorder + 0.1;
}

void QuiverKey::draw(Renderer& renderer) {
    if (!visible) return;
    static const char* const sides = "NSEW";
    static const char* const pivots[] = {"middle", "middle", "tip", "tail"};
    const std::size_t side = std::string(sides).find(labelpos[0]);
    // _init: the quiver's own initialisation, then the key's arrow made as the quiver makes its arrows
    Axes& ax = *q_.ax();
    if (q_.dpiAtLastInit != ax.figure.dpi) q_.init();
    transform = coordinates == "data" ? ax.transData : coordinates == "figure" ? transforms::TransformPtr(ax.figure.transFigure)
              : coordinates == "inches" ? transforms::TransformPtr(ax.figure.dpiScaleTrans) : ax.transAxes;
    const std::string pivot = q_.o.pivot;
    q_.o.pivot = pivots[side];
    const double radians = angle * (M_PI / 180.0);
    const auto verts = q_.makeVerts({{0., 0.}}, {u_ * std::cos(radians)}, {u_ * std::sin(radians)}, "uv", {});
    q_.o.pivot = pivot;
    vector->paths = PolyCollection(rc_, verts).paths;
    vector->offsets = {{x_, y_}};
    vector->offsetTransform = transform;
    vector->transform = q_.transform;
    vector->transformSet = true;
    vector->figure = &ax.figure;
    if (color) {
        vector->setFacecolor(ColorArg::of(*color));
        vector->setEdgecolor(ColorArg::of(*color));
    }
    vector->draw(renderer);
    // the label beside it, labelsep inches away
    const auto pos = transform->transform(x_, y_);
    const double sep = labelsep * ax.figure.dpi;
    const double shift[4][2] = {{0, +sep}, {0, -sep}, {+sep, 0}, {-sep, 0}};
    text->x = pos[0] + shift[side][0];
    text->y = pos[1] + shift[side][1];
    text->figure = &ax.figure;
    text->draw(renderer);
}

Barbs::Barbs(const rc::Params& rc) : PolyCollection(rc, {}) { groupName = "Barbs"; }

std::unique_ptr<Barbs> Barbs::make(const rc::Params& rc, Axes& ax, std::vector<double> x, std::vector<double> y,
                                   std::vector<double> u, std::vector<double> v, std::optional<std::vector<double>> c,
                                   const BarbsOptions& o, std::string* error) {
    std::unique_ptr<Barbs> b(new Barbs(rc));
    // set_UVC: flip broadcast when one; cbook.delete_masked_points; _check_consistent_shapes
    std::vector<bool> flip = o.flip.size() == 1 ? std::vector<bool>(u.size(), o.flip[0]) : o.flip;
    const std::size_t n = x.size();
    if (y.size() != n || u.size() != n || v.size() != n || (c && c->size() != n) || flip.size() != n)
        return *error = "The shapes of the passed in arrays do not match", nullptr;
    std::vector<std::array<double, 2>> xy;
    std::vector<double> ku, kv, kc;
    std::vector<bool> kflip;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(x[i]) || !std::isfinite(y[i]) || !std::isfinite(u[i]) || !std::isfinite(v[i]) || (c && !std::isfinite((*c)[i]))) continue;
        xy.push_back({x[i], y[i]});
        ku.push_back(u[i]), kv.push_back(v[i]), kflip.push_back(flip[i]);
        if (c) kc.push_back((*c)[i]);
    }
    // the parts of the barbs: _find_tails of the magnitudes
    const double length = o.length;
    const double spacing = length * o.spacing, fullHeight = length * o.height, fullWidth = length * o.width,
                 emptyRad = length * o.emptybarb;
    const double endx = 0.0, endy = o.endy;
    // an empty barb: a CirclePolygon's vertices (unit_regular_polygon(20) through its patch transform, closed), and
    // back over itself unless filled
    std::vector<std::array<double, 2>> emptyBarb;
    {
        transforms::Affine2D patch;
        patch.scale(emptyRad).rotate(0).translate(0, 0);
        const transforms::Matrix m = patch.matrix();
        const Path& unit = unitPolygon20Path();
        for (std::size_t k = 0; k + 1 < unit.size(); ++k) emptyBarb.push_back(transforms::apply(m, (*unit.vertices)[2 * k], (*unit.vertices)[2 * k + 1]));
        emptyBarb.push_back(emptyBarb.front());
        if (!o.fillEmpty) {
            const std::vector<std::array<double, 2>> circ = emptyBarb;
            emptyBarb.insert(emptyBarb.end(), circ.rbegin(), circ.rend());
        }
    }
    std::vector<std::vector<std::array<double, 2>>> barbs;
    for (std::size_t i = 0; i < ku.size(); ++i) {
        double mag = std::hypot(ku[i], kv[i]);
        if (o.rounding) mag = o.half * std::nearbyint(mag / o.half);  // half * np.around(mag / half)
        const auto [nflags, rest1] = support::divmod(mag, o.flag);
        const auto [nbarbs, rest2] = support::divmod(rest1, o.full);
        const bool halfBarb = rest2 >= o.half;
        if (!(halfBarb || nflags > 0 || nbarbs > 0)) {
            barbs.push_back(emptyBarb);
            continue;
        }
        const double angle = -(std::atan2(kv[i], ku[i]) + M_PI / 2);
        std::vector<std::array<double, 2>> poly{{endx, endy}};
        double offset = length;
        const double barbHeight = kflip[i] ? -fullHeight : fullHeight;
        for (long long k = 0; k < (long long)nflags; ++k) {
            if (offset != length) offset += spacing / 2.;
            poly.push_back({endx, endy + offset});
            poly.push_back({endx + barbHeight, endy - fullWidth / 2 + offset});
            poly.push_back({endx, endy - fullWidth + offset});
            offset -= fullWidth + spacing;
        }
        for (long long k = 0; k < (long long)nbarbs; ++k) {
            poly.push_back({endx, endy + offset});
            poly.push_back({endx + barbHeight, endy + offset + fullWidth / 2});
            poly.push_back({endx, endy + offset});
            offset -= spacing;
        }
        if (halfBarb) {
            if (offset == length) {
                poly.push_back({endx, endy + offset});
                offset -= 1.5 * spacing;
            }
            poly.push_back({endx, endy + offset});
            poly.push_back({endx + barbHeight / 2, endy + offset + fullWidth / 4});
            poly.push_back({endx, endy + offset});
        }
        // turned to the vector's direction: Affine2D().rotate(-angle)
        transforms::Affine2D turn;
        turn.rotate(-angle);
        const transforms::Matrix m = turn.matrix();
        for (auto& p : poly) p = transforms::apply(m, p[0], p[1]);
        barbs.push_back(poly);
    }
    // set_verts (closed), the colours' values, the offsets; drawn in points (IdentityTransform) at the offsets
    b->paths = PolyCollection(rc, barbs).paths;
    if (c) {
        colors::MaskedArray a;
        a.data = kc;
        b->array = a;
    }
    b->offsets = xy;
    b->offsetTransform = ax.transData;
    b->transform = std::make_shared<transforms::IdentityTransform>();
    b->transformSet = true;
    b->barbSize_ = length * length / 4;
    return b;
}

void Barbs::beforeDraw() {
    const double scale = std::sqrt(barbSize_) * figure->dpi / 72.0 * 1.0;  // set_sizes((barb_size,), dpi) * _factor
    transforms = {transforms::Matrix{scale, 0.0, 0.0, 0.0, scale, 0.0}};
}

}  // namespace calculate_plot::engine
