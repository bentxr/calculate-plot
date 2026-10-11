// Mathtext lays expressions out as matplotlib 3.11.2 does (MathTextParser('path').parse), for 6410
// expressions at several sizes and dpi, with every fontset and with the settings that reach
// mathtext (tests/oracle/mathtext.json, written by tools/oracle/make_mathtext_oracle.py): box size,
// every glyph and its position and every rule, to the last bit; and it refuses the expressions
// matplotlib refuses or breaks on.
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "text/mathtext.hpp"
#include "text/mathtext_fonts.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::text;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

struct Tally {
    int checked = 0, refused = 0, withFontset = 0, withSettings = 0, withRules = 0;
};

void checkCase(const Json& c, FontCache& fonts, Tally& tally) {
    const std::string s = c.find("s")->asString();
    auto rc = calculate_plot::rc::Params::defaults();
    std::string fontset = "dejavusans";
    if (const Json* f = c.find("fontset")) {
        fontset = f->asString();
        ASSERT_TRUE(rc.set("mathtext.fontset", *f, nullptr));
        ++tally.withFontset;
    }
    if (const Json* settings = c.find("rc")) {
        for (const auto& [key, value] : settings->members()) {
            std::string why;
            ASSERT_TRUE(rc.set(key, value, &why)) << key << ": " << why;
            fontset += " " + key;
        }
        ++tally.withSettings;
    }
    SCOPED_TRACE(s + " [" + fontset + "]");
    FontProperties prop = FontProperties::defaults(rc);
    prop.size = c.find("size")->asNumber();
    std::string error;
    const auto p = parseMath(fonts, s, c.find("dpi")->asNumber(), prop, rc, &error);
    if (const Json* e = c.find("error")) {
        // What the grammar refuses is only refused (its messages are pyparsing's); what a handler
        // raises is matplotlib's message, and where matplotlib breaks we say on what.
        EXPECT_FALSE(p.has_value());
        EXPECT_FALSE(error.empty());
        const std::string theirs = e->asString();
        const auto starts = [&](const std::string& prefix) { return theirs.compare(0, prefix.size(), prefix) == 0; };
        std::vector<std::string> expected;
        for (const char* kind : {"ParseFatalException: ", "ParseSyntaxException: "}) {
            if (!starts(kind)) continue;
            const std::size_t from = std::char_traits<char>::length(kind);
            const std::string message = theirs.substr(from, std::min(theirs.find(", found"), theirs.find("  (at char")) - from);
            if (message.compare(0, 9, "Expected ") != 0 || message == "Expected '}'") expected = {message};
        }
        if (starts("TypeError: 'str' object is not callable")) expected = {"\\substack takes neither"};
        if (starts("AttributeError: 'str' object has no attribute 'get_kerning'"))
            expected = {"\\substack takes neither", "a delimiter was left unsized"};
        if (starts("max() iterable argument is empty")) expected = {"\\substack needs a row", "nothing between the delimiters"};
        bool said = expected.empty();
        for (const std::string& message : expected) said = said || error.find(message) != std::string::npos;
        EXPECT_TRUE(said) << "ours: " << error << "; matplotlib's: " << theirs;
        ++tally.refused;
        return;
    }
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
    const Json& rects = *c.find("rects");
    ASSERT_EQ(p->rects.size(), rects.size());
    for (std::size_t i = 0; i < rects.size(); ++i)
        for (std::size_t k = 0; k < 4; ++k) EXPECT_EQ(p->rects[i][k], hex(rects[i][k])) << i << " " << k;
    tally.withRules += rects.size() > 0;
    ++tally.checked;
}

TEST(Mathtext, LaysOutAsMatplotlib) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/mathtext.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    FontCache fonts(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    Tally tally;
    for (const Json& c : cases.items()) checkCase(c, fonts, tally);
    EXPECT_EQ(tally.checked, 4931);
    EXPECT_EQ(tally.refused, 1479);
    EXPECT_EQ(tally.withFontset, 2201);
    EXPECT_EQ(tally.withSettings, 609);
    EXPECT_EQ(tally.withRules, 1127);
}

