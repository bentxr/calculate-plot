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
    int index = 0, writtenOut = 0;
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
        // with rcParams["axes.formatter.min_exponent"], small powers are written out
        LogFormatterSciNotation plain;
        plain.minExponent = int(c.find("min_exponent")->asInteger());
        for (const char* which : {"major", "minor"}) {
            labels.clear(), want.clear();
            plain.setLocs(view[0], view[1]);
            for (double v : std::string(which) == "major" ? majorLocs : minorLocs) labels.push_back(plain(v));
            for (const Json& l : c.find(std::string(which) + "_labels_min")->items()) want.push_back(l.asString());
            EXPECT_EQ(labels, want) << which << " with min_exponent " << plain.minExponent;
            for (std::size_t k = 0; k < labels.size(); ++k) writtenOut += labels[k].find('^') == std::string::npos && !labels[k].empty();
        }
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
    EXPECT_GT(writtenOut, 1000);
}

}  // namespace

// The labels of negative values and of zero (a symmetric log axis has them), as matplotlib 3.11.2
// writes them: LogFormatterSciNotation()(x), with rcParams["axes.formatter.min_exponent"] 0 and 3.
TEST(LogTicker, LabelsOfNegativeValuesAndZero) {
    using calculate_plot::axes::LogFormatterSciNotation;
    const double xs[] = {-100.0, -0.001, -2000.0, -5.0, -1e5, 0.0, 100.0};
    const char* const standard[] = {"$\\mathdefault{-10^{2}}$", "$\\mathdefault{-10^{-3}}$", "$\\mathdefault{-2\\times10^{3}}$",
                                    "$\\mathdefault{-5\\times10^{0}}$", "$\\mathdefault{-10^{5}}$", "$\\mathdefault{0}$",
                                    "$\\mathdefault{10^{2}}$"};
    const char* const writtenOut[] = {"$\\mathdefault{-100}$", "$\\mathdefault{-10^{-3}}$", "$\\mathdefault{-2\\times10^{3}}$",
                                      "$\\mathdefault{-5}$", "$\\mathdefault{-10^{5}}$", "$\\mathdefault{0}$", "$\\mathdefault{100}$"};
    LogFormatterSciNotation f, g;
    g.minExponent = 3;
    for (std::size_t i = 0; i < std::size(xs); ++i) {
        EXPECT_EQ(f(xs[i]), standard[i]);
        EXPECT_EQ(g(xs[i]), writtenOut[i]);
    }
}
