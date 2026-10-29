// mlab's spectra equal matplotlib 3.11.2's (tests/oracle/mlab.json): psd, csd, specgram in every mode, cohere and the
// single spectra of real and complex signals, with windows, overlaps, padding, sides and scaling. Exact where the C
// library is glibc, as the oracle's.
#include "engine/spectral.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace mlab = calculate_plot::engine::mlab;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

mlab::Signal signalOf(const Json& j) {
    mlab::Signal s;
    for (const Json& v : j.items()) {
        if (v.isArray()) {
            s.complex = true;
            s.values.emplace_back(hex(v[0]), hex(v[1]));
        } else {
            s.values.emplace_back(hex(v), 0.0);
        }
    }
    return s;
}

void expectValues(const Json& want, const std::vector<mlab::Complex>& got, bool complex, const std::string& where) {
    ASSERT_EQ(want.size(), got.size()) << where;
    for (std::size_t i = 0; i < got.size(); ++i) {
        const double re = complex ? hex(want[i][0]) : hex(want[i]);
        const double im = complex ? hex(want[i][1]) : 0.0;
        // (off glibc, the last bits of a near-zero imaginary part can take the other sign: an angle of pi then reads -pi)
        const auto same = [](double a, double b) {
            if (!calculate_plot::testing_support::kReferenceLibm && std::fabs(std::fabs(a) - M_PI) < 1e-12 &&
                std::fabs(std::fabs(b) - M_PI) < 1e-12)
                return true;
            return (std::isnan(a) && std::isnan(b)) || calculate_plot::testing_support::sameNumber(a, b);
        };
        EXPECT_TRUE(same(re, got[i].real())) << where << " #" << i << ": " << re << " vs " << got[i].real();
        if (complex) {
            EXPECT_TRUE(same(im, got[i].imag())) << where << " #" << i << " imag";
        }
    }
}

std::vector<mlab::Complex> reals(const std::vector<double>& v) {
    std::vector<mlab::Complex> out;
    for (double x : v) out.emplace_back(x, 0.0);
    return out;
}

}  // namespace

TEST(Mlab, SpectraAreMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/mlab.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    ASSERT_EQ(oracle->find("cases")->size(), 21u);
    int index = 0;
    for (const Json& c : oracle->find("cases")->items()) {
        const std::string call = c.find("call")->asString();
        const std::string where = std::to_string(index++) + " " + call;
        const mlab::Signal x = signalOf(*c.find("x"));
        std::optional<mlab::Signal> y;
        if (const Json* yj = c.find("y")) y = signalOf(*yj);
        mlab::SpectralOptions o;
        for (const auto& [k, v] : c.find("kwargs")->members()) {
            if (k == "NFFT") o.NFFT = (long long)v.asNumber();
            else if (k == "Fs") o.Fs = v.asNumber();
            else if (k == "noverlap") o.noverlap = (long long)v.asNumber();
            else if (k == "pad_to") o.padTo = (long long)v.asNumber();
            else if (k == "sides") o.sides = v.asString();
            else if (k == "scale_by_freq") o.scaleByFreq = v.asBool();
            else if (k == "mode") o.mode = v.asString();
            else if (k == "window" && v.asString() == "none") o.window = std::vector<double>(std::size_t(o.NFFT ? *o.NFFT : 256), 1.0);
        }
        if (const Json* w = c.find("window")) {
            std::vector<double> win;
            for (const Json& v : w->items()) win.push_back(hex(v));
            o.window = win;
        }
        const auto& results = c.find("results")->items();
        std::string error;
        std::vector<mlab::Complex> cv;
        std::vector<double> rv, freqs;
        if (call == "psd" || call == "csd") {
            ASSERT_TRUE(mlab::csd(x, y ? &*y : nullptr, o, &cv, &freqs, &error)) << where << ": " << error;
            if (call == "psd")
                for (auto& v : cv) v = mlab::Complex(v.real(), 0.0);
            expectValues(results[0], cv, call == "csd", where + " P");
            expectValues(results[1], reals(freqs), false, where + " freqs");
        } else if (call == "cohere") {
            ASSERT_TRUE(mlab::cohere(x, *y, o, &rv, &freqs, &error)) << where << ": " << error;
            expectValues(results[0], reals(rv), false, where + " Cxy");
            expectValues(results[1], reals(freqs), false, where + " freqs");
        } else if (call == "specgram") {
            mlab::Spectrum s;
            ASSERT_TRUE(mlab::specgram(x, o, &s, &error)) << where << ": " << error;
            std::vector<mlab::Complex> flat;
            for (const auto& row : s.result) flat.insert(flat.end(), row.begin(), row.end());
            expectValues(results[0], flat, o.mode == "complex", where + " spec");
            expectValues(results[1], reals(s.freqs), false, where + " freqs");
            expectValues(results[2], reals(s.t), false, where + " t");
        } else {
            const std::string mode = call == "magnitude_spectrum" ? "magnitude" : call == "angle_spectrum" ? "angle" : "phase";
            ASSERT_TRUE(mlab::singleSpectrum(mode, x, o, &rv, &freqs, &error)) << where << ": " << error;
            expectValues(results[0], reals(rv), false, where + " spectrum");
            expectValues(results[1], reals(freqs), false, where + " freqs");
        }
    }
}
