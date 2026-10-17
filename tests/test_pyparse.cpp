// float(text), int(text) and text.strip() as Python reads them, for 1439 strings (tests/oracle/pyparse.json, written
// by tools/oracle/make_pyparse_oracle.py). Strings of ASCII characters are read exactly as Python
// reads them; the digits and blanks of other scripts, which Python also takes, are refused.
#include "support/json.hpp"
#include "support/pyparse.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using calculate_plot::support::pythonFloat;
using calculate_plot::support::pythonInt;

TEST(PythonParse, ReadsNumbersAsPythonDoes) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/pyparse.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    int floats = 0, integers = 0, notAscii = 0;
    for (const Json& c : cases.items()) {
        const std::string s = c.find("text")->asString();
        SCOPED_TRACE(s);
        bool ascii = true;
        for (unsigned char ch : s) ascii = ascii && ch < 0x80;
        notAscii += !ascii;
        EXPECT_EQ(calculate_plot::support::pythonStrip(s), c.find("strip")->asString());
        EXPECT_EQ(calculate_plot::support::isAscii(s), ascii);
        const auto f = pythonFloat(s);
        const auto i = pythonInt(s);
        const Json& wantFloat = *c.find("float");
        const Json& wantInt = *c.find("int");
        if (!ascii) {
            EXPECT_FALSE(f.has_value());
            EXPECT_FALSE(i.has_value());
            continue;
        }
        ASSERT_EQ(f.has_value(), !wantFloat.isNull());
        if (f) {
            const double want = std::strtod(wantFloat.asString().c_str(), nullptr);
            if (wantFloat.asString() == "nan") {
                EXPECT_TRUE(std::isnan(*f));
            } else {
                EXPECT_EQ(*f, want);
                EXPECT_EQ(std::signbit(*f), std::signbit(want));
            }
            ++floats;
        }
        ASSERT_EQ(i.has_value(), !wantInt.isNull());
        if (i) {
            // an integer too large for 64 bits is the nearest end of their range
            const std::string& digits = wantInt.asString();
            errno = 0;
            const long long want = std::strtoll(digits.c_str(), nullptr, 10);
            EXPECT_EQ(*i, want);
            ++integers;
        }
    }
    EXPECT_EQ(cases.size(), 1439u);
    EXPECT_EQ(notAscii, 37);
    EXPECT_EQ(floats, 477);
    EXPECT_EQ(integers, 352);
}
