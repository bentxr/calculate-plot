// Derived from Matplotlib 3.11.2: lib/matplotlib/quiver.py (Quiver, QuiverKey, _parse_args).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/quiver.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "support/histogram.hpp"
#include "support/npy_complex.hpp"

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

}  // namespace calculate_plot::engine
