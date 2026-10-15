// np.sum, np.histogram_bin_edges and np.histogram against NumPy 2.5.3 (tests/oracle/histogram.json,
// written by tools/oracle/make_histogram_oracle.py).
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

// A list of numbers, or the generator (i * p) % q / q * scale + offset the oracle describes
std::vector<double> data(const Json& spec) {
    std::vector<double> out;
    if (spec.isObject()) {
        const auto n = static_cast<long long>(spec.find("n")->asNumber());
        const auto p = static_cast<long long>(spec.find("p")->asNumber());
        const auto q = static_cast<long long>(spec.find("q")->asNumber());
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
