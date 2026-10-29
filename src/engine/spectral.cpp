// Derived from Matplotlib 3.11.2: lib/matplotlib/mlab.py (window_hanning, window_none, _spectral_helper,
// _single_spectrum_helper, psd, csd, specgram, cohere, magnitude_spectrum, angle_spectrum, phase_spectrum), with
// NumPy's operations in their order (np.hanning, np.fft.fftfreq, np.unwrap, np.arange of floats, complex products and
// quotients of complex128 arrays).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/spectral.hpp"

#include "support/histogram.hpp"
#include "support/npy_complex.hpp"
#include "support/npy_fft.hpp"
#include "support/pynum.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::engine::mlab {

namespace {

Complex mul(Complex a, Complex b) {  // NumPy's complex128 multiplication (AVX2, fused)
    const auto r = support::npyMultiply({a.real(), a.imag()}, {b.real(), b.imag()});
    return {r.re, r.im};
}

Complex divReal(Complex a, double d) {  // a / (d + 0j)
    const auto r = support::npyDivide({a.real(), a.imag()}, {d, 0.0});
    return {r.re, r.im};
}

// np.hanning(M): 0.5 + 0.5 * cos(pi * n / (M - 1)) for n = 1 - M, 3 - M, ..., M - 1
std::vector<double> hanning(long long m) {
    if (m < 1) return {};
    if (m == 1) return {1.0};
    std::vector<double> w;
    for (long long n = 1 - m; n < m; n += 2) w.push_back(0.5 + 0.5 * std::cos(M_PI * double(n) / double(m - 1)));
    return w;
}

// np.arange(start, stop, step) of floats: start + i * (start + step - start)
std::vector<double> arangeFloat(double start, double stop, double step) {
    std::vector<double> out;
    const double len = std::ceil((stop - start) / step);
    if (!(len > 0)) return out;
    const double delta = (start + step) - start;
    for (long long i = 0; i < (long long)len; ++i) out.push_back(i == 0 ? start : i == 1 ? start + step : start + double(i) * delta);
    return out;
}

// the real part of np.sum of a complex128 array whose imaginary parts are zero (pairwise_sum of CDOUBLE: four complex
// accumulators in blocks of up to 64 values; 0 + the sum)
double complexSum(const std::vector<double>& v, std::size_t begin, std::size_t end) {
    const std::size_t n = (end - begin) * 2;  // (counted in doubles, as NumPy counts them)
    if (n < 8) {
        double r = -0.0;
        for (std::size_t i = begin; i < end; ++i) r += v[i];
        return r;
    }
    if (n <= 128) {
        double r[4] = {v[begin], v[begin + 1], v[begin + 2], v[begin + 3]};
        std::size_t i = 8;
        for (; i < n - (n % 8); i += 8)
            for (std::size_t k = 0; k < 4; ++k) r[k] += v[begin + i / 2 + k];
        double rr = ((r[0] + r[1]) + (r[2] + r[3]));
        for (; i < n; i += 2) rr += v[begin + i / 2];
        return rr;
    }
    std::size_t n2 = n / 2;
    n2 -= n2 % 8;
    return complexSum(v, begin, begin + n2 / 2) + complexSum(v, begin + n2 / 2, end);
}

// window.sum(): of a real window NumPy's float64 pairwise sum; of a complex one (a complex signal's) the complex sum
double windowSum(const std::vector<double>& w, bool complex) {
    return complex ? 0.0 + complexSum(w, 0, w.size()) : support::npSum(w);
}

}  // namespace

