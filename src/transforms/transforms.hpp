// Derived from Matplotlib 3.11.2: lib/matplotlib/transforms.py (the affine transforms and boxes).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "support/pylambda.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace calculate_plot::transforms {

// The first two rows of an affine 3x3 matrix [[sx, shx, tx], [shy, sy, ty], [0, 0, 1]].
struct Matrix {
    double sx = 1, shx = 0, tx = 0, shy = 0, sy = 1, ty = 0;
};

// np.dot(b, a): a, then b; each entry accumulates its products fused, from +0.0, as NumPy's BLAS does.
Matrix dot(const Matrix& b, const Matrix& a);
// numpy.linalg.inv of the 3x3 matrix (LU with partial pivoting, as LAPACK's dgesv).
Matrix inverse(const Matrix& m);
// numpy.linalg.inv raises LinAlgError("Singular matrix") where a pivot is exactly zero, and
// matplotlib lets that end the drawing. inverse() cannot raise: it notes it here (per thread) for
// whoever runs the drawing to ask and clear.
bool& singularMatrixSeen();
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

// Bbox.union: the box around the boxes (at least one).
Bbox unionOf(const std::vector<Bbox>& boxes);

// A box carried through a transform, as TransformedBbox.
class TransformedBbox : public BboxBase {
public:
    TransformedBbox(BboxPtr bbox, TransformPtr transform) : bbox_(std::move(bbox)), transform_(std::move(transform)) {}
    std::array<double, 4> points() const override;

private:
    BboxPtr bbox_;
    TransformPtr transform_;
};

// A transform: a non-affine part (identity for affine transforms), then an affine part, whose
// matrix() is matplotlib's get_affine().get_matrix().
class Transform : public std::enable_shared_from_this<Transform> {
public:
    virtual ~Transform() = default;
    virtual Matrix matrix() const = 0;
    virtual bool isIdentity() const { return false; }
    virtual bool isAffine() const { return true; }
    // isinstance(t, Affine2DBase): decides between BlendedAffine2D and BlendedGenericTransform
    virtual bool isAffine2DBase() const { return true; }
    // transform_non_affine of one point (the identity for affine transforms)
    virtual std::array<double, 2> transformNonAffine(double x, double y) const { return {x, y}; }
    std::array<double, 2> transform(double x, double y) const {
        const auto p = transformNonAffine(x, y);
        return apply(matrix(), p[0], p[1]);
    }
    virtual TransformPtr inverted() const;  // of affine transforms

    // __eq__: true or false, or nothing where Python's returns NotImplemented. The affine classes
    // compare matrices with any affine transform (AffineBase.__eq__); the others equal themselves.
    virtual std::optional<bool> eq(const Transform& other) const;
    // depth: how many transforms are chained to make this one
    virtual int depth() const { return 1; }
    // _iter_break_from_left_to_right: (what comes first, the rest) at each place the chain can be cut
    virtual std::vector<std::pair<TransformPtr, TransformPtr>> breaks() const;
    // contains_branch: `other` is a sub-tree of this transform (by transform equality)
    virtual bool containsBranch(const Transform& other) const;
};

// a == b in Python: a's __eq__, then b's, then identity.
bool equal(const Transform& a, const Transform& b);

// LogTransform: a one-dimensional transform (log of its coordinate; non-positive values clip to
// -1000) used for one direction of a blended transform. As a 2-D transform it is only meaningful
// inside BlendedGenericTransform.
class LogTransform : public Transform {
public:
    // `clip`: nonpositive='clip' (-1000 for what is not positive); else 'mask' (NaN and -inf stay)
    explicit LogTransform(double base, bool clip = true) : base_(base), clip_(clip) {}
    Matrix matrix() const override { return {}; }  // get_affine(): IdentityTransform
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const;
    double inverse1d(double v) const;  // InvertedLogTransform
    double base() const { return base_; }
    TransformPtr inverted() const override;

private:
    double base_;
    bool clip_;
};

