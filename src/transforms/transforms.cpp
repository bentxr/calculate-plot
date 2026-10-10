// Derived from Matplotlib 3.11.2: lib/matplotlib/transforms.py (the affine transforms and boxes)
// and src/_path.h (affine_transform).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "transforms/transforms.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::transforms {

namespace {

double dot3(double b0, double a0, double b1, double a1, double b2, double a2) {
    double acc = 0.0;
    acc += b0 * a0;
    acc += b1 * a1;
    acc += b2 * a2;
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
    const Matrix m = transform_->matrix();
    const std::array<std::array<double, 2>, 4> corners{
        apply(m, p[0], p[1]), apply(m, p[2], p[1]), apply(m, p[0], p[3]), apply(m, p[2], p[3])};
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

TransformPtr blend(const TransformPtr& x, const TransformPtr& y) { return std::make_shared<BlendedAffine2D>(x, y); }

}  // namespace calculate_plot::transforms
