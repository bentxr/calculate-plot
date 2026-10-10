// Log axes tick and label as matplotlib 3.11.2's: LogLocator (major and minor ticks, nonsingular,
// view_limits) and LogFormatterSciNotation, for 600 random log axes (tests/oracle/log_ticker.json).
#include "axes/log_ticker.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::axes;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

std::vector<double> hexes(const Json& j) {
    std::vector<double> out;
    for (const Json& v : j.items()) out.push_back(hex(v));
    return out;
}

TEST(LogTicker, TicksAndLabelsAsMatplotlib) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/log_ticker.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    int index = 0;
    for (const Json& c : cases.items()) {
        SCOPED_TRACE("case " + std::to_string(index++));
        const auto view = hexes(*c.find("view"));
        LogLocator major;
        major.tickSpace = int(c.find("tick_space")->asInteger());
        LogLocator minor = major;
        minor.subs.clear();
        minor.autoSubs = true;
        const auto majorLocs = major.tickValues(view[0], view[1]);
        EXPECT_EQ(majorLocs, hexes(*c.find("major")));
        const auto minorLocs = withoutMajorTicks(minor.tickValues(view[0], view[1]), majorLocs, view[0], view[1], 10.0);
        EXPECT_EQ(minorLocs, hexes(*c.find("minor")));
        LogFormatterSciNotation fmt;
        fmt.setLocs(view[0], view[1]);
        std::vector<std::string> labels;
        for (double v : majorLocs) labels.push_back(fmt(v));
        std::vector<std::string> want;
        for (const Json& l : c.find("major_labels")->items()) want.push_back(l.asString());
        EXPECT_EQ(labels, want);
        labels.clear(), want.clear();
        fmt.setLocs(view[0], view[1]);
        for (double v : minorLocs) labels.push_back(fmt(v));
        for (const Json& l : c.find("minor_labels")->items()) want.push_back(l.asString());
        EXPECT_EQ(labels, want);
        const auto in2 = hexes(*c.find("nonsingular_in"));
        const auto ns = major.nonsingular(in2[0], in2[1]);
        EXPECT_EQ(ns.first, hex((*c.find("nonsingular"))[0]));
        EXPECT_EQ(ns.second, hex((*c.find("nonsingular"))[1]));
        // view_limits(lo, hi) of the limits given to set_*lim, which the view keeps (reversed or not)
        const auto vl = major.viewLimits(view[0], view[1], false);
        const auto wantVl = hexes(*c.find("view_limits"));
        EXPECT_EQ(vl.first, wantVl[0]);
        EXPECT_EQ(vl.second, wantVl[1]);
    }
    EXPECT_EQ(index, 600);
}

}  // namespace
