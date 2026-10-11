// Derived from Matplotlib 3.11.2: lib/matplotlib/transforms.py (the affine transforms and boxes)
// and src/_path.h (affine_transform).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "transforms/transforms.hpp"

#include "support/log10.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::transforms {

namespace {

// One entry of np.dot: NumPy's BLAS (OpenBLAS, on processors with fused multiply-add: x86-64 since
// about 2013, arm64) accumulates each product fused, in order, from +0.0. The difference from
// rounding each product first only shows when two of the products are inexact, that is with
// rotations or shears.
double dot3(double b0, double a0, double b1, double a1, double b2, double a2) {
    double acc = 0.0;
    acc = std::fma(b0, a0, acc);
    acc = std::fma(b1, a1, acc);
    acc = std::fma(b2, a2, acc);
    return acc;
}

}  // namespace

Matrix dot(const Matrix& b, const Matrix& a) {
    Matrix r;
    r.sx = dot3(b.sx, a.sx, b.shx, a.shy, b.tx, 0.0);
    r.shx = dot3(b.sx, a.shx, b.shx, a.sy, b.tx, 0.0);
    r.tx = dot3(b.sx, a.tx, b.shx, a.ty, b.tx, 1.0);
    r.shy = dot3(b.shy, a.sx, b.sy, a.shy, b.ty, 0.0);
    r.sy = dot3(b.shy, a.shx, b.sy, a.sy, b.ty, 0.0);
    r.ty = dot3(b.shy, a.tx, b.sy, a.ty, b.ty, 1.0);
    return r;
}

bool& singularMatrixSeen() {
    thread_local bool seen = false;
    return seen;
}

Matrix inverse(const Matrix& m) {
    // dgesv(A, I): dgetf2 (LU, partial pivoting, the column scaled by the reciprocal of the pivot,
    // rank-one updates), then dgetrs (forward then back substitution), column by column. The back
    // substitution multiplies by the pivot's reciprocal, as NumPy's OpenBLAS does (dividing differs in
    // the last bit).
    double a[3][3] = {{m.sx, m.shx, m.tx}, {m.shy, m.sy, m.ty}, {0.0, 0.0, 1.0}};
    int piv[3];
    for (int j = 0; j < 3; ++j) {
        int p = j;
        for (int i = j + 1; i < 3; ++i)
            if (std::abs(a[i][j]) > std::abs(a[p][j])) p = i;
        piv[j] = p;
        if (p != j)
            for (int k = 0; k < 3; ++k) std::swap(a[j][k], a[p][k]);
        if (a[j][j] != 0.0) {
            const double r = 1.0 / a[j][j];
            for (int i = j + 1; i < 3; ++i) a[i][j] *= r;
        } else {
            singularMatrixSeen() = true;  // (info > 0: the factor U is exactly singular)
        }
        for (int i = j + 1; i < 3; ++i)
            for (int k = j + 1; k < 3; ++k) a[i][k] -= a[i][j] * a[j][k];
    }
    double x[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int j = 0; j < 3; ++j)  // row interchanges on the right-hand side
        if (piv[j] != j)
            for (int k = 0; k < 3; ++k) std::swap(x[j][k], x[piv[j]][k]);
    for (int c = 0; c < 3; ++c) {
        for (int k = 0; k < 3; ++k)  // L y = b (unit lower)
            if (x[k][c] != 0.0)
                for (int i = k + 1; i < 3; ++i) x[i][c] -= x[k][c] * a[i][k];
        for (int k = 2; k >= 0; --k) {  // U x = y
            if (x[k][c] != 0.0) {
                x[k][c] *= 1.0 / a[k][k];
                for (int i = 0; i < k; ++i) x[i][c] -= x[k][c] * a[i][k];
            }
        }
    }
    return Matrix{x[0][0], x[0][1], x[0][2], x[1][0], x[1][1], x[1][2]};
}

std::array<double, 2> apply(const Matrix& m, double x, double y) {
    double t0 = m.sx * x, t1 = m.shx * y;
    const double rx = t0 + t1 + m.tx;
    t0 = m.shy * x, t1 = m.sy * y;
    return {rx, t0 + t1 + m.ty};
}

std::array<double, 4> BboxBase::bounds() const {
    const auto p = points();
    return {p[0], p[1], p[2] - p[0], p[3] - p[1]};
}
double BboxBase::width() const { return points()[2] - points()[0]; }

Bbox unionOf(const std::vector<Bbox>& boxes) {
    double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const auto p = boxes[i].points();
        const double xmin = std::min(p[0], p[2]), xmax = std::max(p[0], p[2]);
        const double ymin = std::min(p[1], p[3]), ymax = std::max(p[1], p[3]);
        if (i == 0 || xmin < x0) x0 = xmin;
        if (i == 0 || xmax > x1) x1 = xmax;
        if (i == 0 || ymin < y0) y0 = ymin;
        if (i == 0 || ymax > y1) y1 = ymax;
    }
    return Bbox({x0, y0, x1, y1});
}
double BboxBase::height() const { return points()[3] - points()[1]; }

