// The library's public API (include/calculate-plot/calculate-plot.hpp) against the figure corpus: what an
// application gets from a figure file.
#include "calculate-plot/calculate-plot.hpp"
#include "png/png.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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
