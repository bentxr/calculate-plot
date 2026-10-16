// np.sum, np.dot, np.mean, np.std, np.percentile, np.histogram_bin_edges and np.histogram (bins by count,
// edges or name) against NumPy 2.5.3 (tests/oracle/histogram.json, written by
// tools/oracle/make_histogram_oracle.py).
#include "support/blas_dot.hpp"
#include "support/histogram.hpp"

#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace support = calculate_plot::support;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/histogram.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double number(const Json& v) {
    if (v.isString()) return std::strtod(v.asString().c_str(), nullptr);  // "nan", "inf", "-inf"
    return v.asNumber();
}

// A list of numbers, or the generator (i * p) % q / q * scale + offset the oracle describes, or for
// integers (i * p) % q + offset
std::vector<double> data(const Json& spec) {
    std::vector<double> out;
    if (spec.isObject()) {
        const auto n = static_cast<long long>(spec.find("n")->asNumber());
        const auto p = static_cast<long long>(spec.find("p")->asNumber());
        const auto q = static_cast<long long>(spec.find("q")->asNumber());
        if (spec.find("int")) {
            const auto offset = static_cast<long long>(spec.find("offset")->asNumber());
            for (long long i = 0; i < n; ++i) out.push_back(double((i * p) % q + offset));
            return out;
        }
        const double scale = spec.find("scale")->asNumber(), offset = spec.find("offset")->asNumber();
        for (long long i = 0; i < n; ++i) {
            const double r = double((i * p) % q) / double(q);
            const double s = r * scale;
            out.push_back(s + offset);
        }
        return out;
    }
    for (const Json& v : spec.items()) out.push_back(number(v));
    return out;
}

std::vector<double> hexData(const Json& spec) {
    std::vector<double> out;
    for (const Json& v : spec.items()) out.push_back(std::strtod(v.asString().c_str(), nullptr));
    return out;
}

double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }

void expectHexes(const std::vector<double>& got, const Json& expected, const std::string& name) {
    ASSERT_EQ(got.size(), expected.size()) << name;
    for (std::size_t i = 0; i < got.size(); ++i) {
        const double e = std::strtod(expected[i].asString().c_str(), nullptr);
        if (std::isnan(e)) EXPECT_TRUE(std::isnan(got[i])) << name << " [" << i << "]";
        else EXPECT_EQ(got[i], e) << name << " [" << i << "]";
    }
}

support::Bins bins(const Json& b) {
    support::Bins out;
    if (b.isNumber()) out.count = static_cast<long long>(b.asNumber());
    else out.edges = data(b);
    return out;
}

}  // namespace

TEST(Histogram, SumIsNumpysPairwiseSum) {
    int checked = 0;
    for (const Json& c : oracle().find("sums")->items()) {
        const std::vector<double> a = data(*c.find("data"));
        const double expected = std::strtod(c.find("sum")->asString().c_str(), nullptr);
        EXPECT_EQ(support::npSum(a), expected) << a.size() << " values";
        ++checked;
    }
    EXPECT_EQ(checked, 81);
}

TEST(Histogram, HistogramAndBinEdgesAreNumpys) {
    int checked = 0, refused = 0;
    for (const Json& c : oracle().find("histograms")->items()) {
        const std::vector<double> a = data(*c.find("data"));
        const support::Bins b = bins(*c.find("bins"));
        std::optional<std::pair<double, double>> range;
        if (!c.find("range")->isNull()) range = std::make_pair(number((*c.find("range"))[0]), number((*c.find("range"))[1]));
        std::optional<std::vector<double>> weights;
        if (!c.find("weights")->isNull()) weights = data(*c.find("weights"));
        const bool density = c.find("density")->asBool();
        const std::string name = std::to_string(checked + refused) + " (" + std::to_string(a.size()) + " values)";
        std::vector<double> counts, edges;
        std::string error;
        const bool ok = support::histogram(a, b, range, density, weights ? &*weights : nullptr, &counts, &edges, &error);
        if (c.find("error")) {
            EXPECT_FALSE(ok) << name;
            EXPECT_EQ(error, c.find("error")->asString()) << name;
            ++refused;
        } else {
            ASSERT_TRUE(ok) << name << ": " << error;
            expectHexes(counts, *c.find("counts"), name + " counts");
            expectHexes(edges, *c.find("edges"), name + " edges");
            ++checked;
        }
        std::vector<double> binEdges;
        error.clear();
        const bool edgesOk = support::histogramBinEdges(a, b, range, weights ? &*weights : nullptr, &binEdges, &error);
        if (c.find("bin_edges_error")) {
            EXPECT_FALSE(edgesOk) << name;
            EXPECT_EQ(error, c.find("bin_edges_error")->asString()) << name;
        } else {
            ASSERT_TRUE(edgesOk) << name << ": " << error;
            expectHexes(binEdges, *c.find("bin_edges"), name + " bin edges");
        }
    }
    EXPECT_EQ(checked, 30);
    EXPECT_EQ(refused, 8);
}

