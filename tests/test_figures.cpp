// Figures drawn by calculate-plot's engine from their figure files: the draw calls equal the ones
// matplotlib made for the same figure (tests/figures/<case>/trace.json, recorded by
// tools/oracle/make_figures.py), field by field with doubles exact, and the pixels equal ref.png.
#include "engine/engine.hpp"
#include "model/figure_model.hpp"
#include "png/png.hpp"
#include "render/replay.hpp"
#include "render/svg_replay.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;

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

std::string describe(const Json& v) {
    switch (v.type()) {
        case Json::Type::Null: return "null";
        case Json::Type::Bool: return v.asBool() ? "true" : "false";
        case Json::Type::Number: return v.numberText();
        case Json::Type::String: return '"' + v.asString() + '"';
        case Json::Type::Array: return "array of " + std::to_string(v.size());
        case Json::Type::Object: return "object";
    }
    return {};
}

// Where two traces first differ ("events[3].gc.linewidth: 1.5 instead of 1.25"); empty when equal.
// Numbers compare as doubles, the sign of zero included (it reaches the SVG).
// `ignored` names object members left out of the comparison.
std::string firstDifference(const Json& got, const Json& want, const std::string& where,
                            const std::vector<std::string>& ignored = {}) {
    if (got.type() != want.type()) return where + ": " + describe(got) + " instead of " + describe(want);
    switch (want.type()) {
        case Json::Type::Null: return {};
        case Json::Type::Bool:
            return got.asBool() == want.asBool() ? std::string() : where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::Number:
            if (got.asNumber() == want.asNumber() && std::signbit(got.asNumber()) == std::signbit(want.asNumber())) return {};
            return where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::String:
            return got.asString() == want.asString() ? std::string() : where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::Array:
            for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
                const std::string d = firstDifference(got[i], want[i], where + "[" + std::to_string(i) + "]", ignored);
                if (!d.empty()) return d;
            }
            if (got.size() != want.size())
                return where + ": " + std::to_string(got.size()) + " elements instead of " + std::to_string(want.size());
            return {};
        case Json::Type::Object:
            for (const auto& [key, value] : want.members()) {
                if (std::find(ignored.begin(), ignored.end(), key) != ignored.end()) continue;
                const Json* mine = got.find(key);
                if (!mine) return where + "." + key + ": missing";
                const std::string d = firstDifference(*mine, value, where + "." + key, ignored);
                if (!d.empty()) return d;
            }
            for (const auto& [key, value] : got.members())
                if (!want.find(key)) return where + "." + key + ": not in matplotlib's trace";
            return {};
    }
    return {};
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

std::string drawnTrace(const std::string& name) {
    std::string error;
    const auto figure = calculate_plot::model::parseFigure(readFile(figures() / name / "fig.json"), &error);
    EXPECT_TRUE(figure.has_value()) << error;
    if (!figure) return {};
    const auto drawing = calculate_plot::engine::drawAgg(*figure);
    EXPECT_TRUE(drawing.error.empty()) << drawing.error;
    return drawing.trace;
}

class Figures : public testing::TestWithParam<std::string> {};

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
    const auto drawing = calculate_plot::engine::drawSvg(*figure);
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
