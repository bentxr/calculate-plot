// Derived from NumPy 2.5.3: numpy/fft/_pocketfft_umath.cpp (fft_loop, rfft_impl) and numpy/fft/_pocketfft.py
// (_raw_fft with norm=None).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt. Changes: licensing/changes/numpy.md
#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace calculate_plot::support {

// np.fft.fft(a, n) of one transform: a cut or padded with zeros to n values, pocketfft's forward complex transform
// (factor 1). n must be at least 1.
std::vector<std::complex<double>> npFft(const std::vector<std::complex<double>>& a, std::size_t n);
// np.fft.rfft(a, n): the n // 2 + 1 values of the real forward transform
std::vector<std::complex<double>> npRfft(const std::vector<double>& a, std::size_t n);

}  // namespace calculate_plot::support
