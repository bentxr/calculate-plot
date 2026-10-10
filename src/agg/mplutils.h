// Derived from Matplotlib 3.11.2: src/mplutils.h.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

/* Small utilities that are shared by most extension modules. */

#ifndef MPLUTILS_H
#define MPLUTILS_H

#include <array>
#include <cstddef>

#include "ndarray.h"

inline int mpl_round_to_int(double v)
{
    return (int)(v + ((v >= 0.0) ? 0.5 : -0.5));
}

inline double mpl_round(double v)
{
    return (double)mpl_round_to_int(v);
}

// 'kind' codes for paths.
enum {
    STOP = 0,
    MOVETO = 1,
    LINETO = 2,
    CURVE3 = 3,
    CURVE4 = 4,
    CLOSEPOLY = 0x4f
};

// Check that array has shape (N, d1) or (N, d1, d2). Returns false where matplotlib raises.
template<typename T>
inline bool check_trailing_shape(const T &array, long d1)
{
    if (array.ndim() != 2) {
        return false;
    }
    if (array.size() == 0) {
        // Sometimes things come through as atleast_2d, etc., but they're empty, so
        // don't bother enforcing the trailing shape.
        return true;
    }
    return long(array.shape(1)) == d1;
}

template<typename T>
inline bool check_trailing_shape(const T &array, long d1, long d2)
{
    if (array.ndim() != 3) {
        return false;
    }
    if (array.size() == 0) {
        return true;
    }
    return long(array.shape(1)) == d1 && long(array.shape(2)) == d2;
}

using mpl::safe_first_shape;

template <typename T, std::size_t N>
constexpr std::size_t
safe_first_shape(const std::array<T, N> &)
{
    return N;
}

#endif
