// Derived from NumPy 2.5.3: numpy/fft/_pocketfft_umath.cpp (fft_loop, rfft_impl, copy_input) and
// numpy/fft/_pocketfft.py (_raw_fft with norm=None).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md

#include "support/npy_fft.hpp"

// as NumPy builds it, without threads; no plan cache (shared, it would need a lock: the library draws on several
// threads) and no vector lanes (a batch of transforms gives the same values one at a time)
#define POCKETFFT_NO_MULTITHREADING
#define POCKETFFT_CACHE_SIZE 0
#define POCKETFFT_NO_VECTORS
#include <cstdint>  // (uintptr_t: what NumPy's Python.h brings in before the header)
#include "support/pocketfft_hdronly.h"

namespace calculate_plot::support {

std::vector<std::complex<double>> npFft(const std::vector<std::complex<double>>& a, std::size_t n) {
    // copy_input: the first min(nin, n) values, then zeros
    std::vector<std::complex<double>> buff(n, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < n && i < a.size(); ++i) buff[i] = a[i];
    pocketfft::detail::pocketfft_c<double> plan(n);
    plan.exec(reinterpret_cast<pocketfft::detail::cmplx<double>*>(buff.data()), 1.0, pocketfft::FORWARD);
    return buff;
}

std::vector<std::complex<double>> npRfft(const std::vector<double>& a, std::size_t n) {
    // rfft_impl: the input from the second double of the output on, the real transform there, then R0 from I0
    const std::size_t nout = n / 2 + 1;
    std::vector<std::complex<double>> buff(nout, std::complex<double>(0.0, 0.0));
    double* data = reinterpret_cast<double*>(buff.data());
    const std::size_t room = nout * 2 - 1, used = a.size() < n ? a.size() : n;
    std::size_t i = 0;
    for (; i < used && i < room; ++i) data[1 + i] = a[i];
    for (; i < room; ++i) data[1 + i] = 0;
    pocketfft::detail::pocketfft_r<double> plan(n);
    plan.exec(data + 1, 1.0, pocketfft::FORWARD);
    buff[0] = buff[0].imag();  // I0 -> R0, I0 = 0
    return buff;
}

}  // namespace calculate_plot::support