// x.dot(y) of float64 vectors: OpenBLAS's ddot (the reference machine's Haswell kernel)
TEST(Histogram, DotIsTheReferenceBlasDot) {
    int checked = 0;
    for (const Json& c : oracle().find("dots")->items()) {
        const std::vector<double> a = c.find("hex") ? hexData(*c.find("a")) : data(*c.find("a"));
        const std::vector<double> b = c.find("b")->isNull() ? a : data(*c.find("b"));
        EXPECT_EQ(support::blasDot(a, b), hex(*c.find("dot"))) << checked << " (" << a.size() << " values)";
        ++checked;
    }
    EXPECT_EQ(checked, 304);
}

// np.mean, np.std and np.percentile(x, [75, 25]) of float64 and of int64 data
TEST(Histogram, MeanStdAndPercentilesAreNumpys) {
    int checked = 0;
    for (const Json& c : oracle().find("stats")->items()) {
        const std::vector<double> a = data(*c.find("data"));
        const bool integer = c.find("int")->asBool();
        const std::string name = std::to_string(checked) + " (" + std::to_string(a.size()) + (integer ? " integers)" : " values)");
        const auto same = [](double got, double want) { return got == want || (std::isnan(got) && std::isnan(want)); };
        EXPECT_TRUE(same(support::npMean(a, integer), hex(*c.find("mean")))) << name;
        EXPECT_TRUE(same(support::npStd(a, integer), hex(*c.find("std")))) << name;
        EXPECT_TRUE(same(support::npPercentile(a, 75), hex(*c.find("p75")))) << name;
        EXPECT_TRUE(same(support::npPercentile(a, 25), hex(*c.find("p25")))) << name;
        ++checked;
    }
    EXPECT_EQ(checked, 79);
}

// bins='auto', 'fd', 'doane', 'scott', 'stone', 'rice', 'sturges', 'sqrt'
TEST(Histogram, BinsByNameAreNumpys) {
    int checked = 0, refused = 0;
    for (const Json& c : oracle().find("named")->items()) {
        const std::vector<double> a = data(*c.find("data"));
        const bool integer = c.find("int")->asBool();
        support::Bins b;
        b.name = c.find("bins")->asString();
        std::optional<std::pair<double, double>> range;
        if (!c.find("range")->isNull()) range = std::make_pair(number((*c.find("range"))[0]), number((*c.find("range"))[1]));
        std::optional<std::vector<double>> weights;
        if (const Json* w = c.find("weights"); w && !w->isNull()) weights = data(*w);
        const std::string name = std::to_string(checked + refused) + " " + b.name + " (" + std::to_string(a.size()) +
                                 (integer ? " integers)" : " values)");
        std::vector<double> edges, counts, histEdges;
        std::string error;
        const bool ok = support::histogramBinEdges(a, b, range, weights ? &*weights : nullptr, &edges, &error, integer);
        if (c.find("error")) {
            EXPECT_FALSE(ok) << name;
            EXPECT_EQ(error, c.find("error")->asString()) << name;
            ++refused;
            continue;
        }
        ASSERT_TRUE(ok) << name << ": " << error;
        expectHexes(edges, *c.find("edges"), name + " edges");
        ++checked;
        if (!c.find("counts")) continue;  // (the edges alone)
        error.clear();
        ASSERT_TRUE(support::histogram(a, b, range, false, nullptr, &counts, &histEdges, &error, integer)) << name << ": " << error;
        expectHexes(counts, *c.find("counts"), name + " counts");
        expectHexes(histEdges, *c.find("edges"), name + " histogram edges");
    }
    EXPECT_EQ(checked, 202);
    EXPECT_EQ(refused, 3);
}

// np.interp(x, xp, fp): binary_search_with_guess, the slopes, NaN tried from the other end
TEST(Histogram, InterpIsNumpys) {
    int checked = 0;
    for (const Json& c : oracle().find("interps")->items()) {
        const std::vector<double> x = data(*c.find("x")), xp = data(*c.find("xp")), fp = data(*c.find("fp"));
        const std::vector<double> want = hexData(*c.find("interp"));
        const std::vector<double> got = support::npInterp(x, xp, fp);
        ASSERT_EQ(got.size(), want.size()) << checked;
        for (std::size_t i = 0; i < want.size(); ++i)
            EXPECT_TRUE((got[i] == want[i] && std::signbit(got[i]) == std::signbit(want[i])) || (std::isnan(got[i]) && std::isnan(want[i])))
                << "case " << checked << " at " << i << ": " << got[i] << " instead of " << want[i];
        ++checked;
    }
    EXPECT_EQ(checked, 24);
}