// InvertedLogTransform: the base to the power of its coordinate; like LogTransform, one direction of
// a blended transform.
class InvertedLogTransform : public Transform {
public:
    explicit InvertedLogTransform(double base) : log_(base) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const { return log_.inverse1d(v); }
    TransformPtr inverted() const override { return std::make_shared<LogTransform>(log_.base()); }

private:
    LogTransform log_;
};

// scale.FuncTransform(forward, inverse): a function of its coordinate (a figure file's {"$lambda": ...}),
// one direction of a blended transform as LogTransform is. The functions are elementwise, so each value is
// transformed alone.
class FuncTransform : public Transform {
public:
    FuncTransform(std::shared_ptr<const support::PyLambda> forward, std::shared_ptr<const support::PyLambda> inverse)
        : forward_(std::move(forward)), inverse_(std::move(inverse)) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const;
    TransformPtr inverted() const override { return std::make_shared<FuncTransform>(inverse_, forward_); }

private:
    std::shared_ptr<const support::PyLambda> forward_, inverse_;
};

// scale.SymmetricalLogTransform(base, linthresh, linscale): linear within linthresh of zero, logarithmic
// outside; one direction of a blended transform, as LogTransform is.
class SymmetricalLogTransform : public Transform {
public:
    SymmetricalLogTransform(double base, double linthresh, double linscale) : base_(base), linthresh_(linthresh), linscale_(linscale) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const;
    double inverse1d(double v) const;  // InvertedSymmetricalLogTransform
    double base() const { return base_; }
    double linthresh() const { return linthresh_; }
    double linscale() const { return linscale_; }
    TransformPtr inverted() const override;

private:
    double base_, linthresh_, linscale_;
};

class InvertedSymmetricalLogTransform : public Transform {
public:
    InvertedSymmetricalLogTransform(double base, double linthresh, double linscale) : forward_(base, linthresh, linscale) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const { return forward_.inverse1d(v); }
    TransformPtr inverted() const override {
        return std::make_shared<SymmetricalLogTransform>(forward_.base(), forward_.linthresh(), forward_.linscale());
    }

private:
    SymmetricalLogTransform forward_;
};

// scale.LogitTransform(nonpositive): log10(p / (1 - p)); 'clip' puts what is outside (0, 1) at -1000 and
// 1000, 'mask' leaves NaN and the infinities; LogisticTransform is its inverse.
class LogitTransform : public Transform {
public:
    explicit LogitTransform(bool clip) : clip_(clip) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const;
    static double inverse1d(double v) { return 1.0 / (1 + std::pow(10.0, -v)); }  // 1 / (1 + 10 ** -v)
    bool clip() const { return clip_; }
    TransformPtr inverted() const override;

private:
    bool clip_;
};

