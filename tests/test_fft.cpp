// np.fft.fft and np.fft.rfft through the vendored pocketfft equal NumPy 2.5.3's (tests/oracle/fft.json): every radix,
// Bluestein's lengths, padding, truncation, and batches along an axis (NumPy's vector lanes). Exact where the C
// library is glibc, as the oracle's.
#include "support/json.hpp"
#include "support/npy_fft.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

std::vector<double> reals(const Json& j) {
    std::vector<double> out;
    for (const Json& v : j.items()) out.push_back(hex(v));
    return out;
}

void expectComplex(const Json& want, const std::vector<std::complex<double>>& got, const std::string& where) {
    ASSERT_EQ(want.size(), got.size()) << where;
    for (std::size_t i = 0; i < got.size(); ++i) {
        EXPECT_TRUE(calculate_plot::testing_support::sameNumber(hex(want[i][0]), got[i].real())) << where << " re #" << i;
        EXPECT_TRUE(calculate_plot::testing_support::sameNumber(hex(want[i][1]), got[i].imag())) << where << " im #" << i;
    }
}

}  // namespace

TEST(Fft, PocketfftIsNumpys) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/fft.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    namespace s = calculate_plot::support;
    ASSERT_EQ(oracle->find("cases")->size(), 38u);
    for (const Json& c : oracle->find("cases")->items()) {
        const std::size_t n = std::size_t(c.find("n")->asNumber());
        const auto re = reals(*c.find("re")), im = reals(*c.find("im"));
        std::vector<std::complex<double>> z, r;
        for (std::size_t i = 0; i < n; ++i) z.emplace_back(re[i], im[i]), r.emplace_back(re[i], 0.0);
        const std::string where = "n=" + std::to_string(n);
        expectComplex(*c.find("fft"), s::npFft(z, n), where + " fft");
        expectComplex(*c.find("fft_real"), s::npFft(r, n), where + " fft of real");
        expectComplex(*c.find("rfft"), s::npRfft(re, n), where + " rfft");
    }
    for (const Json& c : oracle->find("padded")->items()) {
        const auto x = reals(*c.find("x"));
        const std::size_t n = std::size_t(c.find("n")->asNumber());
        std::vector<std::complex<double>> z;
        for (double v : x) z.emplace_back(v, 0.0);
        expectComplex(*c.find("fft"), s::npFft(z, n), "padded fft n=" + std::to_string(n));
        expectComplex(*c.find("rfft"), s::npRfft(x, n), "padded rfft n=" + std::to_string(n));
    }
    for (const Json& c : oracle->find("batches")->items()) {
        const std::size_t rows = std::size_t(c.find("rows")->asNumber()), cols = std::size_t(c.find("cols")->asNumber());
        const auto a = reals(*c.find("a"));
        for (int padded = 0; padded < 2; ++padded) {
            const std::size_t n = padded ? 2 * rows : rows;
            std::vector<std::complex<double>> all(n * cols);
            for (std::size_t col = 0; col < cols; ++col) {
                std::vector<std::complex<double>> column;
                for (std::size_t row = 0; row < rows; ++row) column.emplace_back(a[row * cols + col], 0.0);
                const auto f = s::npFft(column, n);
                for (std::size_t row = 0; row < n; ++row) all[row * cols + col] = f[row];
            }
            expectComplex(*c.find(padded ? "fft_n" : "fft"), all, "batch " + std::to_string(rows) + "x" + std::to_string(cols));
        }
    }
}
