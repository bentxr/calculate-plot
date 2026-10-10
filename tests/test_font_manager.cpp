// The font manager picks the fonts matplotlib 3.11.2 picks: FontManager._find_fonts_by_props over
// the bundled fonts, for 2000 font properties recorded from matplotlib (tests/oracle/findfont.json).
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "text/font_manager.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::text;

namespace {

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/findfont.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

FontValue value(const Json& j) {
    if (j.isNumber()) return int(j.asInteger());
    return j.asString();
}

FontProperties properties(const Json& c, const calculate_plot::rc::Params& rc) {
    FontProperties p = FontProperties::defaults(rc);
    p.family.clear();
    for (const Json& f : c.find("family")->items()) p.family.push_back(f.asString());
    p.style = c.find("style")->asString();
    p.variant = c.find("variant")->asString();
    p.weight = value(*c.find("weight"));
    p.stretch = value(*c.find("stretch"));
    const Json& size = *c.find("size");
    if (size.isNumber()) p.size = size.asNumber();
    else if (size.isString()) p.size = *fontSize(size.asString(), rc);
    return p;
}

std::vector<std::string> files(const Json& c) {
    std::vector<std::string> out;
    for (const Json& f : c.find("files")->items()) out.push_back(f.asString());
    return out;
}

TEST(FontManager, ListsTheBundledFonts) {
    EXPECT_EQ(bundledFonts().size(), 40u);
    EXPECT_EQ(bundledFonts()[3].file, "DejaVuSans.ttf");
    EXPECT_EQ(bundledFonts()[3].name, "DejaVu Sans");
}

TEST(FontManager, DefaultPropertiesFindDejaVuSans) {
    const auto rc = calculate_plot::rc::Params::defaults();
    EXPECT_EQ(findFontsByProps(FontProperties::defaults(rc), rc), std::vector<std::string>{"DejaVuSans.ttf"});
}

TEST(FontManager, FindsWhatMatplotlibFinds) {
    const auto rc = calculate_plot::rc::Params::defaults();
    int checked = 0;
    const Json cases = oracle();
    for (const Json& c : cases.items()) {
        ++checked;
        EXPECT_EQ(findFontsByProps(properties(c, rc), rc), files(c)) << "case " << checked;
    }
    EXPECT_EQ(checked, 2000);
}

// matplotlib's own list order comes from a Python set; no lookup of the bundled fonts depends on it.
TEST(FontManager, LookupsDoNotDependOnTheListOrder) {
    const auto rc = calculate_plot::rc::Params::defaults();
    std::vector<FontEntry> reversed(bundledFonts().rbegin(), bundledFonts().rend());
    const Json cases = oracle();
    for (const Json& c : cases.items())
        EXPECT_EQ(findFontsByProps(properties(c, rc), rc, &reversed), files(c));
}

TEST(FontManager, SizeNamesScaleTheDefaultSize) {
    const auto rc = calculate_plot::rc::Params::defaults();
    EXPECT_EQ(*fontSize("medium", rc), 10.0);
    EXPECT_EQ(*fontSize("xx-large", rc), 17.28);
    EXPECT_EQ(*fontSize("smaller", rc), 8.33);
    EXPECT_FALSE(fontSize("huge", rc).has_value());
}

TEST(FontManager, FamiliesFollowTheRcParams) {
    auto rc = calculate_plot::rc::Params::defaults();
    std::string error;
    ASSERT_TRUE(rc.set("font.sans-serif", *calculate_plot::support::parseJson(R"(["DejaVu Serif"])", nullptr), &error)) << error;
    EXPECT_EQ(findFontsByProps(FontProperties::defaults(rc), rc), std::vector<std::string>{"DejaVuSerif.ttf"});
}

}  // namespace