class LogisticTransform : public Transform {
public:
    explicit LogisticTransform(bool clip) : clip_(clip) {}
    Matrix matrix() const override { return {}; }
    bool isAffine() const override { return false; }
    bool isAffine2DBase() const override { return false; }
    double transform1d(double v) const { return LogitTransform::inverse1d(v); }
    TransformPtr inverted() const override { return std::make_shared<LogitTransform>(clip_); }

private:
    bool clip_;
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
    // _boxout, which saving a part of the figure replaces for a while (_tight_bbox.adjust_bbox)
    const BboxPtr& boxout() const { return boxout_; }
    void setBoxout(BboxPtr boxout) { boxout_ = std::move(boxout); }

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

// BlendedAffine2D, or BlendedGenericTransform when either part is not an Affine2DBase (the same
// matrix; a non-affine part, such as a log scale, transforms its own coordinate first).
class BlendedAffine2D : public Transform {
public:
    BlendedAffine2D(TransformPtr x, TransformPtr y, bool generic = false)
        : x_(std::move(x)), y_(std::move(y)), generic_(generic) {}
    Matrix matrix() const override;
    bool isAffine() const override { return x_->isAffine() && y_->isAffine(); }
    bool isAffine2DBase() const override { return !generic_; }
    std::array<double, 2> transformNonAffine(double x, double y) const override;
    const TransformPtr& x() const { return x_; }
    const TransformPtr& y() const { return y_; }
    std::optional<bool> eq(const Transform& other) const override;  // _BlendedMixin.__eq__
    // BlendedGenericTransform.inverted: the blend of the two inverses (BlendedAffine2D inverts its matrix)
    TransformPtr inverted() const override;
    int depth() const override { return generic_ ? std::max(x_->depth(), y_->depth()) : 1; }
    // BlendedGenericTransform never contains a branch
    bool containsBranch(const Transform& other) const override { return generic_ ? false : Transform::containsBranch(other); }

private:
    TransformPtr x_, y_;
    bool generic_;
};

// a, then b. matplotlib makes a CompositeAffine2D when both are Affine2D objects (inverted: the
// inverse of the product) and a CompositeGenericTransform otherwise (inverted: b inverted, then a
// inverted); the two invert to different last bits.
class CompositeTransform : public Transform {
public:
    CompositeTransform(TransformPtr a, TransformPtr b, bool generic)
        : a_(std::move(a)), b_(std::move(b)), generic_(generic) {}
    // get_affine(): b's alone when b is not affine
    Matrix matrix() const override { return b_->isAffine() ? dot(b_->matrix(), a_->matrix()) : b_->matrix(); }
    bool isAffine() const override { return a_->isAffine() && b_->isAffine(); }
    bool isAffine2DBase() const override { return !generic_; }
    std::array<double, 2> transformNonAffine(double x, double y) const override;
    TransformPtr inverted() const override;
    const TransformPtr& a() const { return a_; }
    const TransformPtr& b() const { return b_; }
    std::optional<bool> eq(const Transform& other) const override;  // CompositeGenericTransform.__eq__ when generic
    int depth() const override { return a_->depth() + b_->depth(); }
    std::vector<std::pair<TransformPtr, TransformPtr>> breaks() const override;

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

// TransformWrapper: stands for another transform that can be replaced later (Axes.transScale).
// Never an identity for composition, as in matplotlib (only IdentityTransform drops out).
class TransformWrapper : public Transform {
public:
    explicit TransformWrapper(TransformPtr child) : child_(std::move(child)) {}
    void set(TransformPtr child) { child_ = std::move(child); }
    const TransformPtr& child() const { return child_; }
    Matrix matrix() const override { return child_->matrix(); }
    bool isAffine() const override { return child_->isAffine(); }
    bool isAffine2DBase() const override { return false; }
    std::array<double, 2> transformNonAffine(double x, double y) const override { return child_->transformNonAffine(x, y); }
    TransformPtr inverted() const override { return child_->inverted(); }
    std::optional<bool> eq(const Transform& other) const override { return child_->eq(other); }

private:
    TransformPtr child_;
};

// Transform.contains_branch_separately(other): a blended transform answers per direction; a generic
// composite is `other` or answers as its second part; another answers the same for both.
std::array<bool, 2> containsBranchSeparately(const TransformPtr& t, const TransformPtr& other);
// a - b (Transform.__sub__): what is left of `a` before a sub-tree equal to `b`, or the inverse of
// what `b` has before a sub-tree equal to `a`, or a + b.inverted().
TransformPtr subtract(const TransformPtr& a, const TransformPtr& b);

// a + b in matplotlib (composite_transform_factory): a, then b; identities drop out.
TransformPtr compose(const TransformPtr& a, const TransformPtr& b);
// blended_transform_factory: x from one transform, y from the other.
TransformPtr blend(const TransformPtr& x, const TransformPtr& y);

}  // namespace calculate_plot::transforms
