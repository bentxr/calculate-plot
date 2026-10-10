# Changes to NumPy code (BSD-3-Clause: redistribution keeps the notice)

src/support/pynum.cpp contains, translated to C++:
- npy_divmod from numpy/_core/src/npymath/npy_math_internal.h.src (NumPy 2.5.3), specialised to double;
  floor division and divmod of doubles as NumPy (and CPython, which uses the same algorithm) compute them;
- power_of_ten and the multiply / rint / divide steps of PyArray_Round from
  numpy/_core/src/multiarray/calculation.c, for one double instead of an array.