std::shared_ptr<Bbox> Bbox::fromBounds(double x0, double y0, double width, double height) {
    return fromExtents(x0, y0, x0 + width, y0 + height);
}
std::shared_ptr<Bbox> Bbox::fromExtents(double x0, double y0, double x1, double y1) {
    return std::make_shared<Bbox>(std::array<double, 4>{x0, y0, x1, y1});
}

std::array<double, 4> TransformedBbox::points() const {
    // Transform all four corners, then make a box of the result with the same orientation.
    const auto p = bbox_->points();
    const Transform& t = *transform_;
    const std::array<std::array<double, 2>, 4> corners{t.transform(p[0], p[1]), t.transform(p[2], p[1]),
                                                       t.transform(p[0], p[3]), t.transform(p[2], p[3])};
    double xmin = corners[0][0], xmax = corners[0][0], ymin = corners[0][1], ymax = corners[0][1];
    for (const auto& c : corners) {  // Python's min()/max(): the first of equal values
        if (c[0] < xmin) xmin = c[0];
        if (c[0] > xmax) xmax = c[0];
        if (c[1] < ymin) ymin = c[1];
        if (c[1] > ymax) ymax = c[1];
    }
    double xs[2] = {xmin, xmax}, ys[2] = {ymin, ymax};
    if (p[0] > p[2]) std::swap(xs[0], xs[1]);
    if (p[1] > p[3]) std::swap(ys[0], ys[1]);
    return {xs[0], ys[0], xs[1], ys[1]};
}

TransformPtr Transform::inverted() const { return std::make_shared<Affine2D>(inverse(matrix())); }

Affine2D& Affine2D::scale(double sx, double sy) {
    m_.sx *= sx, m_.shx *= sx, m_.tx *= sx;
    m_.shy *= sy, m_.sy *= sy, m_.ty *= sy;
    return *this;
}

Affine2D& Affine2D::translate(double tx, double ty) {
    m_.tx += tx;
    m_.ty += ty;
    return *this;
}

Affine2D& Affine2D::rotate(double theta) {
    const double a = std::cos(theta), b = std::sin(theta);
    const Matrix o = m_;
    m_.sx = a * o.sx - b * o.shy;
    m_.shx = a * o.shx - b * o.sy;
    m_.tx = a * o.tx - b * o.ty;
    m_.shy = b * o.sx + a * o.shy;
    m_.sy = b * o.shx + a * o.sy;
    m_.ty = b * o.tx + a * o.ty;
    return *this;
}

Affine2D& Affine2D::rotateDeg(double degrees) { return rotate(degrees * (3.14159265358979323846 / 180.0)); }

Matrix BboxTransformTo::matrix() const {
    const auto [outl, outb, outw, outh] = boxout_->bounds();
    return Matrix{outw, 0.0, outl, 0.0, outh, outb};
}

Matrix BboxTransformFrom::matrix() const {
    const auto [inl, inb, inw, inh] = boxin_->bounds();
    const double xScale = 1.0 / inw, yScale = 1.0 / inh;
    return Matrix{xScale, 0.0, -inl * xScale, 0.0, yScale, -inb * yScale};
}

Matrix BboxTransform::matrix() const {
    const auto [inl, inb, inw, inh] = boxin_->bounds();
    const auto [outl, outb, outw, outh] = boxout_->bounds();
    const double xScale = outw / inw, yScale = outh / inh;
    return Matrix{xScale, 0.0, (-inl * xScale + outl), 0.0, yScale, (-inb * yScale + outb)};
}

Matrix BlendedAffine2D::matrix() const {
    if (x_ == y_) return x_->matrix();
    const Matrix x = x_->matrix(), y = y_->matrix();
    return Matrix{x.sx, x.shx, x.tx, y.shy, y.sy, y.ty};
}

Matrix ScaledTranslation::matrix() const {
    const auto t = scaleTrans_->transform(xt_, yt_);
    return Matrix{1.0, 0.0, t[0], 0.0, 1.0, t[1]};
}

std::array<double, 2> CompositeTransform::transformNonAffine(double x, double y) const {
    if (a_->isAffine() && b_->isAffine()) return {x, y};
    if (!a_->isAffine() && b_->isAffine()) return a_->transformNonAffine(x, y);
    const auto p = a_->transform(x, y);
    return b_->transformNonAffine(p[0], p[1]);
}

std::array<double, 2> BlendedAffine2D::transformNonAffine(double x, double y) const {
    if (x_->isAffine() && y_->isAffine()) return {x, y};
    // a one-dimensional part transforms its own coordinate; a two-dimensional one gives its column
    const auto one = [](const Transform& t, double v, double* out) {
        if (const auto* log = dynamic_cast<const LogTransform*>(&t)) return *out = log->transform1d(v), true;
        if (const auto* power = dynamic_cast<const InvertedLogTransform*>(&t)) return *out = power->transform1d(v), true;
        return false;
    };
    double rx = 0, ry = 0;
    if (!one(*x_, x, &rx)) rx = x_->transformNonAffine(x, y)[0];
    if (!one(*y_, y, &ry)) ry = y_->transformNonAffine(x, y)[1];
    return {rx, ry};
}

