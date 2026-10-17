// The library's public API (include/calculate-plot/calculate-plot.hpp) against the figure corpus: what an
// application gets from a figure file.
#include "calculate-plot/calculate-plot.hpp"
#include "png/png.hpp"
#include "support/json.hpp"
#include "support/sha256.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <cmath>
#include <map>
#include <sstream>

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

calculate_plot::png::Image decoded(const std::string& file) {
    std::string error;
    const auto image = calculate_plot::png::decodePng(file, &error);
    EXPECT_TRUE(image.has_value()) << error;
    return image ? *image : calculate_plot::png::Image{};
}

const char* const kCases[] = {"corpus_legend_box", "mathtext_rules", "hatch_legend", "style_ggplot", "pyplot_subplot"};

}  // namespace

TEST(Api, APngIsTheReference) {
    for (const char* name : kCases) {
        std::string error;
        const auto png = cp::drawPng(readFile(kFigures / name / "fig.json"), kFonts, &error);
        ASSERT_EQ(error, "") << name;
        const auto mine = decoded(png), want = decoded(readFile(kFigures / name / "ref.png"));
        EXPECT_EQ(mine.width, want.width) << name;
        EXPECT_EQ(mine.height, want.height) << name;
        EXPECT_TRUE(mine.rgba == want.rgba) << name;
    }
}

TEST(Api, AnSvgAndAScriptAreTheReferences) {
    for (const char* name : kCases) {
        std::string error;
        EXPECT_EQ(cp::drawSvg(readFile(kFigures / name / "fig.json"), kFonts, &error), readFile(kFigures / name / "ref.svg")) << name;
        EXPECT_EQ(error, "") << name;
        EXPECT_EQ(cp::exportPython(readFile(kFigures / name / "fig.json"), &error), readFile(kFigures / name / "fig.py")) << name;
        EXPECT_EQ(error, "") << name;
    }
}

TEST(Api, FontsMayComeAsBytes) {
    // the application holds the font files (as resources, say): drawing then needs no directory
    std::map<std::string, std::string> files;
    for (const auto& entry : fs::directory_iterator(kFonts)) files[entry.path().filename().string()] = readFile(entry.path());
    cp::useFontBytes([&files](std::string_view name) {
        const auto it = files.find(std::string(name));
        return it == files.end() ? std::string_view() : std::string_view(it->second);
    });
    for (const char* name : kCases) {
        std::string error;
        const auto png = cp::drawPng(readFile(kFigures / name / "fig.json"), "/no/such/directory", &error);
        EXPECT_EQ(error, "") << name;
        EXPECT_TRUE(decoded(png).rgba == decoded(readFile(kFigures / name / "ref.png")).rgba) << name;
        EXPECT_EQ(cp::drawSvg(readFile(kFigures / name / "fig.json"), "", &error), readFile(kFigures / name / "ref.svg")) << name;
    }
    cp::useFontBytes(nullptr);
}

TEST(Api, TheScreenDrawsAtTheDevicePixelRatio) {
    // as matplotlib's Qt canvas: the figure's dpi times the ratio, without the savefig settings
    const std::string figure = readFile(kFigures / "corpus_legend_box" / "fig.json");
    std::string error;
    const auto one = cp::drawScreen(figure, 1.0, kFonts, &error);
    ASSERT_EQ(error, "");
    const auto want = decoded(readFile(kFigures / "corpus_legend_box" / "ref.png"));
    EXPECT_EQ(one.width, want.width);
    EXPECT_EQ(one.height, want.height);
    EXPECT_TRUE(one.rgba == want.rgba);
    const auto two = cp::drawScreen(figure, 2.0, kFonts, &error);
    ASSERT_EQ(error, "");
    EXPECT_EQ(two.width, 2 * want.width);
    EXPECT_EQ(two.height, 2 * want.height);
    EXPECT_EQ(two.rgba.size(), std::size_t(two.width) * std::size_t(two.height) * 4);
}

// As matplotlib's Qt canvas on a widget of W x H logical pixels: the dpi times the ratio, then the figure resized to
// the widget (set_size_inches(W * ratio / dpi, H * ratio / dpi)), its layout engine run again at that size.
TEST(Api, TheCanvasDrawsTheFigureAtItsSize) {
    calculate_plot::support::JsonError jsonError;
    const auto oracle = calculate_plot::support::parseJson(readFile(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/screen.json"), &jsonError);
    ASSERT_TRUE(oracle.has_value()) << jsonError.message;
    std::size_t checked = 0;
    for (const auto& c : oracle->find("cases")->items()) {
        const auto number = [&c](const char* key) { return c.find(key)->asNumber(); };
        const std::string name = c.find("case")->asString();
        const cp::Canvas canvas{number("width"), number("height"), number("ratio")};
        std::string error;
        const auto image = cp::drawCanvas(readFile(kFigures / name / "fig.json"), canvas, kFonts, &error);
        const std::string where = name + " " + std::to_string(canvas.width) + "x" + std::to_string(canvas.height) + "@" + std::to_string(canvas.devicePixelRatio);
        EXPECT_EQ(error, "") << where;
        EXPECT_EQ(image.width, int(number("buffer_width"))) << where;
        EXPECT_EQ(image.height, int(number("buffer_height"))) << where;
        const std::string bytes(image.rgba.begin(), image.rgba.end());
        EXPECT_EQ(calculate_plot::support::sha256Hex(bytes), c.find("buffer_sha256")->asString()) << where;
        ++checked;
    }
    EXPECT_EQ(checked, 60u);
}

TEST(Api, ACanvasOfNoSizeIsRefused) {
    const std::string figure = readFile(kFigures / "corpus_legend_box" / "fig.json");
    for (const cp::Canvas canvas : {cp::Canvas{0, 100, 1}, cp::Canvas{100, -1, 1}, cp::Canvas{100, 100, 0}, cp::Canvas{std::nan(""), 100, 1}}) {
        std::string error;
        EXPECT_EQ(cp::drawCanvas(figure, canvas, kFonts, &error).width, 0);
        EXPECT_NE(error, "");
    }
}

TEST(Api, WhatCannotBeDrawnSaysWhy) {
    std::string error;
    EXPECT_TRUE(cp::drawPng("{", kFonts, &error).empty());
    EXPECT_NE(error.find("not valid JSON"), std::string::npos) << error;
    EXPECT_TRUE(cp::drawPng(R"({"version": 1, "calls": [{"call": "bogus"}]})", kFonts, &error).empty());
    EXPECT_EQ(error, "unsupported call: plt.bogus");
    EXPECT_TRUE(cp::exportPython("[]", &error).empty());
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(cp::drawScreen("{", 1.0, kFonts, &error).width, 0);
}