// Every symbol comes from the font and the glyph matplotlib takes it from: in each fontset, with
// each font command, and in the custom fontset with each fallback (tests/oracle/mathtext_glyphs.json,
// written by tools/oracle/make_mathtext_glyphs_oracle.py: rows of forty symbols).
TEST(Mathtext, TakesEverySymbolFromMatplotlibsFont) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/mathtext_glyphs.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json oracle = *calculate_plot::support::parseJson(text.str(), nullptr);
    const Json& names = *oracle.find("fonts");
    FontCache fonts(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    std::size_t rows = 0, glyphs = 0;
    for (const Json& c : oracle.find("cases")->items()) {
        const std::string s = c.find("s")->asString();
        auto rc = calculate_plot::rc::Params::defaults();
        ASSERT_TRUE(rc.set("mathtext.fontset", *c.find("fontset"), nullptr));
        std::string settings;
        if (const Json* more = c.find("rc"))
            for (const auto& [key, value] : more->members()) {
                ASSERT_TRUE(rc.set(key, value, nullptr)) << key;
                settings += " " + key + (value.isNull() ? " None" : " " + value.asString());
            }
        SCOPED_TRACE(s + " [" + c.find("fontset")->asString() + settings + "]");
        FontProperties prop = FontProperties::defaults(rc);
        prop.size = 10;
        std::string error;
        const auto p = parseMath(fonts, s, 100, prop, rc, &error);
        ASSERT_TRUE(p.has_value()) << error;
        const Json& want = *c.find("glyphs");
        ASSERT_EQ(p->glyphs.size(), want.size());
        for (std::size_t i = 0; i < want.size(); ++i) {
            EXPECT_EQ(p->glyphs[i].file, names[std::size_t(want[i][0].asInteger())].asString()) << "glyph " << i;
            EXPECT_EQ(p->glyphs[i].num, unsigned(want[i][1].asInteger())) << "glyph " << i;
            EXPECT_EQ(p->glyphs[i].index, FT_UInt(want[i][2].asInteger())) << "glyph " << i;
        }
        ++rows;
        glyphs += want.size();
    }
    EXPECT_EQ(rows, 2520u);
    EXPECT_EQ(glyphs, 95760u);
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
    EXPECT_FALSE(parseMath(fonts, R"($x{$)", 100, FontProperties::defaults(rc), rc, &error));
}

// Without its font files an expression is refused, not laid out with nothing.
TEST(Mathtext, RefusesWhenAFontIsMissing) {
    const fs::path empty = fs::path(CALCULATE_PLOT_BINARY_DIR) / "mathtext-no-fonts";
    fs::create_directories(empty);
    FontCache none(empty.string());
    for (const char* fontset : {"dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"}) {
        auto rc = calculate_plot::rc::Params::defaults();
        ASSERT_TRUE(rc.set("mathtext.fontset", *calculate_plot::support::parseJson(std::string("\"") + fontset + "\"", nullptr),
                           nullptr));
        for (const char* s : {"$x$", "plain", R"($\sum_i\left(\frac{a}{b}\right)'$)"}) {
            std::string error;
            EXPECT_FALSE(parseMath(none, s, 100, FontProperties::defaults(rc), rc, &error)) << fontset << " " << s;
            EXPECT_NE(error.find("cannot be opened"), std::string::npos) << fontset << " " << s << ": " << error;
        }
    }
}

// The names in messages are written in UTF-8.
TEST(Mathtext, WritesCodePointsAsUtf8) {
    EXPECT_EQ(mathtext::utf8(U'a'), "a");
    EXPECT_EQ(mathtext::utf8(U'\x7f'), "\x7f");
    EXPECT_EQ(mathtext::utf8(char32_t(0x80)), "\xc2\x80");
    EXPECT_EQ(mathtext::utf8(char32_t(0xe9)), "\xc3\xa9");
    EXPECT_EQ(mathtext::utf8(char32_t(0x7ff)), "\xdf\xbf");
    EXPECT_EQ(mathtext::utf8(char32_t(0x800)), "\xe0\xa0\x80");
    EXPECT_EQ(mathtext::utf8(char32_t(0x4e2d)), "\xe4\xb8\xad");
    EXPECT_EQ(mathtext::utf8(char32_t(0xffff)), "\xef\xbf\xbf");
    EXPECT_EQ(mathtext::utf8(char32_t(0x10000)), "\xf0\x90\x80\x80");
    EXPECT_EQ(mathtext::utf8(char32_t(0x1d49c)), "\xf0\x9d\x92\x9c");
    EXPECT_EQ(mathtext::utf8(char32_t(0x10ffff)), "\xf4\x8f\xbf\xbf");
}

}  // namespace
