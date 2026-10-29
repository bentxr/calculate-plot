# Changes to pocketfft

pocketfft's C++ header at commit 33ae5dc94c9cdc7f1c78346504a85de87cadaa12 (the one NumPy 2.5.3 bundles as
numpy/fft/pocketfft) is src/support/pocketfft_hdronly.h, changed as follows (2026-10-29):

- A provenance header (where it comes from, its copyright, its licence file and this file) is put before the
  original comment, which is kept.
- calculate-plot builds without C++ exceptions: every `throw` outside the multithreading code (which is not
  compiled: POCKETFFT_NO_MULTITHREADING) is POCKETFFT_FAIL(message), a macro that stops the program
  (std::abort). The library checks what pocketfft would refuse (zero lengths) before it calls it.
- The algorithms, the twiddle factors and the arithmetic are unchanged. src/support/npy_fft.cpp includes it without
  threads, without the plan cache and without vector lanes.
