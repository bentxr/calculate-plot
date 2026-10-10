// Mathtext lays expressions out as matplotlib 3.11.2 does (MathTextParser('path').parse), for 267
// expressions of the supported subset at several sizes and dpi (tests/oracle/mathtext.json, written
// by tools/oracle/make_mathtext_oracle.py): box size, every glyph and its position, to the last bit.
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "text/mathtext.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::text;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

TEST(Mathtext, LaysOutAsMatplotlib) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/mathtext.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    const auto rc = calculate_plot::rc::Params::defaults();
    FontCache fonts(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    int checked = 0;
    for (const Json& c : cases.items()) {
        const std::string s = c.find("s")->asString();
        SCOPED_TRACE(s);
        FontProperties prop = FontProperties::defaults(rc);
        prop.size = c.find("size")->asNumber();
        std::string error;
        const auto p = parseMath(fonts, s, c.find("dpi")->asNumber(), prop, rc, &error);
        ASSERT_TRUE(p.has_value()) << error;
        EXPECT_EQ(p->width, hex(*c.find("width")));
        EXPECT_EQ(p->height, hex(*c.find("height")));
        EXPECT_EQ(p->depth, hex(*c.find("depth")));
        const Json& glyphs = *c.find("glyphs");
        ASSERT_EQ(p->glyphs.size(), glyphs.size());
        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            const Json& g = glyphs[i];
            EXPECT_EQ(p->glyphs[i].file, g.find("font")->asString()) << i;
            EXPECT_EQ(p->glyphs[i].fontsize, hex(*g.find("size"))) << i;
            EXPECT_EQ(p->glyphs[i].num, unsigned(g.find("num")->asInteger())) << i;
            EXPECT_EQ(p->glyphs[i].index, FT_UInt(g.find("index")->asInteger())) << i;
            EXPECT_EQ(p->glyphs[i].ox, hex(*g.find("ox"))) << i;
            EXPECT_EQ(p->glyphs[i].oy, hex(*g.find("oy"))) << i;
        }
        EXPECT_EQ(p->rects.size(), c.find("rects")->size());
        ++checked;
    }
    EXPECT_EQ(checked, 267);
}

TEST(Mathtext, RefusesWhatItDoesNotKnow) {
    const auto rc = calculate_plot::rc::Params::defaults();
    FontCache fonts(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    std::string error;
    EXPECT_FALSE(parseMath(fonts, R"($\nosuchcommand$)", 100, FontProperties::defaults(rc), rc, &error));
    EXPECT_NE(error.find("Unknown symbol: \\nosuchcommand"), std::string::npos) << error;
    EXPECT_FALSE(parseMath(fonts, R"($\cdotT$)", 100, FontProperties::defaults(rc), rc, &error));
    EXPECT_FALSE(parseMath(fonts, R"($x^2^3$)", 100, FontProperties::defaults(rc), rc, &error));
    EXPECT_NE(error.find("Double superscript"), std::string::npos) << error;
    EXPECT_FALSE(parseMath(fonts, R"($\frac{1}{2}$)", 100, FontProperties::defaults(rc), rc, &error));
    EXPECT_NE(error.find("not supported yet"), std::string::npos) << error;
    EXPECT_FALSE(parseMath(fonts, R"($x{$)", 100, FontProperties::defaults(rc), rc, &error));
}

}  // namespace
