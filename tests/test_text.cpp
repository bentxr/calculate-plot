// Text layout: for every plain string matplotlib drew in the corpus, calculate-plot's layout gives
// the same glyphs, fonts and positions (exactly, as doubles).
#include "support/json.hpp"
#include "text/text_render.h"

#include <gtest/gtest.h>

#include <clocale>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace text = calculate_plot::text;
using calculate_plot::support::Json;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

const std::string kFonts = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

std::string fileName(const std::string& path) { return fs::path(path).filename().string(); }

}  // namespace

TEST(TextLayout, MatchesMatplotlibForEveryCorpusString) {
    int strings = 0, glyphsChecked = 0;
    for (const auto& entry : fs::directory_iterator(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/corpus")) {
        const auto trace = calculate_plot::support::parseJson(readFile(entry.path() / "trace.json"), nullptr);
        ASSERT_TRUE(trace.has_value());
        text::FontCache fonts(kFonts);
        const auto& events = trace->find("events")->items();
        const double dpi = trace->find("dpi")->asNumber();
        for (std::size_t i = 0; i + 1 < events.size(); ++i) {
            const Json& e = events[i];
            if (e.find("op")->asString() != "text" || !e.find("ismath")->isBool() || e.find("ismath")->asBool())
                continue;
            const Json& recorded = events[i + 1];
            ASSERT_EQ(recorded.find("op")->asString(), "glyphs");
            std::vector<std::string> names;
            for (const Json& n : e.find("fonts")->items()) names.push_back(n.asString());
            std::string error;
            const auto glyphs = text::layoutText(fonts, names, e.find("s")->asString(), e.find("size")->asNumber(), dpi,
                                                 *text::hintingFlags(e.find("hinting")->asString()), std::nullopt,
                                                 std::nullopt, error, trace->find("locale")->asString());
            ASSERT_TRUE(error.empty()) << error;
            const auto& want = recorded.find("glyphs")->items();
            const std::string where = entry.path().filename().string() + ": \"" + e.find("s")->asString() + "\"";
            ASSERT_EQ(glyphs.size(), want.size()) << where;
            for (std::size_t g = 0; g < glyphs.size(); ++g) {
                EXPECT_EQ(fileName(glyphs[g].font->path()), want[g].find("font")->asString()) << where << " glyph " << g;
                EXPECT_EQ(glyphs[g].index, FT_UInt(want[g].find("index")->asNumber())) << where << " glyph " << g;
                EXPECT_EQ(glyphs[g].dx, want[g].find("dx")->asNumber()) << where << " glyph " << g;
                EXPECT_EQ(glyphs[g].dy, want[g].find("dy")->asNumber()) << where << " glyph " << g;
                EXPECT_EQ(glyphs[g].size, want[g].find("size")->asNumber()) << where;
                ++glyphsChecked;
            }
            ++strings;
        }
    }
    EXPECT_GE(strings, 280);  // the corpus has 280 plain strings, 745 glyphs
    EXPECT_GE(glyphsChecked, 745);
}

TEST(TextLayout, ShapingFollowsTheLanguageNotTheUsersLocale) {
    // DejaVu Sans forms the "fi" ligature (glyph 5039) in the reference locale, not for Spanish, as
    // matplotlib does in those locales; the process locale itself never matters.
    text::FontCache fonts(kFonts);
    std::string error;
    auto shape = [&](const std::string& language) {
        return text::layoutText(fonts, {"DejaVuSans.ttf"}, "fi", 12, 100, FT_LOAD_DEFAULT, std::nullopt, std::nullopt,
                                error, language);
    };
    ASSERT_EQ(shape(text::kReferenceLocale).size(), 1u);
    EXPECT_EQ(shape(text::kReferenceLocale)[0].index, 5039u);
    EXPECT_EQ(shape("es_ES.UTF-8").size(), 2u);
    const char* previous = std::setlocale(LC_CTYPE, nullptr);
    const std::string saved = previous ? previous : "C";
    if (std::setlocale(LC_CTYPE, "es_ES.UTF-8")) {
        EXPECT_EQ(shape(text::kReferenceLocale).size(), 1u);  // the user's locale changes nothing
        std::setlocale(LC_CTYPE, saved.c_str());
    }
}

TEST(TextLayout, RoundingIsPythons) {
    EXPECT_EQ(text::pythonRound(2.5), 2);
    EXPECT_EQ(text::pythonRound(3.5), 4);
    EXPECT_EQ(text::pythonRound(-2.5), -2);
    EXPECT_EQ(text::pythonRound(-3.5), -4);
    EXPECT_EQ(text::pythonRound(2.4999999999999996), 2);
    EXPECT_EQ(text::pythonRound(0x40 * 0.0390625), 2);  // 2.5 sixty-fourths of a pixel
    EXPECT_EQ(text::pythonRound(65536 * -0.5), -32768);
}

TEST(TextLayout, HintingNamesMapToFreeTypeFlags) {
    EXPECT_EQ(*text::hintingFlags("default"), FT_LOAD_DEFAULT);
    EXPECT_EQ(*text::hintingFlags("no_autohint"), FT_LOAD_NO_AUTOHINT);
    EXPECT_EQ(*text::hintingFlags("force_autohint"), FT_LOAD_FORCE_AUTOHINT);
    EXPECT_EQ(*text::hintingFlags("no_hinting"), FT_LOAD_NO_HINTING);
    EXPECT_EQ(*text::hintingFlags("auto"), FT_LOAD_FORCE_AUTOHINT);
    EXPECT_FALSE(text::hintingFlags("sometimes").has_value());
}

TEST(TextLayout, MissingGlyphsFallBackToTheLastResortFont) {
    text::FontCache fonts(kFonts);
    std::string error;
    // U+0F00 TIBETAN SYLLABLE OM is not in DejaVu Sans.
    const auto glyphs = text::layoutText(fonts, {"DejaVuSans.ttf"}, "a\xe0\xbc\x80", 10, 100, FT_LOAD_DEFAULT,
                                         std::nullopt, std::nullopt, error);
    ASSERT_TRUE(error.empty()) << error;
    ASSERT_EQ(glyphs.size(), 2u);
    EXPECT_EQ(fileName(glyphs[0].font->path()), "DejaVuSans.ttf");
    EXPECT_EQ(fileName(glyphs[1].font->path()), "LastResortHE-Regular.ttf");
}

TEST(TextLayout, AnUnknownFontIsAnError) {
    text::FontCache fonts(kFonts);
    std::string error;
    text::layoutText(fonts, {"NoSuchFont.ttf"}, "x", 10, 100, FT_LOAD_DEFAULT, std::nullopt, std::nullopt, error);
    EXPECT_NE(error.find("NoSuchFont.ttf"), std::string::npos) << error;
}

TEST(TextLayout, Utf8DecodesEveryLength) {
    EXPECT_EQ(text::codepoints("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"), std::u32string({U'a', 0xe9, 0x20ac, 0x1f600}));
    EXPECT_EQ(text::codepoints("\xff"), std::u32string({0xfffd}));
    EXPECT_EQ(text::codepoints("\xc3"), std::u32string({0xfffd}));
}