double LogTransform::transform1d(double v) const {
    // np.log10 / np.log2 / np.log(v) / np.log(base); clip: values <= 0 give -1000
    double out;
    if (base_ == 10) out = support::log10(v);
    else if (base_ == 2) out = std::log2(v);
    else out = std::log(v) / std::log(base_);
    if (v <= 0) out = -1000;
    return out;
}

TransformPtr LogTransform::inverted() const { return std::make_shared<InvertedLogTransform>(base_); }

double LogTransform::inverse1d(double v) const {
    // InvertedLogTransform: np.exp2 for base 2, else np.exp(values * np.log(base))
    if (base_ == 2) return std::exp2(v);
    if (base_ == 2.718281828459045) return std::exp(v);
    return std::exp(v * std::log(base_));
}

namespace {
// (self.get_matrix() == other.get_matrix()).all()
bool sameMatrix(const Matrix& a, const Matrix& b) {
    return a.sx == b.sx && a.shx == b.shx && a.tx == b.tx && a.shy == b.shy && a.sy == b.sy && a.ty == b.ty;
}
}  // namespace

std::optional<bool> Transform::eq(const Transform& other) const {
    if (isAffine2DBase()) {  // AffineBase.__eq__
        if (!other.isAffine()) return std::nullopt;
        return sameMatrix(matrix(), other.matrix());
    }
    return std::nullopt;  // object.__eq__ (equal() then compares the objects)
}

bool equal(const Transform& a, const Transform& b) {
    if (const auto r = a.eq(b)) return *r;
    if (const auto r = b.eq(a)) return *r;
    return &a == &b;
}

std::vector<std::pair<TransformPtr, TransformPtr>> Transform::breaks() const {
    return {{std::make_shared<IdentityTransform>(), shared_from_this()}};
}

bool Transform::containsBranch(const Transform& other) const {
    if (depth() < other.depth()) return false;
    for (const auto& [left, subTree] : breaks())
        if (equal(*subTree, other)) return true;
    return false;
}

std::optional<bool> BlendedAffine2D::eq(const Transform& other) const {
    if (const auto* o = dynamic_cast<const BlendedAffine2D*>(&other)) return equal(*x_, *o->x_) && equal(*y_, *o->y_);
    if (equal(*x_, *y_)) return equal(*x_, other);
    return std::nullopt;
}

TransformPtr BlendedAffine2D::inverted() const {
    if (!generic_) return Transform::inverted();
    return std::make_shared<BlendedAffine2D>(x_->inverted(), y_->inverted(), true);
}

std::optional<bool> CompositeTransform::eq(const Transform& other) const {
    if (!generic_) return Transform::eq(other);  // CompositeAffine2D is an Affine2DBase
    if (const auto* o = dynamic_cast<const CompositeTransform*>(&other))
        return this == o || (equal(*a_, *o->a_) && equal(*b_, *o->b_));
    return false;
}

std::vector<std::pair<TransformPtr, TransformPtr>> CompositeTransform::breaks() const {
    std::vector<std::pair<TransformPtr, TransformPtr>> out;
    for (const auto& [left, right] : a_->breaks()) out.emplace_back(left, compose(right, b_));
    for (const auto& [left, right] : b_->breaks()) out.emplace_back(compose(a_, left), right);
    return out;
}

std::array<bool, 2> containsBranchSeparately(const TransformPtr& t, const TransformPtr& other) {
    if (const auto* b = dynamic_cast<const BlendedAffine2D*>(t.get()))
        return {b->x()->containsBranch(*other), b->y()->containsBranch(*other)};
    // CompositeGenericTransform: itself, or what its second part says (a patch's own transform before
    // a blended one keeps the blend's answer)
    if (const auto* c = dynamic_cast<const CompositeTransform*>(t.get()); c && !c->isAffine2DBase()) {
        if (equal(*t, *other)) return {true, true};
        return containsBranchSeparately(c->b(), other);
    }
    const bool c = t->containsBranch(*other);
    return {c, c};
}

TransformPtr subtract(const TransformPtr& a, const TransformPtr& b) {
    for (const auto& [remainder, subTree] : a->breaks())
        if (equal(*subTree, *b)) return remainder;
    for (const auto& [remainder, subTree] : b->breaks())
        if (equal(*subTree, *a)) return remainder->inverted();
    return compose(a, b->inverted());
}

TransformPtr compose(const TransformPtr& a, const TransformPtr& b) {
    if (a->isIdentity()) return b;
    if (b->isIdentity()) return a;
    const bool bothAffine2D = dynamic_cast<const Affine2D*>(a.get()) && dynamic_cast<const Affine2D*>(b.get());
    return std::make_shared<CompositeTransform>(a, b, !bothAffine2D);
}

TransformPtr CompositeTransform::inverted() const {
    if (generic_) return compose(b_->inverted(), a_->inverted());
    return Transform::inverted();
}

TransformPtr blend(const TransformPtr& x, const TransformPtr& y) {
    return std::make_shared<BlendedAffine2D>(x, y, !(x->isAffine2DBase() && y->isAffine2DBase()));
}

}  // namespace calculate_plot::transforms
