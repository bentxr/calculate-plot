// Doubles spelled exactly as Python and NumPy spell them (the oracle was written by Python).
#include "support/json.hpp"
#include "support/pyformat.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace py = calculate_plot::support;

namespace {

std::vector<py::Json> oracle() {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/float_format.json");
    std::ostringstream text;
    text << in.rdbuf();
    auto json = py::parseJson(text.str(), nullptr);
    return json ? json->items() : std::vector<py::Json>{};
}

}  // namespace

TEST(PythonFormat, ReprMatchesPythonForEveryOracleDouble) {
    const auto cases = oracle();
    ASSERT_GT(cases.size(), 1000u);
    for (const auto& c : cases) {
        const double x = std::strtod(c.find("hex")->asString().c_str(), nullptr);
        EXPECT_EQ(py::pythonRepr(x), c.find("repr")->asString()) << c.find("hex")->asString();
    }
}

TEST(PythonFormat, NumpyReprMatchesNumpy) {
    for (const auto& c : oracle()) {
        const double x = std::strtod(c.find("hex")->asString().c_str(), nullptr);
        EXPECT_EQ(py::numpyRepr(x), c.find("numpy")->asString()) << c.find("hex")->asString();
    }
}

TEST(PythonFormat, ShortFloatMatchesMatplotlib) {
    int checked = 0;
    for (const auto& c : oracle()) {
        if (c.find("short")->isNull()) continue;
        const double x = std::strtod(c.find("hex")->asString().c_str(), nullptr);
        EXPECT_EQ(py::shortFloat(x), c.find("short")->asString()) << c.find("hex")->asString();
        ++checked;
    }
    EXPECT_GT(checked, 500);
}

TEST(PythonFormat, NonFiniteValues) {
    EXPECT_EQ(py::pythonRepr(std::numeric_limits<double>::infinity()), "inf");
    EXPECT_EQ(py::pythonRepr(-std::numeric_limits<double>::infinity()), "-inf");
    EXPECT_EQ(py::pythonRepr(std::nan("")), "nan");
}

TEST(PythonFormat, FixedIgnoresTheLocale) {
    EXPECT_EQ(py::fixed(1.5, 6), "1.500000");
    EXPECT_EQ(py::fixed(-0.0, 6), "-0.000000");
    EXPECT_EQ(py::fixed(0.0000005, 6), "0.000000");  // 5e-7 is just below the halfway point
    EXPECT_EQ(py::fixed(2.5, 0), "2");                // halves to even, as printf and Python do
}
