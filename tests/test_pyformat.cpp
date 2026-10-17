// Doubles spelled exactly as Python and NumPy spell them (the oracle was written by Python).
#include "engine/text.hpp"
#include "support/json.hpp"
#include "support/pyformat.hpp"

#include <gtest/gtest.h>

#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

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

// cbook._auto_format_str (bar_label's fmt) against matplotlib 3.11.2 (tests/oracle/auto_format.json, written by
// tools/oracle/make_format_oracle.py): % formatting, then str.format, of a np.float64 or a np.int64.
TEST(PythonFormat, AutoFormatIsMatplotlibs) {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/auto_format.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto doc = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(doc.has_value());
    // (the values are read before the locale changes: strtod follows it)
    std::vector<double> values;
    for (const auto& c : doc->find("cases")->items())
        values.push_back(c.find("int")->asBool() ? c.find("value")->asNumber() : std::strtod(c.find("value")->asString().c_str(), nullptr));
    int same = 0, raised = 0, unsupported = 0;
    const auto run = [&] {
        same = raised = unsupported = 0;
        std::size_t k = 0;
        for (const auto& c : doc->find("cases")->items()) {
            const bool isInt = c.find("int")->asBool();
            const double v = values[k++];
            const std::string fmt = c.find("fmt")->asString();
            const auto got = calculate_plot::engine::autoFormatStr(fmt, v, isInt);
            const std::string where = "fmt " + fmt + " value " + std::to_string(v) + (isInt ? " (int)" : "");
            if (got.kind == calculate_plot::support::Formatted::Unsupported) {
                ++unsupported;
                continue;
            }
            if (const auto* e = c.find("error")) {
                EXPECT_EQ(got.kind, calculate_plot::support::Formatted::Raised) << where << ": " << got.text;
                EXPECT_EQ(got.text, e->asString()) << where;
                ++raised;
            } else {
                EXPECT_EQ(got.kind, calculate_plot::support::Formatted::Text) << where << ": " << got.text;
                EXPECT_EQ(got.text, c.find("text")->asString()) << where;
                ++same;
            }
        }
    };
    run();
    EXPECT_EQ(same + raised + unsupported, 1800);
    EXPECT_LE(unsupported, 120) << "forms not ported";
    std::printf("auto format: %d equal, %d raised alike, %d not supported\n", same, raised, unsupported);
    // whatever the locale's decimal point
    const char* old = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = old ? old : "C";
    if (std::setlocale(LC_NUMERIC, "es_ES.UTF-8")) {
        const int before = same;
        run();
        EXPECT_EQ(same, before);
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}
