# Changes to ContourPy 1.4.0

calculate-plot uses ContourPy's mpl2014 contour generator, the algorithm matplotlib's contour and contourf use by
default (rcParams["contour.algorithm"]).

- src/contour/mpl2014.h, src/contour/mpl2014.cpp, src/contour/mpl_kind_code.h: ContourPy's src/mpl2014.h,
  src/mpl2014.cpp and src/mpl_kind_code.h with a header naming their origin. The constructor's checks record the
  message in `_invalid` and return instead of throwing (calculate-plot builds without exceptions; the caller checks
  the arguments first); get_chunk_count and get_chunk_size return pairs instead of Python tuples; the output
  arrays are made with their number of dimensions.
- src/contour/common.h, src/contour/contour_generator.h: ContourPy's src/common.h and src/contour_generator.h with
  the pybind11 types replaced by small C++ classes (two-dimensional input arrays; output arrays, lists and tuples
  of std::vector) and the base class reduced to what mpl2014 uses; level checks are made by the caller.
- src/contour/contour.hpp, src/contour/contour.cpp: lib/contourpy/__init__.py's contour_generator and
  _remove_z_mask and lib/contourpy/chunk.py's calc_chunk_sizes, for the arguments matplotlib passes, translated to
  C++.
