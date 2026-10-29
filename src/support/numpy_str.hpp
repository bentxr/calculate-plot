// Derived from NumPy 2.5.3: numpy/_core/arrayprint.py (array2string, FloatingFormat, _formatArray, _extendLine,
// array_repr, array_str) and
// numpy/_core/src/multiarray/dragon4.c (its rounding of positional and scientific digits).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md
#pragma once

#include <string>
#include <vector>

namespace calculate_plot::support {

// str() of a 2-D float64 array of finite values with NumPy's default print options (precision 8, floatmode
// 'maxprec', no suppression), rows that fit its line width of 75 (as a 3 x 3 matrix always does).
std::string numpyStr(const std::vector<std::vector<double>>& rows);

// repr() (asRepr) or str() of a 2-D object array (np.zeros((r, c), dtype=object) filled in) with NumPy's default
// print options, given each element's text (_object_format: repr, or 'list(...)' of a list); rows wrapped at its
// line width as _formatArray wraps them. Arrays of more than 1000 elements (summarized) are not taken.
std::string numpyObjectArray(const std::vector<std::vector<std::string>>& words, bool asRepr);

}  // namespace calculate_plot::support
