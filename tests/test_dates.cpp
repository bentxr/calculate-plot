// matplotlib's dates (date2num, num2date, AutoDateLocator, AutoDateFormatter) against matplotlib 3.11.2 and
// dateutil 2.9.0 (tests/oracle/dates.json, written by tools/oracle/make_dates_oracle.py).
#include "axes/dates.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace support = calculate_plot::support;
namespace dates = calculate_plot::axes::dates;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/dates.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }
bool same(double a, double b) { return (std::isnan(a) && std::isnan(b)) || (a == b && std::signbit(a) == std::signbit(b)); }

}  // namespace

TEST(Dates, ConversionsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("conversions")->items()) {
        dates::DateTime d;
        ASSERT_TRUE(dates::parseIso(c.find("datetime")->asString(), &d)) << c.find("datetime")->asString();
        EXPECT_TRUE(same(dates::date2num(d), hex(*c.find("num")))) << c.find("datetime")->asString() << ": " << dates::date2num(d);
        EXPECT_EQ(dates::isoFormat(d), c.find("datetime")->asString());
        ++checked;
    }
    for (const Json& c : oracle().find("backs")->items()) {
        dates::DateTime d;
        std::string error;
        ASSERT_TRUE(dates::num2date(hex(*c.find("num")), &d, &error)) << error;
        EXPECT_EQ(dates::isoFormat(d), c.find("datetime")->asString()) << hex(*c.find("num"));
        ++checked;
    }
    EXPECT_EQ(checked, 47 + 41);
}

TEST(Dates, AutoDateLocatorAndFormatterAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("locators")->items()) {
        dates::AutoDateLocator loc;
        loc.intervalMultiples = c.find("multiples")->asBool();
        if (c.find("view")->isNull()) {
            const auto ns = loc.nonsingular(hex((*c.find("nonsingular_of"))[0]), hex((*c.find("nonsingular_of"))[1]));
            EXPECT_TRUE(same(ns.first, hex((*c.find("nonsingular"))[0])) && same(ns.second, hex((*c.find("nonsingular"))[1]))) << checked;
            ++checked;
            continue;
        }
        const double vmin = hex((*c.find("view"))[0]), vmax = hex((*c.find("view"))[1]);
        std::vector<double> ticks;
        std::string error;
        ASSERT_TRUE(loc.ticks(vmin, vmax, &ticks, &error)) << error;
        const Json& want = *c.find("ticks");
        ASSERT_EQ(ticks.size(), want.size()) << "case " << checked << " view " << vmin << " " << vmax;
        dates::AutoDateFormatter fmt;
        EXPECT_TRUE(same(loc.unit(), hex(*c.find("unit")))) << "case " << checked;
        for (std::size_t i = 0; i < want.size(); ++i) {
            EXPECT_TRUE(same(ticks[i], hex(want[i]))) << "case " << checked << " tick " << i << ": " << ticks[i] << " instead of " << hex(want[i]);
            std::string label;
            ASSERT_TRUE(fmt.format(ticks[i], loc.unit(), &label, &error)) << error;
            EXPECT_EQ(label, (*c.find("labels"))[i].asString()) << "case " << checked << " tick " << i;
        }
        const auto ns = loc.nonsingular(vmin, vmax);
        EXPECT_TRUE(same(ns.first, hex((*c.find("nonsingular"))[0])) && same(ns.second, hex((*c.find("nonsingular"))[1]))) << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 387);
}