bool spectralHelper(const Signal& xs, const Signal* ys, const SpectralOptions& o, Spectrum* out, std::string* error) {
    const bool same = ys == nullptr;
    const double Fs = o.Fs ? *o.Fs : 2;
    const long long noverlap = o.noverlap ? *o.noverlap : 0;
    const long long NFFT = o.NFFT ? *o.NFFT : 256;
    if (!(0 <= noverlap && noverlap < NFFT)) return *error = "noverlap must be less than NFFT", false;
    std::string mode = o.mode == "default" ? "psd" : o.mode;
    if (mode != "psd" && mode != "complex" && mode != "magnitude" && mode != "angle" && mode != "phase")
        return *error = "'" + o.mode + "' is not a valid value for mode; supported values are 'default', 'psd', 'complex', 'magnitude', 'angle', 'phase'", false;
    if (!same && mode != "psd") return *error = "x and y must be equal if mode is not 'psd'", false;
    std::string sides = o.sides;
    if (sides == "default") sides = xs.complex ? "twosided" : "onesided";
    if (sides != "onesided" && sides != "twosided")
        return *error = "'" + o.sides + "' is not a valid value for sides; supported values are 'default', 'onesided', 'twosided'", false;
    std::vector<Complex> x = xs.values, y = ys ? ys->values : std::vector<Complex>{};
    if ((long long)x.size() < NFFT) x.resize(std::size_t(NFFT), Complex(0.0, 0.0));
    if (!same && (long long)y.size() < NFFT) y.resize(std::size_t(NFFT), Complex(0.0, 0.0));
    const long long padTo = o.padTo ? *o.padTo : NFFT;
    if (padTo < 1) return *error = "Invalid number of FFT data points (" + std::to_string(padTo) + ") specified.", false;
    bool scaleByFreq = mode == "psd" ? (o.scaleByFreq ? *o.scaleByFreq : true) : false;
    long long numFreqs = 0, freqcenter = 0;
    double scaling = 1.0;
    if (sides == "twosided") {
        numFreqs = padTo;
        freqcenter = padTo % 2 ? (padTo - 1) / 2 + 1 : padTo / 2;
    } else {
        numFreqs = padTo % 2 ? (padTo + 1) / 2 : padTo / 2 + 1;
        scaling = 2.0;
    }
    const std::vector<double> window = o.window ? *o.window : hanning(NFFT);  // (window_hanning of ones)
    if ((long long)window.size() != NFFT) return *error = "The window length must match the data's first dimension", false;
    // the segments: NFFT values every NFFT - noverlap (sliding_window_view), windowed, transformed and cut
    const long long step = NFFT - noverlap;
    const auto spectra = [&](const std::vector<Complex>& sig) {
        std::vector<std::vector<Complex>> cols;
        for (long long s0 = 0; s0 + NFFT <= (long long)sig.size(); s0 += step) {
            std::vector<Complex> seg;
            for (long long k = 0; k < NFFT; ++k) {
                const Complex v = sig[std::size_t(s0 + k)];
                seg.emplace_back(v.real() * window[std::size_t(k)], v.imag() * window[std::size_t(k)]);
            }
            auto f = support::npFft(seg, std::size_t(padTo));
            f.resize(std::size_t(numFreqs));
            cols.push_back(std::move(f));
        }
        return cols;
    };
    std::vector<std::vector<Complex>> cols = spectra(x);
    if (!same) {
        const auto ycols = spectra(y);
        for (std::size_t c = 0; c < cols.size(); ++c)
            for (std::size_t i = 0; i < cols[c].size(); ++i) cols[c][i] = mul(std::conj(cols[c][i]), ycols[c][i]);
    } else if (mode == "psd") {
        for (auto& col : cols)
            for (auto& v : col) v = mul(std::conj(v), v);
    } else if (mode == "magnitude") {
        // np.abs(result) / window.sum(): by a complex sum (a complex signal's), a complex quotient
        const double sum = windowSum(window, xs.complex);
        for (auto& col : cols)
            for (auto& v : col) {
                const double a = support::npyAbsolute({v.real(), v.imag()});
                v = xs.complex ? divReal(Complex(a, 0.0), sum) : Complex(a / sum, 0.0);
            }
    } else if (mode == "angle" || mode == "phase") {
        for (auto& col : cols)
            for (auto& v : col) v = Complex(std::atan2(v.imag(), v.real()), 0.0);
    } else if (mode == "complex") {
        const double sum = windowSum(window, xs.complex);
        for (auto& col : cols)
            for (auto& v : col) v = divReal(v, sum);
    }
    if (mode == "psd") {
        // the one-sided spectrum's inner frequencies doubled; then by Fs and the window's energy, or its sum squared
        const long long last = NFFT % 2 ? numFreqs : numFreqs - 1;
        std::vector<double> squares;
        for (double w : window) squares.push_back(w * w);
        const double energy = windowSum(squares, xs.complex), sum = windowSum(window, xs.complex);
        for (auto& col : cols) {
            for (long long i = 1; i < last; ++i) col[std::size_t(i)] = mul(col[std::size_t(i)], Complex(scaling, 0.0));
            for (auto& v : col) {
                if (scaleByFreq) {
                    v = divReal(v, Fs);
                    v = divReal(v, energy);
                } else {
                    v = divReal(v, sum * sum);
                }
            }
        }
    }
    // the times of the segments' middles; np.fft.fftfreq(pad_to, 1 / Fs)
    out->t.clear();
    for (double v : arangeFloat(double(NFFT) / 2, double(x.size()) - double(NFFT) / 2 + 1, double(step))) out->t.push_back(v / Fs);
    const double val = 1.0 / (double(padTo) * (1 / Fs));
    std::vector<double> freqs;
    const long long N = (padTo - 1) / 2 + 1;
    for (long long i = 0; i < padTo; ++i) freqs.push_back(double(i < N ? i : i - padTo) * val);
    freqs.resize(std::size_t(numFreqs));
    // result[frequency][segment]
    std::vector<std::vector<Complex>> result(std::size_t(numFreqs), std::vector<Complex>(cols.size()));
    for (std::size_t c = 0; c < cols.size(); ++c)
        for (std::size_t i = 0; i < std::size_t(numFreqs); ++i) result[i][c] = cols[c][i];
    if (sides == "twosided") {
        std::rotate(freqs.begin(), freqs.begin() + freqcenter, freqs.end());
        std::rotate(result.begin(), result.begin() + freqcenter, result.end());
    } else if (!(padTo % 2)) {
        freqs.back() *= -1;
    }
    if (mode == "phase") {
        // np.unwrap(result, axis=0): the jumps beyond pi brought back by whole turns, summed down each segment
        for (std::size_t c = 0; c < cols.size(); ++c) {
            std::vector<double> p;  // (np.diff of the angles as they were)
            for (const auto& row : result) p.push_back(row[c].real());
            double acc = 0;
            for (std::size_t i = 1; i < result.size(); ++i) {
                const double dd = p[i] - p[i - 1];
                double ddmod = support::divmod(dd - (-M_PI), 2 * M_PI).second + (-M_PI);
                if (ddmod == -M_PI && dd > 0) ddmod = M_PI;
                double correct = ddmod - dd;
                if (std::fabs(dd) < M_PI) correct = 0;
                acc += correct;  // (cumsum)
                result[i][c] = Complex(p[i] + acc, 0.0);
            }
        }
    }
    out->result = std::move(result);
    out->freqs = std::move(freqs);
    return true;
}

