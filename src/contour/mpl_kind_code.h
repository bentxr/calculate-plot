// Derived from ContourPy 1.4.0: src/mpl_kind_code.h.
// Copyright (c) 2021-2026, ContourPy Developers. All rights reserved.
// Licence: LICENSES/contourpy.txt. Changes: licensing/changes/contourpy.md

// Enum for kind codes used in Matplotlib Paths.

#ifndef MPL_KIND_CODE
#define MPL_KIND_CODE

namespace contourpy {

typedef enum
{
    MOVETO = 1,
    LINETO = 2,
    CLOSEPOLY = 79
} MplKindCode;

} // namespace contourpy

#endif // MPL_KIND_CODE
