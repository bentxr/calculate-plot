// cbook.violin_stats (mlab.GaussianKDE, np.mean, np.median, np.percentile) against matplotlib 3.11.2
// (tests/oracle/violin.json, written by tools/oracle/make_violin_oracle.py). The densities go through exp:
// another libm (WASM's musl) may differ in the last bit, so there they are compared within 1e-12.
#include "engine/violin.hpp"

#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using calculate_plot::support::Json;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(std::filesystem::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/violin.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *calculate_plot::support::parseJson(text.str(), nullptr);
    }();
    return o;
}

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
            out.push_back(r * scale + offset);
        }
        return out;
    }
    for (const Json& v : spec.items()) out.push_back(v.isString() ? std::strtod(v.asString().c_str(), nullptr) : v.asNumber());
    return out;
}

void expectHexes(const std::vector<double>& got, const Json& want, const std::string& name) {
    ASSERT_EQ(got.size(), want.size()) << name;
    for (std::size_t i = 0; i < got.size(); ++i) {
        const double w = std::strtod(want[i].asString().c_str(), nullptr);
        if (std::isnan(w)) EXPECT_TRUE(std::isnan(got[i])) << name << " [" << i << "]";
        // (exact against glibc's libm, which made the references; within 1e-12 on another, as for traces)
        else EXPECT_TRUE(calculate_plot::testing_support::sameNumber(got[i], w))
            << name << " [" << i << "] " << std::hexfloat << got[i] << " instead of " << w;
    }
}

}  // namespace

TEST(Violin, StatsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("cases")->items()) {
        const std::vector<double> x = data(*c.find("data"));
        calculate_plot::engine::Bandwidth bw;
        const Json& b = *c.find("bw");
        if (b.isString()) bw.rule = b.asString();
        else if (b.isNumber()) bw.factor = b.asNumber();
        std::vector<double> quantiles;
        if (const Json* q = c.find("q"); q && !q->isNull())
            for (const Json& v : q->items()) quantiles.push_back(v.asNumber());
        calculate_plot::engine::ViolinStats s;
        std::string error;
        const std::string name = std::to_string(checked) + " (" + std::to_string(x.size()) + " values, bw " +
                                 (b.isNull() ? "None" : b.isString() ? b.asString() : std::to_string(b.asNumber())) + ", " +
                                 std::to_string(static_cast<long long>(c.find("points")->asNumber())) + " points)";
        ASSERT_TRUE(calculate_plot::engine::violinStats(x, c.find("int")->asBool(), bw,
                                                        static_cast<long long>(c.find("points")->asNumber()), quantiles, &s, &error))
            << name << ": " << error;
        expectHexes(s.coords, *c.find("coords"), name + " coords");
        expectHexes(s.vals, *c.find("vals"), name + " vals");
        expectHexes({s.mean}, *c.find("mean"), name + " mean");
        expectHexes({s.median}, *c.find("median"), name + " median");
        expectHexes({s.min}, *c.find("min"), name + " min");
        expectHexes({s.max}, *c.find("max"), name + " max");
        expectHexes(s.quantiles, *c.find("quantiles"), name + " quantiles");
        ++checked;
    }
    EXPECT_EQ(checked, 480);
}
