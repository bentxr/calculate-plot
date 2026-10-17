// support::log10 gives the last bit glibc's log10 gives (tests/oracle/log10.json, recorded from the
// reference environment), on every target: log scales then place their ticks and curves where
// matplotlib does also where the C library is another one (in the browser).
#include "support/json.hpp"
#include "support/log10.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;

namespace {

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

TEST(Log10, EqualsTheReferenceEnvironments) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/log10.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    const Json& values = *oracle->find("values");
    ASSERT_GT(values.size(), 8000u);
    int different = 0;
    std::string first;
    for (const Json& v : values.items()) {
        const double x = std::strtod(v[0].asString().c_str(), nullptr);
        const double want = std::strtod(v[1].asString().c_str(), nullptr);
        if (!sameBits(calculate_plot::support::log10(x), want)) {
            if (first.empty()) first = v[0].asString();
            ++different;
        }
    }
    // The formula is glibc's; it stands on the C library's natural logarithm, which is the same
    // code in glibc and musl but for the use of fused multiply-add: a few results per million
    // differ in the last bit where the logarithm does.
    if (calculate_plot::testing_support::kReferenceLibm) EXPECT_EQ(different, 0) << "first: log10(" << first << ")";
    else EXPECT_LE(different, 2) << "first: log10(" << first << ")";
}

// Where the C library is glibc, it is the library's own log10 to the last bit.
TEST(Log10, IsTheLibrarysOnTheReferenceLibrary) {
    if (!calculate_plot::testing_support::kReferenceLibm) GTEST_SKIP() << "another C library";
    std::uint64_t s = 88172645463325252ull;
    const auto next = [&] {
        s ^= s << 13, s ^= s >> 7, s ^= s << 17;
        return double(s >> 11) / 9007199254740992.0;
    };
    int different = 0;
    for (int i = 0; i < 2000000; ++i) {
        volatile double x = i % 3 == 0 ? next() * 1000 : std::pow(10.0, next() * 600 - 300) * (1 + next());
        if (!sameBits(calculate_plot::support::log10(x), std::log10(x))) ++different;
    }
    EXPECT_EQ(different, 0);
}

TEST(Log10, SpecialValues) {
    using calculate_plot::support::log10;
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(log10(0.0), -inf);
    EXPECT_EQ(log10(-0.0), -inf);
    EXPECT_EQ(log10(inf), inf);
    EXPECT_TRUE(std::isnan(log10(-1.0)));
    EXPECT_TRUE(std::isnan(log10(-inf)));
    EXPECT_TRUE(std::isnan(log10(std::numeric_limits<double>::quiet_NaN())));
    EXPECT_EQ(log10(1.0), 0.0);
    EXPECT_FALSE(std::signbit(log10(1.0)));
    for (int n = 0; n <= 22; ++n) EXPECT_EQ(log10(std::pow(10.0, n)), double(n)) << n;  // exact for 10**0..22
    EXPECT_EQ(log10(5e-324), std::strtod("-0x1.434e6420f4374p+8", nullptr));  // the smallest subnormal
}

}  // namespace
