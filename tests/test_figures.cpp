// Figures drawn by calculate-plot's engine from their figure files: the draw calls equal the ones
// matplotlib made for the same figure (tests/figures/<case>/trace.json, recorded by
// tools/oracle/make_figures.py), field by field with doubles exact, and the pixels equal ref.png.
#include "engine/engine.hpp"
#include "model/figure_model.hpp"
#include "png/png.hpp"
#include "render/replay.hpp"
#include "render/svg_replay.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using calculate_plot::testing_support::firstDifference;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

fs::path figures() { return fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures"; }

std::vector<std::string> cases() {
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(figures()))
        if (fs::exists(entry.path() / "trace.json")) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string pixelDifference(const calculate_plot::png::Image& got, const calculate_plot::png::Image& want) {
    if (got.width != want.width || got.height != want.height)
        return "size " + std::to_string(got.width) + "x" + std::to_string(got.height) + " instead of " +
               std::to_string(want.width) + "x" + std::to_string(want.height);
    long differing = 0;
    for (std::size_t i = 0; i < got.rgba.size(); i += 4)
        if (!std::equal(got.rgba.begin() + i, got.rgba.begin() + i + 4, want.rgba.begin() + i)) ++differing;
    return differing ? std::to_string(differing) + " pixels differ" : std::string();
}

calculate_plot::engine::DrawOptions drawOptions() {
    calculate_plot::engine::DrawOptions options;
    options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
    return options;
}

std::string drawnTrace(const std::string& name) {
    std::string error;
    const auto figure = calculate_plot::model::parseFigure(readFile(figures() / name / "fig.json"), &error);
    EXPECT_TRUE(figure.has_value()) << error;
    if (!figure) return {};
    const auto drawing = calculate_plot::engine::drawAgg(*figure, drawOptions());
    EXPECT_TRUE(drawing.error.empty()) << drawing.error;
    return drawing.trace;
}

// A case whose figure the engine cannot draw yet carries a PENDING file saying why (its references
// already serve the renderers' tests); the figure tests skip it.
class Figures : public testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        const fs::path pending = figures() / GetParam() / "PENDING";
        if (fs::exists(pending)) GTEST_SKIP() << readFile(pending);
    }
};

TEST_P(Figures, DrawCallsEqualMatplotlibs) {
    const std::string mine = drawnTrace(GetParam());
    const auto got = calculate_plot::support::parseJson(mine, nullptr);
    ASSERT_TRUE(got.has_value()) << mine;
    const auto want = calculate_plot::support::parseJson(readFile(figures() / GetParam() / "trace.json"), nullptr);
    ASSERT_TRUE(want.has_value());
    EXPECT_EQ(firstDifference(*got, *want, "trace"), "");
}

TEST_P(Figures, PixelsEqualMatplotlibs) {
    calculate_plot::render::ReplayOptions options;
    options.text = calculate_plot::render::TextMode::Glyphs;
    options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
    const auto result = calculate_plot::render::replayTrace(drawnTrace(GetParam()), options);
    ASSERT_TRUE(result.error.empty()) << result.error;
    std::string error;
    const auto want = calculate_plot::png::decodePng(readFile(figures() / GetParam() / "ref.png"), &error);
    ASSERT_TRUE(want.has_value()) << error;
    EXPECT_EQ(pixelDifference(result.image, *want), "");
}

std::string drawnSvgTrace(const std::string& name) {
    std::string error;
    const auto figure = calculate_plot::model::parseFigure(readFile(figures() / name / "fig.json"), &error);
    EXPECT_TRUE(figure.has_value()) << error;
    if (!figure) return {};
    const auto drawing = calculate_plot::engine::drawSvg(*figure, drawOptions());
    EXPECT_TRUE(drawing.error.empty()) << drawing.error;
    return drawing.trace;
}

// The SVG renderer's calls; "ids" (what matplotlib hashed into element ids) is the SVG renderer's
// own output, compared through the SVG bytes.
TEST_P(Figures, SvgDrawCallsEqualMatplotlibs) {
    const std::string mine = drawnSvgTrace(GetParam());
    const auto got = calculate_plot::support::parseJson(mine, nullptr);
    ASSERT_TRUE(got.has_value()) << mine;
    const auto want = calculate_plot::support::parseJson(readFile(figures() / GetParam() / "svg-trace.json"), nullptr);
    ASSERT_TRUE(want.has_value());
    EXPECT_EQ(firstDifference(*got, *want, "svg-trace", {"ids"}), "");
}

TEST_P(Figures, SvgBytesEqualMatplotlibs) {
    const auto result = calculate_plot::render::replaySvgTrace(drawnSvgTrace(GetParam()),
                                                               std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    ASSERT_TRUE(result.error.empty()) << result.error;
    EXPECT_TRUE(result.svg == readFile(figures() / GetParam() / "ref.svg")) << result.svg;
}

INSTANTIATE_TEST_SUITE_P(Engine, Figures, testing::ValuesIn(cases()),
                         [](const testing::TestParamInfo<std::string>& param) { return param.param; });

// The comparison itself can fail: a trace that differs in one field is reported at that field.
TEST(FigureComparison, NamesTheFirstDifferingField) {
    const auto a = calculate_plot::support::parseJson(R"({"events":[{"gc":{"linewidth":1.0}}]})", nullptr);
    const auto b = calculate_plot::support::parseJson(R"({"events":[{"gc":{"linewidth":1.25}}]})", nullptr);
    const auto z = calculate_plot::support::parseJson(R"({"events":[{"gc":{"linewidth":-0.0}}]})", nullptr);
    const auto y = calculate_plot::support::parseJson(R"({"events":[{"gc":{"linewidth":0}}]})", nullptr);
    EXPECT_EQ(firstDifference(*a, *a, "trace"), "");
    EXPECT_EQ(firstDifference(*a, *b, "trace"), "trace.events[0].gc.linewidth: 1.0 instead of 1.25");
    EXPECT_EQ(firstDifference(*z, *y, "trace"), "trace.events[0].gc.linewidth: -0.0 instead of 0");
    EXPECT_EQ(firstDifference(*y, *z, "trace"), "trace.events[0].gc.linewidth: 0 instead of -0.0");
}

}  // namespace
