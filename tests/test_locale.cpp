// The application's locale: an application may set one whose numbers have a decimal comma (a Qt application
// does, from the environment), but figure files, style sheets, SVG and Python are written with a point, as
// Python reads and writes them. Every call of the public API must give the same result under such a locale.
#include "calculate-plot/calculate-plot.hpp"

#include <gtest/gtest.h>

#include <clocale>
#include <filesystem>
#include <fstream>
#include <langinfo.h>
#include <locale.h>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace cp = calculate_plot;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const fs::path kFigures = fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures";
const std::string kFonts = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

// An installed locale whose decimal point is a comma (the C library of the browser build has none).
locale_t commaLocale() {
    for (const char* name : {"es_ES.UTF-8", "es_ES.utf8", "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8"}) {
        locale_t l = newlocale(LC_ALL_MASK, name, locale_t(0));
        if (l && std::string(nl_langinfo_l(RADIXCHAR, l)) == ",") return l;
        if (l) freelocale(l);
    }
    return locale_t(0);
}

// The answers of every call that reads or writes numbers, for one figure.
std::vector<std::string> answers(const std::string& figure) {
    std::vector<std::string> out;
    std::string error;
    out.push_back(cp::drawPng(figure, kFonts, &error) + error);
    out.push_back(cp::drawSvg(figure, kFonts, &error) + error);
    out.push_back(cp::exportPython(figure, &error) + error);
    const cp::Image screen = cp::drawScreen(figure, 1.5, kFonts, &error);
    out.push_back(std::string(screen.rgba.begin(), screen.rgba.end()) + error);
    const cp::Canvas canvas{400, 300, 1};
    const cp::Image drawn = cp::drawCanvas(figure, canvas, kFonts, &error);
    out.push_back(std::string(drawn.rgba.begin(), drawn.rgba.end()) + error);
    for (const std::string& text : cp::pointerTexts(figure, canvas, {{200, 150}, {120, 200}}, kFonts, &error)) out.push_back(text);
    out.push_back(error);
    cp::Gesture zoom;
    zoom.tool = cp::Gesture::Tool::Zoom;
    zoom.fromX = 120, zoom.fromY = 100, zoom.toX = 260, zoom.toY = 220;
    out.push_back(cp::navigate(figure, canvas, zoom, kFonts, &error) + error);
    out.push_back(cp::setKeyword(figure, 1, "linewidth", "2.75", {"lw"}, &error) + error);
    out.push_back(cp::setStyle(figure, R"("classic")", &error) + error);
    out.push_back(cp::checkFigure(figure, kFonts));
    for (const std::string& name : cp::axesNames(figure, kFonts, &error)) out.push_back(name);
    return out;
}

// A line, in the styles whose sheets hold fractions (the subplot margins, line widths) and a corpus figure.
std::vector<std::string> figures() {
    const std::string base =
        R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
        R"( {"on": "ax", "call": "plot", "args": [[0, 0.5, 1.25, 3], [1.5, 3.25, 2, 4.75]], "kwargs": {"marker": "o", "lw": 1.5}},)"
        R"( {"on": "ax", "call": "set_title", "args": ["$\\alpha = 0.5$"]}]})";
    std::vector<std::string> out{base};
    for (const char* style : {"classic", "_mpl-gallery", "fivethirtyeight", "ggplot", "seaborn-v0_8-paper"}) {
        std::string error;
        out.push_back(cp::setStyle(base, std::string("\"") + style + "\"", &error));
        EXPECT_EQ(error, "") << style;
    }
    out.push_back(readFile(kFigures / "corpus_log_axes" / "fig.json"));
    return out;
}

}  // namespace

TEST(Locale, AnswersUnderALocaleWithADecimalCommaEqualAnswersUnderC) {
    const locale_t comma = commaLocale();
    if (!comma) GTEST_SKIP() << "no locale with a decimal comma here";
    const std::vector<std::string> files = figures();
    std::vector<std::vector<std::string>> inC;
    for (const std::string& file : files) inC.push_back(answers(file));
    const std::string schemaInC = cp::schema(kFonts);
    const locale_t before = uselocale(comma);
    std::vector<std::vector<std::string>> inComma;
    for (const std::string& file : files) inComma.push_back(answers(file));
    const std::string schemaInComma = cp::schema(kFonts);
    const std::string separator = localeconv()->decimal_point;  // the caller's locale is still its own
    uselocale(before);
    freelocale(comma);
    EXPECT_EQ(separator, ",");
    for (std::size_t i = 0; i < files.size(); ++i) {
        ASSERT_EQ(inComma[i].size(), inC[i].size()) << "figure " << i;
        for (std::size_t k = 0; k < inC[i].size(); ++k) EXPECT_TRUE(inComma[i][k] == inC[i][k]) << "figure " << i << ", answer " << k;
    }
    EXPECT_FALSE(schemaInC.empty());
    EXPECT_TRUE(schemaInComma == schemaInC);
}
