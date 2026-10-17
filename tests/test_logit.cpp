// The logit scale's transform, locator and labels against matplotlib 3.11.2 (tests/oracle/logit.json,
// written by tools/oracle/make_logit_oracle.py).
#include "axes/log_ticker.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"
#include "transforms/transforms.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace support = calculate_plot::support;
namespace tr = calculate_plot::transforms;
namespace ax = calculate_plot::axes;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/logit.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }
bool same(double got, double want) {
    return (std::isnan(got) && std::isnan(want)) || calculate_plot::testing_support::sameNumber(got, want);
}

std::vector<double> hexes(const Json& list) {
    std::vector<double> out;
    for (const Json& v : list.items()) out.push_back(hex(v));
    return out;
}

}  // namespace

TEST(Logit, TransformAndInverseAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("transforms")->items()) {
        const tr::LogitTransform t(c.find("nonpositive")->asString() == "clip");
        const auto values = hexes(*c.find("values")), forward = hexes(*c.find("forward"));
        for (std::size_t i = 0; i < values.size(); ++i)
            EXPECT_TRUE(same(t.transform1d(values[i]), forward[i])) << "case " << checked << " value " << i << ": " << t.transform1d(values[i]);
        const auto inputs = hexes(*c.find("inputs")), inverse = hexes(*c.find("inverse"));
        for (std::size_t i = 0; i < inputs.size(); ++i)
            EXPECT_TRUE(same(tr::LogitTransform::inverse1d(inputs[i]), inverse[i])) << "case " << checked << " inverse " << i;
        ++checked;
    }
    EXPECT_EQ(checked, 2);
}

TEST(Logit, LocatorIsMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("locators")->items()) {
        ax::LogitLocator loc;
        loc.minor = c.find("minor")->asBool();
        loc.tickSpace = int(c.find("space")->asNumber());
        loc.minpos = hex(*c.find("minpos"));
        const double vmin = hex((*c.find("range"))[0]), vmax = hex((*c.find("range"))[1]);
        const auto ns = loc.nonsingular(vmin, vmax);
        const auto wantNs = hexes(*c.find("nonsingular"));
        EXPECT_TRUE(same(ns.first, wantNs[0]) && same(ns.second, wantNs[1])) << "case " << checked;
        const auto ticks = loc.tickValues(vmin, vmax);
        const auto want = hexes(*c.find("ticks"));
        ASSERT_EQ(ticks.size(), want.size()) << "case " << checked;
        for (std::size_t i = 0; i < want.size(); ++i) EXPECT_TRUE(same(ticks[i], want[i])) << "case " << checked << " tick " << i;
        for (const bool round : {false, true}) {
            const Json* w = c.find(round ? "round_numbers" : "data");
            if (!w) continue;
            const auto v = loc.viewLimits(ns.first, ns.second, round);
            EXPECT_TRUE(same(v.first, hex((*w)[0])) && same(v.second, hex((*w)[1]))) << "case " << checked << (round ? " round" : " data");
        }
        ++checked;
    }
    EXPECT_EQ(checked, 408);
}

TEST(Logit, LabelsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("labels")->items()) {
        ax::LogitFormatter f;
        f.minor = c.find("minor")->asBool();
        f.useOverline = c.find("overline")->asBool();
        const auto locs = hexes(*c.find("locs"));
        f.setLocs(locs);
        for (std::size_t i = 0; i < locs.size(); ++i)
            EXPECT_EQ(f(locs[i]), (*c.find("labels"))[i].asString()) << "case " << checked << " x " << i;
        ++checked;
    }
    EXPECT_EQ(checked, 104);
}
