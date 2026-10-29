// Derived from Matplotlib 3.11.2: lib/matplotlib/mlab.py (window_hanning, window_none, _spectral_helper,
// _single_spectrum_helper, psd, csd, specgram, cohere, magnitude_spectrum, angle_spectrum, phase_spectrum).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <complex>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine::mlab {

using Complex = std::complex<double>;

// A signal: its values, and whether NumPy holds it as complex (np.iscomplexobj)
struct Signal {
    std::vector<Complex> values;
    bool complex = false;
};

// _spectral_helper's parameters (None: unset)
struct SpectralOptions {
    std::optional<long long> NFFT;
    std::optional<double> Fs;
    std::optional<long long> noverlap, padTo;
    std::string sides = "default";
    std::optional<bool> scaleByFreq;
    std::optional<std::vector<double>> window;  // the window's values; unset: window_hanning (np.hanning)
    std::string mode = "default";
};

// result[frequency][segment], the frequencies and the segments' times
struct Spectrum {
    std::vector<std::vector<Complex>> result;
    std::vector<double> freqs, t;
};

// _spectral_helper(x, y, ...): false and matplotlib's message where it raises
bool spectralHelper(const Signal& x, const Signal* y, const SpectralOptions& o, Spectrum* out, std::string* error);
// csd (psd with y unset: its real part): the mean over the segments (each frequency summed in order, times 1 / n)
bool csd(const Signal& x, const Signal* y, SpectralOptions o, std::vector<Complex>* pxy, std::vector<double>* freqs,
         std::string* error);
// specgram: noverlap 128 and NFFT 256 when unset; the real part unless the mode is 'complex'
bool specgram(const Signal& x, SpectralOptions o, Spectrum* out, std::string* error);
// cohere: |Pxy|^2 / (Pxx Pyy); the signal at least two NFFT long
bool cohere(const Signal& x, const Signal& y, SpectralOptions o, std::vector<double>* cxy, std::vector<double>* freqs,
            std::string* error);
// _single_spectrum_helper(mode, x, Fs, window, pad_to, sides): mode 'magnitude', 'angle' or 'phase' (real values)
bool singleSpectrum(const std::string& mode, const Signal& x, SpectralOptions o, std::vector<double>* spec,
                    std::vector<double>* freqs, std::string* error);

}  // namespace calculate_plot::engine::mlab