bool csd(const Signal& x, const Signal* y, SpectralOptions o, std::vector<Complex>* pxy, std::vector<double>* freqs, std::string* error) {
    if (!o.NFFT) o.NFFT = 256;
    o.mode = "psd";
    Spectrum s;
    if (!spectralHelper(x, y, o, &s, error)) return false;
    pxy->clear();
    for (const auto& row : s.result) {
        if (row.size() == 1) {
            pxy->push_back(row[0]);
            continue;
        }
        Complex sum(0.0, 0.0);  // Pxy.mean(axis=1): the segments in order, then times 1 / n
        for (const Complex& v : row) sum = Complex(sum.real() + v.real(), sum.imag() + v.imag());
        pxy->push_back(divReal(sum, double(row.size())));
    }
    *freqs = s.freqs;
    return true;
}

bool specgram(const Signal& x, SpectralOptions o, Spectrum* out, std::string* error) {
    if (!o.noverlap) o.noverlap = 128;
    if (!o.NFFT) o.NFFT = 256;
    if (!spectralHelper(x, nullptr, o, out, error)) return false;
    if (o.mode != "complex")
        for (auto& row : out->result)
            for (auto& v : row) v = Complex(v.real(), 0.0);
    return true;
}

bool cohere(const Signal& x, const Signal& y, SpectralOptions o, std::vector<double>* cxy, std::vector<double>* freqs,
            std::string* error) {
    const long long NFFT = o.NFFT ? *o.NFFT : 256;
    o.NFFT = NFFT;
    if ((long long)x.values.size() < 2 * NFFT)
        return *error = "Coherence is calculated by averaging over *NFFT* length segments.  Your signal is too short for your choice of *NFFT*.", false;
    std::vector<Complex> pxx, pyy, pxy;
    std::vector<double> f;
    if (!csd(x, nullptr, o, &pxx, &f, error) || !csd(y, nullptr, o, &pyy, &f, error) || !csd(x, &y, o, &pxy, &f, error)) return false;
    cxy->clear();
    for (std::size_t i = 0; i < pxy.size(); ++i) {
        const double a = support::npyAbsolute({pxy[i].real(), pxy[i].imag()});
        cxy->push_back(a * a / (pxx[i].real() * pyy[i].real()));
    }
    *freqs = f;
    return true;
}

bool singleSpectrum(const std::string& mode, const Signal& x, SpectralOptions o, std::vector<double>* spec, std::vector<double>* freqs,
                    std::string* error) {
    if (!o.padTo) o.padTo = (long long)x.values.size();
    o.NFFT = (long long)x.values.size();
    o.noverlap = 0;
    o.scaleByFreq = false;
    o.mode = mode;
    Spectrum s;
    if (!spectralHelper(x, nullptr, o, &s, error)) return false;
    spec->clear();
    for (const auto& row : s.result) spec->push_back(row[0].real());
    *freqs = s.freqs;
    return true;
}

}  // namespace calculate_plot::engine::mlab
