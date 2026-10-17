// Interactive navigation (calculate_plot::navigate) against matplotlib's NavigationToolbar2 on an emulated Qt
// canvas (tests/oracle/navigation.json, written by tools/oracle/make_navigation_oracle.py): after each pan or
// zoom, the figure drawn at the canvas' size has the same pixels as matplotlib's canvas after the gesture.
#include "calculate-plot/calculate-plot.hpp"
#include "support/json.hpp"
#include "support/sha256.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;
namespace cp = calculate_plot;
using calculate_plot::support::Json;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const fs::path kSource = fs::path(CALCULATE_PLOT_SOURCE_DIR);
const std::string kFonts = (kSource / "fonts/ttf").string();

}  // namespace

TEST(Navigation, PansAndZoomsAsMatplotlibsToolbar) {
    calculate_plot::support::JsonError jsonError;
    const auto oracle = calculate_plot::support::parseJson(readFile(kSource / "tests/oracle/navigation.json"), &jsonError);
    ASSERT_TRUE(oracle.has_value()) << jsonError.message;
    // Each gesture draws the figure twice: by default one gesture in five is checked (CALCULATE_PLOT_FULL=1: all)
    const std::size_t stride = std::getenv("CALCULATE_PLOT_FULL") ? 1 : 5;
    std::size_t checked = 0, ignored = 0, index = 0;
    for (const Json& c : oracle->find("cases")->items()) {
        if (index++ % stride != 0) continue;
        const auto number = [&c](const char* key) { return c.find(key)->asNumber(); };
        const std::string name = c.find("case")->asString();
        const std::string figure = readFile(kSource / "tests/figures" / name / "fig.json");
        const cp::Canvas canvas{number("width"), number("height"), number("ratio")};
        cp::Gesture g;
        g.tool = c.find("tool")->asString() == "pan" ? cp::Gesture::Tool::Pan : cp::Gesture::Tool::Zoom;
        g.button = int(number("button"));
        g.key = c.find("key")->isNull() ? std::string() : c.find("key")->asString();
        g.fromX = (*c.find("from"))[0].asNumber(), g.fromY = (*c.find("from"))[1].asNumber();
        g.toX = (*c.find("to"))[0].asNumber(), g.toY = (*c.find("to"))[1].asNumber();
        const std::string where = name + " @" + std::to_string(canvas.devicePixelRatio) + " axes " + std::to_string(int(number("axes"))) + " " +
                                  c.find("tool")->asString() + " button " + std::to_string(g.button) + " key '" + g.key + "' from " +
                                  std::to_string(g.fromX) + "," + std::to_string(g.fromY) + " to " + std::to_string(g.toX) + "," + std::to_string(g.toY);
        std::string error;
        const std::string after = cp::navigate(figure, canvas, g, kFonts, &error);
        ASSERT_EQ(error, "") << where;
        if (c.find("ignored")) {
            EXPECT_EQ(after, figure) << where;  // the toolbar does nothing: the same file
            ++ignored;
            continue;
        }
        const auto image = cp::drawCanvas(after, canvas, kFonts, &error);
        ASSERT_EQ(error, "") << where;
        const std::string bytes(image.rgba.begin(), image.rgba.end());
        std::string limits;  // matplotlib's view of each Axes after the gesture, to read beside ours on a failure
        for (const Json& l : c.find("limits")->items()) {
            limits += "\n  [";
            for (const Json& v : l.items()) limits += std::to_string(std::strtod(v.asString().c_str(), nullptr)) + " ";
            limits += "]";
        }
        EXPECT_EQ(calculate_plot::support::sha256Hex(bytes), c.find("sha256")->asString())
            << where << "\nours: ..." << after.substr(after.size() > 400 ? after.size() - 400 : 0) << "\nmatplotlib's limits:" << limits;
        ++checked;
    }
    EXPECT_EQ(checked + ignored, (868u + stride - 1) / stride);
    if (stride == 1) {
        EXPECT_EQ(ignored, 34u);
    }
    EXPECT_GT(ignored, 0u);
}

TEST(Navigation, WhatCannotBeNavigatedSaysWhy) {
    const std::string figure = readFile(kSource / "tests/figures/corpus_legend_box/fig.json");
    std::string error;
    cp::Gesture g;
    g.fromX = 300, g.fromY = 200, g.toX = 320, g.toY = 220;
    EXPECT_EQ(cp::navigate(figure, cp::Canvas{0, 480, 1}, g, kFonts, &error), "");
    EXPECT_NE(error, "");
    g.button = 2;  // the toolbar takes the left and right buttons only
    EXPECT_EQ(cp::navigate(figure, cp::Canvas{640, 480, 1}, g, kFonts, &error), figure);
    EXPECT_EQ(error, "");
    g.button = 1;
    g.key = "alt";  // a key the toolbar does not know is no key
    const std::string withAlt = cp::navigate(figure, cp::Canvas{640, 480, 1}, g, kFonts, &error);
    g.key.clear();
    EXPECT_EQ(withAlt, cp::navigate(figure, cp::Canvas{640, 480, 1}, g, kFonts, &error));
    // an Axes the figure file gives no name cannot be written to
    const std::string unnamed = R"({"version": 1, "calls": [{"call": "figure"}, {"call": "plot", "args": [[0, 1], [0, 1]]}]})";
    EXPECT_EQ(cp::navigate(unnamed, cp::Canvas{640, 480, 1}, g, kFonts, &error), "");
    EXPECT_EQ(error, "the Axes under the pointer has no name in the figure file");
}
