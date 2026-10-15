# Changes to NumPy code (BSD-3-Clause: redistribution keeps the notice)

src/support/pynum.cpp contains, translated to C++:
- npy_divmod from numpy/_core/src/npymath/npy_math_internal.h.src (NumPy 2.5.3), specialised to double;
  floor division and divmod of doubles as NumPy (and CPython, which uses the same algorithm) compute them;
- power_of_ten and the multiply / rint / divide steps of PyArray_Round from
  numpy/_core/src/multiarray/calculation.c, for one double instead of an array.

src/support/histogram.cpp contains, translated to C++ for float64 data:
- histogram, histogram_bin_edges, _get_outer_edges, _get_bin_edges and _search_sorted_inclusive from
  numpy/lib/_histograms_impl.py, with their messages; bins given as a count or as edges (the rules given by
  name are left out), the uniform-bin and sorted paths in the same blocks of 65536 values;
- linspace from numpy/_core/function_base.py, for a scalar start and stop with the endpoint;
- pairwise_sum from numpy/_core/src/umath/loops_utils.h.src, for np.sum of a contiguous float64 array.
