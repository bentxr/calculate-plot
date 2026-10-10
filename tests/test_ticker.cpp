// Gate G4: matplotlib's default tick locator and formatter, ported, give exactly matplotlib's
// ticks (bit for bit), labels and offset text, for 9135 view limits recorded from matplotlib.
#include "axes/ticker.hpp"
#include "support/json.hpp"
#include "support/pynum.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ax = calculate_plot::axes;
namespace pn = calculate_plot::support;
using calculate_plot::support::Json;

namespace {

std::vector<Json> oracle() {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/ticker.json");
    std::ostringstream text;
    text << in.rdbuf();
    auto json = calculate_plot::support::parseJson(text.str(), nullptr);
    return json ? json->items() : std::vector<Json>{};
}

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

ax::MaxNLocator locatorFor(const Json& c) {
    const std::string name = c.find("locator")->asString();
    if (name == "auto") return ax::MaxNLocator::autoLocator();
    ax::MaxNLocator l;
    const Json& p = *c.find("params");
    if (const Json* v = p.find("nbins")) l.nbins = int(v->asNumber());
    if (const Json* v = p.find("integer")) l.integer = v->asBool();
    if (const Json* v = p.find("symmetric")) l.symmetric = v->asBool();
    if (const Json* v = p.find("prune")) l.prune = v->asString();
    if (const Json* v = p.find("round_numbers")) l.roundNumbers = v->asBool();
    if (const Json* v = p.find("steps")) {
        std::vector<double> steps;
        for (const Json& s : v->items()) steps.push_back(s.asNumber());
        l.setSteps(steps);
    }
    return l;
}

std::string hexText(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%a", v);
    return buffer;
}

}  // namespace

TEST(Ticker, TickValuesMatchMatplotlibBitForBit) {
    const auto cases = oracle();
    ASSERT_GT(cases.size(), 10900u);
    int mismatches = 0;
    for (const auto& c : cases) {
        const double vmin = hex(*c.find("vmin")), vmax = hex(*c.find("vmax"));
        const auto ticks = locatorFor(c).tickValues(vmin, vmax);
        const auto& want = c.find("ticks")->items();
        bool same = ticks.size() == want.size();
        for (std::size_t i = 0; same && i < ticks.size(); ++i) same = ticks[i] == hex(want[i]) && std::signbit(ticks[i]) == std::signbit(hex(want[i]));
        if (!same && ++mismatches <= 10) {
            std::string got, exp;
            for (double t : ticks) got += hexText(t) + " ";
            for (const auto& w : want) exp += w.asString() + " ";
            ADD_FAILURE() << c.find("locator")->asString() << " [" << c.find("vmin")->asString() << ", "
                          << c.find("vmax")->asString() << "]\n  ours:       " << got << "\n  matplotlib: " << exp;
        }
    }
    EXPECT_EQ(mismatches, 0);
}

TEST(Ticker, LabelsAndOffsetMatchMatplotlib) {
    int checked = 0, mismatches = 0;
    for (const auto& c : oracle()) {
        if (!c.find("labels")) continue;
        std::vector<double> ticks;
        for (const auto& t : c.find("ticks")->items()) ticks.push_back(hex(t));
        ax::ScalarFormatter f;
        f.setViewInterval(hex(*c.find("vmin")), hex(*c.find("vmax")));
        f.setLocs(ticks);
        const auto& labels = c.find("labels")->items();
        std::string got, want;
        for (std::size_t i = 0; i < ticks.size(); ++i) {
            got += f(ticks[i]) + " | ";
            want += labels[i].asString() + " | ";
        }
        got += "offset " + f.offsetText();
        want += "offset " + c.find("offset")->asString();
        if (got != want && ++mismatches <= 10)
            ADD_FAILURE() << "[" << c.find("vmin")->asString() << ", " << c.find("vmax")->asString() << "]\n  ours:       "
                          << got << "\n  matplotlib: " << want;
        ++checked;
    }
    EXPECT_GT(checked, 1800);
    EXPECT_EQ(mismatches, 0);
}

TEST(PythonNumbers, FloorDivisionAndDivmod) {
    EXPECT_EQ(pn::floorDivide(7.0, 2.0), 3.0);
    EXPECT_EQ(pn::floorDivide(-7.0, 2.0), -4.0);
    EXPECT_EQ(pn::floorDivide(7.0, -2.0), -4.0);
    EXPECT_TRUE(std::signbit(pn::floorDivide(-0.5, 2.0)) && pn::floorDivide(-0.5, 2.0) == -1.0);
    EXPECT_TRUE(std::signbit(pn::floorDivide(-0.0, 2.0)));  // 0 with the sign of the quotient
    EXPECT_EQ(pn::floorDivide(1.0, 0.1), 9.0);              // as Python: 1 // 0.1 == 9.0
    const auto [q, r] = pn::divmod(-7.5, 2.0);
    EXPECT_EQ(q, -4.0);
    EXPECT_EQ(r, 0.5);
    EXPECT_TRUE(std::signbit(pn::divmod(4.0, -2.0).second));  // the remainder takes the divisor's sign
}

TEST(PythonNumbers, RoundingAndFormatting) {
    EXPECT_EQ(pn::npRound(0.125, 2), 0.12);  // x * 100, rint, / 100
    EXPECT_EQ(pn::npRound(2.675, 2), 2.68);  // NumPy multiplies first (Python's round(2.675, 2) is 2.67)
    EXPECT_EQ(pn::pythonRound(2.675, 2), 2.67);
    EXPECT_EQ(pn::pythonRound(0.30000000000000004, 10), 0.3);
    EXPECT_EQ(pn::percentF(-0.0, 1), "-0.0");
    EXPECT_EQ(pn::percentF(1234.5, 0), "1234");
    EXPECT_EQ(pn::percentG(1.23456789012345, 10), "1.23456789");
    EXPECT_EQ(pn::percentG(1e-5, 10), "1e-05");
    EXPECT_EQ(pn::percentG(123456789012.0, 10), "1.23456789e+11");
}
