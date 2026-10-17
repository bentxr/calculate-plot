// The symlog scale's transform, locator and labels against matplotlib 3.11.2 (tests/oracle/symlog.json,
// written by tools/oracle/make_symlog_oracle.py).
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
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/symlog.json", std::ios::binary);
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

}  // namespace

TEST(Symlog, TransformAndInverseAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("transforms")->items()) {
        const Json& p = *c.find("params");
        const tr::SymmetricalLogTransform t(hex(p[0]), hex(p[1]), hex(p[2]));
        for (std::size_t i = 0; i < c.find("values")->size(); ++i) {
            const double got = t.transform1d(hex((*c.find("values"))[i])), want = hex((*c.find("forward"))[i]);
            EXPECT_TRUE(same(got, want)) << "case " << checked << " value " << i << ": " << got << " instead of " << want;
        }
        for (std::size_t i = 0; i < c.find("inputs")->size(); ++i) {
            const double got = t.inverse1d(hex((*c.find("inputs"))[i])), want = hex((*c.find("inverse"))[i]);
            EXPECT_TRUE(same(got, want)) << "case " << checked << " inverse " << i << ": " << got << " instead of " << want;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 7);
}

TEST(Symlog, LocatorIsMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("locators")->items()) {
        ax::SymmetricalLogLocator loc;
        loc.base = hex(*c.find("base"));
        loc.linthresh = hex(*c.find("linthresh"));
        if (!c.find("subs")->isNull()) {
            loc.subs.clear();
            for (const Json& s : c.find("subs")->items()) loc.subs.push_back(s.asNumber());
        }
        const double vmin = hex((*c.find("range"))[0]), vmax = hex((*c.find("range"))[1]);
        const auto ticks = loc.tickValues(vmin, vmax);
        const Json& want = *c.find("ticks");
        ASSERT_EQ(ticks.size(), want.size()) << "case " << checked;
        for (std::size_t i = 0; i < want.size(); ++i) EXPECT_TRUE(same(ticks[i], hex(want[i]))) << "case " << checked << " tick " << i;
        for (const bool round : {false, true}) {
            const auto v = loc.viewLimits(vmin, vmax, round);
            const Json& w = *c.find(round ? "round_numbers" : "data");
            EXPECT_TRUE(same(v.first, hex(w[0])) && same(v.second, hex(w[1]))) << "case " << checked << (round ? " round" : " data");
        }
        ++checked;
    }
    EXPECT_EQ(checked, 270);
}

TEST(Symlog, LabelsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("labels")->items()) {
        ax::LogFormatterSciNotation f;
        f.base = hex(*c.find("base"));
        f.linthresh = hex(*c.find("linthresh"));
        f.setLocs(hex((*c.find("range"))[0]), hex((*c.find("range"))[1]));
        for (std::size_t i = 0; i < c.find("x")->size(); ++i)
            EXPECT_EQ(f(hex((*c.find("x"))[i])), (*c.find("labels"))[i].asString()) << "case " << checked << " x " << i;
        ++checked;
    }
    EXPECT_EQ(checked, 90);
}
