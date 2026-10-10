// Derived from Matplotlib 3.11.2: lib/matplotlib/transforms.py (the affine transforms and boxes).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <array>
#include <memory>
#include <vector>

namespace calculate_plot::transforms {

// The first two rows of an affine 3x3 matrix [[sx, shx, tx], [shy, sy, ty], [0, 0, 1]].
struct Matrix {
    double sx = 1, shx = 0, tx = 0, shy = 0, sy = 1, ty = 0;
};

// np.dot(b, a): a, then b, accumulating each entry from +0.0 as NumPy does.
Matrix dot(const Matrix& b, const Matrix& a);
// numpy.linalg.inv of the 3x3 matrix (LU with partial pivoting, as LAPACK's dgesv).
Matrix inverse(const Matrix& m);
// Transform.transform of one point: (sx*x + shx*y) + tx, as matplotlib's C++ affine_transform.
std::array<double, 2> apply(const Matrix& m, double x, double y);

class Transform;
using TransformPtr = std::shared_ptr<const Transform>;

// A box given by two corners (x0, y0) and (x1, y1), possibly reversed.
class BboxBase {
public:
    virtual ~BboxBase() = default;
    virtual std::array<double, 4> points() const = 0;  // x0, y0, x1, y1
    std::array<double, 4> bounds() const;              // x0, y0, width, height
    double width() const;
    double height() const;
};
using BboxPtr = std::shared_ptr<const BboxBase>;

class Bbox : public BboxBase {
public:
    explicit Bbox(std::array<double, 4> points) : points_(points) {}
    static std::shared_ptr<Bbox> fromBounds(double x0, double y0, double width, double height);
    static std::shared_ptr<Bbox> fromExtents(double x0, double y0, double x1, double y1);
    std::array<double, 4> points() const override { return points_; }
    void setPoints(std::array<double, 4> points) { points_ = points; }

private:
    std::array<double, 4> points_;
};

// A box carried through a transform, as TransformedBbox.
class TransformedBbox : public BboxBase {
public:
    TransformedBbox(BboxPtr bbox, TransformPtr transform) : bbox_(std::move(bbox)), transform_(std::move(transform)) {}
    std::array<double, 4> points() const override;

private:
    BboxPtr bbox_;
    TransformPtr transform_;
};

class Transform : public std::enable_shared_from_this<Transform> {
public:
    virtual ~Transform() = default;
    virtual Matrix matrix() const = 0;
    virtual bool isIdentity() const { return false; }
    std::array<double, 2> transform(double x, double y) const { return apply(matrix(), x, y); }
    virtual TransformPtr inverted() const;
};

class Affine2D : public Transform {
public:
    Affine2D() = default;
    explicit Affine2D(const Matrix& m) : m_(m) {}
    Matrix matrix() const override { return m_; }
    Affine2D& scale(double sx, double sy);
    Affine2D& scale(double s) { return scale(s, s); }
    Affine2D& translate(double tx, double ty);
    Affine2D& rotate(double theta);
    Affine2D& rotateDeg(double degrees);

private:
    Matrix m_;
};

class IdentityTransform : public Transform {
public:
    Matrix matrix() const override { return {}; }
    bool isIdentity() const override { return true; }
};

class BboxTransformTo : public Transform {
public:
    explicit BboxTransformTo(BboxPtr boxout) : boxout_(std::move(boxout)) {}
    Matrix matrix() const override;

private:
    BboxPtr boxout_;
};

class BboxTransformFrom : public Transform {
public:
    explicit BboxTransformFrom(BboxPtr boxin) : boxin_(std::move(boxin)) {}
    Matrix matrix() const override;

private:
    BboxPtr boxin_;
};

class BboxTransform : public Transform {
public:
    BboxTransform(BboxPtr boxin, BboxPtr boxout) : boxin_(std::move(boxin)), boxout_(std::move(boxout)) {}
    Matrix matrix() const override;

private:
    BboxPtr boxin_, boxout_;
};

class BlendedAffine2D : public Transform {
public:
    BlendedAffine2D(TransformPtr x, TransformPtr y) : x_(std::move(x)), y_(std::move(y)) {}
    Matrix matrix() const override;

private:
    TransformPtr x_, y_;
};

// a, then b. matplotlib makes a CompositeAffine2D when both are Affine2D objects (inverted: the
// inverse of the product) and a CompositeGenericTransform otherwise (inverted: b inverted, then a
// inverted); the two invert to different last bits.
class CompositeTransform : public Transform {
public:
    CompositeTransform(TransformPtr a, TransformPtr b, bool generic)
        : a_(std::move(a)), b_(std::move(b)), generic_(generic) {}
    Matrix matrix() const override { return dot(b_->matrix(), a_->matrix()); }
    TransformPtr inverted() const override;

private:
    TransformPtr a_, b_;
    bool generic_;
};

class ScaledTranslation : public Transform {
public:
    ScaledTranslation(double xt, double yt, TransformPtr scaleTrans)
        : xt_(xt), yt_(yt), scaleTrans_(std::move(scaleTrans)) {}
    Matrix matrix() const override;

private:
    double xt_, yt_;
    TransformPtr scaleTrans_;
};

// a + b in matplotlib (composite_transform_factory): a, then b; identities drop out.
TransformPtr compose(const TransformPtr& a, const TransformPtr& b);
// blended_transform_factory: x from one transform, y from the other.
TransformPtr blend(const TransformPtr& x, const TransformPtr& y);

}  // namespace calculate_plot::transforms
