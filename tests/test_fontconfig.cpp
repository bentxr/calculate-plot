// Fontconfig patterns as matplotlib 3.11.2 reads them (tests/oracle/fontconfig.json, written by
// tools/oracle/make_fontconfig_oracle.py), for 705 patterns: which ones parse, the font properties
// they give over the rcParams' defaults, where FontProperties refuses them, and the bundled font
// findfont then picks.
#include "rc/rc_params.hpp"
#include "support/fontconfig_pattern.hpp"
#include "support/json.hpp"
#include "text/font_manager.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::text;

namespace {

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/fontconfig.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

void expectValue(const FontValue& got, const Json& want) {
    if (want.isNumber()) {
        ASSERT_TRUE(std::holds_alternative<int>(got));
        EXPECT_EQ(std::get<int>(got), int(want.asInteger()));
    } else {
        ASSERT_TRUE(std::holds_alternative<std::string>(got));
        EXPECT_EQ(std::get<std::string>(got), want.asString());
    }
}

TEST(FontconfigPattern, ReadsPatternsAsMatplotlibDoes) {
    const Json cases = oracle();
    const auto rc = calculate_plot::rc::Params::defaults();
    int invalid = 0, broken = 0, withFile = 0, found = 0;
    for (const Json& c : cases.items()) {
        const std::string pattern = c.find("pattern")->asString();
        SCOPED_TRACE("pattern [" + pattern + "]");
        const bool parses = calculate_plot::support::parseFontconfigPattern(pattern).has_value();
        FontProperties prop;
        std::optional<std::string> file;
        std::string error;
        const bool made = fontPropertiesFromPattern(pattern, rc, &prop, &file, &error);
        if (c.find("invalid")) {
            EXPECT_FALSE(parses);
            EXPECT_FALSE(made);
            EXPECT_NE(error.find("is not a fontconfig pattern"), std::string::npos) << error;
            ++invalid;
            continue;
        }
        EXPECT_TRUE(parses);
        if (const Json* why = c.find("broken")) {
            // FontProperties has no such property (AttributeError), or refuses its value (ValueError)
            EXPECT_FALSE(made);
            const bool attribute = why->asString().compare(0, 14, "AttributeError") == 0;
            EXPECT_EQ(error.find("is not a font property") != std::string::npos, attribute) << error << " / " << why->asString();
            EXPECT_FALSE(error.empty());
            ++broken;
            continue;
        }
        ASSERT_TRUE(made) << error;
        const Json& family = *c.find("family");
        ASSERT_EQ(prop.family.size(), family.size());
        for (std::size_t i = 0; i < family.size(); ++i) EXPECT_EQ(prop.family[i], family[i].asString());
        EXPECT_EQ(prop.style, c.find("style")->asString());
        EXPECT_EQ(prop.variant, c.find("variant")->asString());
        expectValue(prop.weight, *c.find("weight"));
        expectValue(prop.stretch, *c.find("stretch"));
        EXPECT_EQ(prop.size, std::strtod(c.find("size")->asString().c_str(), nullptr));
        const Json& wantFile = *c.find("file");
        EXPECT_EQ(file.has_value(), !wantFile.isNull());
        if (file) {
            EXPECT_EQ(*file, wantFile.asString());
            ++withFile;
        } else {
            EXPECT_EQ(findFont(prop, rc), c.find("font")->asString());
            ++found;
        }
    }
    EXPECT_EQ(invalid, 221);
    EXPECT_EQ(broken, 34);
    EXPECT_EQ(withFile, 6);
    EXPECT_EQ(found, 444);
}

// The properties a pattern leaves alone are the rcParams'.
TEST(FontconfigPattern, StartsFromTheRcParams) {
    auto rc = calculate_plot::rc::Params::defaults();
    ASSERT_TRUE(rc.set("font.style", *calculate_plot::support::parseJson(R"("italic")", nullptr), nullptr));
    ASSERT_TRUE(rc.set("font.size", *calculate_plot::support::parseJson("17", nullptr), nullptr));
    ASSERT_TRUE(rc.set("font.weight", *calculate_plot::support::parseJson(R"("bold")", nullptr), nullptr));
    FontProperties prop;
    std::optional<std::string> file;
    std::string error;
    ASSERT_TRUE(fontPropertiesFromPattern("serif", rc, &prop, &file, &error)) << error;
    EXPECT_EQ(prop.family, std::vector<std::string>{"serif"});
    EXPECT_EQ(prop.style, "italic");
    EXPECT_EQ(prop.size, 17.0);
    EXPECT_EQ(std::get<std::string>(prop.weight), "bold");
    ASSERT_TRUE(fontPropertiesFromPattern("serif-9:roman:light", rc, &prop, &file, &error)) << error;
    EXPECT_EQ(prop.style, "normal");
    EXPECT_EQ(prop.size, 9.0);
    EXPECT_EQ(std::get<std::string>(prop.weight), "light");
}

// The rcParams take a math font only as a pattern that parses (validate_font_properties).
TEST(FontconfigPattern, TheRcParamsRefuseWhatDoesNotParse) {
    auto rc = calculate_plot::rc::Params::defaults();
    std::string error;
    for (const char* key : {"mathtext.rm", "mathtext.it", "mathtext.bf", "mathtext.bfit", "mathtext.cal", "mathtext.sf", "mathtext.tt"}) {
        EXPECT_TRUE(rc.set(key, *calculate_plot::support::parseJson(R"("DejaVu Serif:italic:weight=bold")", nullptr), &error)) << key << error;
        EXPECT_FALSE(rc.set(key, *calculate_plot::support::parseJson(R"("sans-serif")", nullptr), &error)) << key;
        EXPECT_NE(error.find("'sans-serif' is not a fontconfig pattern"), std::string::npos) << error;
        EXPECT_FALSE(rc.set(key, *calculate_plot::support::parseJson(R"("sans:boldx")", nullptr), &error)) << key;
        EXPECT_FALSE(rc.set(key, *calculate_plot::support::parseJson("3", nullptr), &error)) << key;
        EXPECT_EQ(rc.text(key), "DejaVu Serif:italic:weight=bold");
        // (a pattern FontProperties refuses is still a pattern: matplotlib takes it here too)
        EXPECT_TRUE(rc.set(key, *calculate_plot::support::parseJson(R"("sans:condensed")", nullptr), &error)) << key << error;
    }
}

}  // namespace
