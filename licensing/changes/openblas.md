# Changes to OpenBLAS code (BSD-3-Clause: redistribution keeps the notice)

src/support/blas_dot.hpp and src/support/blas_dot.cpp contain, translated to C++ from OpenBLAS 0.3.34
(kernel/x86_64/ddot.c with the Haswell kernel kernel/x86_64/ddot_microk_haswell-2.c, the one OpenBLAS
chooses on the machine that made the references):
- the order of ddot's arithmetic for contiguous vectors: blocks of 16 values into four accumulators of four
  lanes with fused multiply-adds (the kernel's vfmadd231pd), their reduction (each accumulator's high lanes
  onto its low ones, the accumulators in pairs, then the two lanes), then the remaining values one by one
  with a multiplication and an addition, not fused;
- the inline assembly is written as plain C++ loops (std::fma for the fused steps); strided vectors, the
  other processors' kernels and the threaded path for more than 10 000 values are not included.
