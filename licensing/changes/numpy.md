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
- median (_median: the mean of the middle values, as np.mean makes it), cov of one row (np.dot of the
  deviations, times the reciprocal of n - 1), and linalg.det of a 1x1 matrix through slogdet (sign * exp(log|u|));
- percentile with the 'linear' method from numpy/lib/_function_base_impl.py (_quantile, _get_indexes,
  _get_gamma, _lerp) for one quantile of finite data, sorted instead of partitioned;
- linspace from numpy/_core/function_base.py, for a scalar start and stop with the endpoint;
- pairwise_sum from numpy/_core/src/umath/loops_utils.h.src, for np.sum of a contiguous float64 array.
- arr_interp (np.interp without period, left or right) and binary_search_with_guess from
  numpy/_core/src/multiarray/compiled_base.c, for float64 arrays; the slopes are computed where they are used
  (NumPy precomputes them when there are fewer of them than points, with the same quotient).
- fast_scalar_power from numpy/_core/src/multiarray/number.c (an array raised to a scalar 2, -1 or 0.5 is
  np.square, np.reciprocal or np.sqrt of it) and npy_deg2rad / npy_rad2deg from
  numpy/_core/src/npymath/npy_math_internal.h.src, in the evaluation of the restricted functions of figure files
  (src/support/pylambda.*: NumPy's elementwise float64 arithmetic and its functions as the C library computes
  them, with Python's own arithmetic for the parts without the array).
- **float32 logarithm** (src/support/npy_logf.cpp): loops_exponent_log.dispatch.c.src's simd_log_FLOAT for AVX2 and
  FMA (the reference machine's np.log of float32 arrays) with npy_simd_data.h's coefficients, translated from vector
  intrinsics to one lane at a time in standard C++ (std::fma for the fused multiply-adds).
- **str() of float arrays** (src/support/numpy_str.cpp): arrayprint.py's array2string and FloatingFormat with the
  default print options (precision 8, floatmode 'maxprec'), for 2-D float64 arrays of finite values whose rows fit
  the line width (no wrapping, no non-finite values), with dragon4.c's rounding: the shortest digits, or the value
  correctly rounded at the precision when those need more (here through pythonRepr, fixed and printf's %.8e).
- **complex arithmetic** (src/support/npy_complex.cpp): loops_arithm_fp.dispatch.c.src's simd_cmul_f64 and
  loops_unary_complex.dispatch.c.src's simd_cabsolute_f64 for AVX2 and FMA (the reference machine's complex128
  multiplication and np.abs), translated from vector intrinsics to one value at a time (std::fma for the fused
  multiply-adds), for quiver's arrows.
