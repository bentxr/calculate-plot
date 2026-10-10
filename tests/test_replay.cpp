// Gate G1: replaying a trace recorded from matplotlib through calculate-plot's renderer gives
// exactly matplotlib's pixels, for every figure of the corpus.
#include "png/png.hpp"
#include "render/replay.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

fs::path corpus() { return fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/corpus"; }

// Describes how two images differ; empty when they are identical.
std::string difference(const calculate_plot::png::Image& got, const calculate_plot::png::Image& want) {
    if (got.width != want.width || got.height != want.height)
        return "size " + std::to_string(got.width) + "x" + std::to_string(got.height) + " instead of " +
               std::to_string(want.width) + "x" + std::to_string(want.height);
    long differing = 0, first = -1, maxDelta = 0;
    for (std::size_t i = 0; i < got.rgba.size(); i += 4) {
        long delta = 0;
        for (int c = 0; c < 4; ++c) delta = std::max(delta, std::labs(long(got.rgba[i + c]) - long(want.rgba[i + c])));
        if (delta) {
            ++differing;
            maxDelta = std::max(maxDelta, delta);
            if (first < 0) first = long(i / 4);
        }
    }
    if (!differing) return {};
    return std::to_string(differing) + " pixels differ (largest channel difference " + std::to_string(maxDelta) +
           "), first at x=" + std::to_string(first % want.width) + " y=" + std::to_string(first / want.width);
}

class Replay : public testing::TestWithParam<std::string> {};

}  // namespace

TEST_P(Replay, GivesMatplotlibsPixels) {
    const fs::path dir = corpus() / GetParam();
    const auto result = calculate_plot::render::replayTrace(readFile(dir / "trace.json"));
    ASSERT_TRUE(result.error.empty()) << result.error;
    std::string error;
    const auto want = calculate_plot::png::decodePng(readFile(dir / "ref.png"), &error);
    ASSERT_TRUE(want.has_value()) << error;
    const std::string diff = difference(result.image, *want);
    if (!diff.empty()) {
        const fs::path out = fs::temp_directory_path() / ("cplot-replay-" + GetParam() + ".png");
        std::ofstream(out, std::ios::binary) << calculate_plot::png::encodePng(result.image, {});
        ADD_FAILURE() << GetParam() << ": " << diff << " (our image: " << out.string() << ")";
    }
}

std::vector<std::string> corpusCases() {
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(corpus())) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

INSTANTIATE_TEST_SUITE_P(Corpus, Replay, testing::ValuesIn(corpusCases()),
                         [](const testing::TestParamInfo<std::string>& param) { return param.param; });

TEST(ReplayErrors, RefusesAnUnknownOperation) {
    const auto result = calculate_plot::render::replayTrace(
        R"({"version":1,"width":2,"height":2,"dpi":100.0,"events":[{"op":"explode"}]})");
    EXPECT_NE(result.error.find("explode"), std::string::npos) << result.error;
}

TEST(ReplayErrors, RefusesANewerTraceVersion) {
    const auto result = calculate_plot::render::replayTrace(R"({"version":2,"width":2,"height":2,"dpi":100.0,"events":[]})");
    EXPECT_FALSE(result.error.empty());
}

TEST(ReplayErrors, ReportsMalformedJson) {
    EXPECT_FALSE(calculate_plot::render::replayTrace("{").error.empty());
}

TEST(ReplayErrors, AnEmptyTraceIsATransparentCanvas) {
    const auto result = calculate_plot::render::replayTrace(R"({"version":1,"width":3,"height":2,"dpi":72.0,"events":[]})");
    ASSERT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(result.image.width, 3);
    EXPECT_EQ(result.image.height, 2);
    for (std::size_t i = 0; i < result.image.rgba.size(); i += 4) {
        EXPECT_EQ(result.image.rgba[i], 255);  // matplotlib fills with (1, 1, 1, 0)
        EXPECT_EQ(result.image.rgba[i + 3], 0);
    }
}

TEST(ReplayErrors, EveryMissingFieldIsReportedNeverCrashes) {
    // Each event of a varied trace, replayed alone with one field removed at a time (also inside
    // the graphics context): the replay must report an error or draw, never crash.
    for (const char* name : {"markers_all", "contour_filled", "image_resampled", "gouraud_triangles",
                             "pcolormesh_grid", "clipped_by_patch", "text_rotations", "bars_hatched"}) {
        const auto trace = calculate_plot::support::parseJson(readFile(corpus() / name / "trace.json"), nullptr);
        ASSERT_TRUE(trace.has_value());
        const auto& events = trace->find("events")->items();
        int checked = 0;
        for (const auto& event : events) {
            const std::string op = event.find("op")->asString();
            if (op == "clear" || op == "text" || op == "glyphs") continue;  // annotations are checked by the text tests
            const std::string compact = event.find("op")->asString();
            for (const auto& [key, value] : event.members()) {
                (void)value;
                // Rebuild the event without `key` by editing its JSON text.
                std::string json = "{";
                bool first = true;
                for (const auto& [k, v] : event.members()) {
                    if (k == key) continue;
                    json += (first ? "\"" : ",\"") + k + "\":" + calculate_plot::render::toJson(v);
                    first = false;
                }
                json += "}";
                const auto result = calculate_plot::render::replayTrace(
                    R"({"version":1,"width":20,"height":20,"dpi":100.0,"events":[)" + json + "]}");
                EXPECT_FALSE(result.error.empty()) << name << ": " << compact << " without " << key;
                ++checked;
            }
            if (checked > 60) break;
        }
        EXPECT_GT(checked, 5) << name;
    }
}
