// Derived from NumPy 2.5.3: numpy/_core/src/multiarray/number.c (fast_scalar_power: an array raised to
// 2, -1 or 0.5 is squared, inverted or square-rooted) and numpy/_core/src/npymath/npy_math_internal.h.src
// (npy_deg2rad, npy_rad2deg), as the elementwise arithmetic of the functions below.
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt (BSD-3-Clause). Changes: licensing/changes/numpy.md
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::support {

// A function of one variable written as the body of a Python lambda, `lambda x: <text>`, in a restricted
// form: numbers (Python's int and float literals), x, + - * / ** (unary + and - too), parentheses, the
// constants np.pi and np.e and the NumPy functions np.sqrt, np.exp, np.exp2, np.expm1, np.log, np.log2,
// np.log10, np.log1p, np.cbrt, np.sin, np.cos, np.tan, np.arcsin, np.arccos, np.arctan, np.sinh, np.cosh,
// np.arcsinh, np.arctanh, np.abs, np.deg2rad, np.rad2deg, np.radians and np.degrees. It is evaluated as
// Python and NumPy do with x a float64 array: Python's own arithmetic for the parts without x (an error
// where Python raises one), NumPy's elementwise arithmetic for the rest.
class PyLambda {
public:
    // false and why when the text is not of that form
    static std::optional<PyLambda> parse(const std::string& text, std::string* error);
    // f(x) for an array x; false and Python's error where Python raises one. `masked`: x is a masked array
    // whose non-finite values are masked (scatter's offsets): NumPy's masked arithmetic then masks the
    // non-finite results of divisions and powers and the domains of sqrt, log, log2, log10, tan, arcsin, arccos
    // and arctanh; what is masked comes out as NaN.
    bool apply(const std::vector<double>& x, std::vector<double>* out, std::string* error, bool masked = false) const;
    const std::string& text() const { return text_; }
    bool usesNumpy() const { return usesNumpy_; }

    struct Node;

private:
    std::string text_;
    bool usesNumpy_ = false;
    std::shared_ptr<const Node> root_;
};

}  // namespace calculate_plot::support
