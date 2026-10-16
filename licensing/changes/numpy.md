# Changes to NumPy code (BSD-3-Clause: redistribution keeps the notice)

src/support/pynum.cpp contains, translated to C++:
- npy_divmod from numpy/_core/src/npymath/npy_math_internal.h.src (NumPy 2.5.3), specialised to double;
  floor division and divmod of doubles as NumPy (and CPython, which uses the same algorithm) compute them;
- power_of_ten and the multiply / rint / divide steps of PyArray_Round from
  numpy/_core/src/multiarray/calculation.c, for one double instead of an array.

src/support/histogram.cpp contains, translated to C++ for float64 data (and int64 data, held as doubles,
where the bin rules treat them otherwise):
- histogram, histogram_bin_edges, _get_outer_edges, _get_bin_edges and _search_sorted_inclusive from
  numpy/lib/_histograms_impl.py, with their messages; bins given as a count, as edges or by name, the
  uniform-bin and sorted paths in the same blocks of 65536 values; a rule by name that asks for more than
  100 000 bins is refused (NumPy would make them);
- the bin rules of _hist_bin_selectors (_hist_bin_sqrt, _sturges, _rice, _scott, _stone, _doane, _fd,
  _auto) and _ptp, with Python's and NumPy's operations in their order (Python floats' ** as C's pow);
- _mean, _var and _std from numpy/_core/_methods.py for one-dimensional data (int64 data are cast to float64
  for the sums in buffers of 8192 values, as NumPy's iterator does);
- percentile with the 'linear' method from numpy/lib/_function_base_impl.py (_quantile, _get_indexes,
  _get_gamma, _lerp) for one quantile of finite data, sorted instead of partitioned;
- linspace from numpy/_core/function_base.py, for a scalar start and stop with the endpoint;
- pairwise_sum from numpy/_core/src/umath/loops_utils.h.src, for np.sum of a contiguous float64 array.
