// Gate G2: from a trace of matplotlib's SVG renderer, calculate-plot writes the same bytes.
// Embedded raster images are PNGs that matplotlib writes through Pillow, whose bytes depend on
// Pillow's version and platform: those are compared as decoded pixels instead.
#include "png/png.hpp"
#include "render/svg_replay.hpp"
#include "support/base64.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
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

const std::regex kImageData("data:image/png;base64,\n([A-Za-z0-9+/=]+)");
const std::regex kImageId("image[0-9a-f]{10}");

std::vector<std::string> embeddedImages(const std::string& svg) {
    std::vector<std::string> out;
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), kImageData); it != std::sregex_iterator(); ++it)
        out.push_back((*it)[1].str());
    return out;
}

std::string withoutImageBytes(const std::string& svg) {
    return std::regex_replace(std::regex_replace(svg, kImageData, "data:image/png;base64,\n(image)"), kImageId,
                              "image(id)");
}

// The first line where two texts differ, for the failure message.
std::string firstDifference(const std::string& got, const std::string& want) {
    std::istringstream a(got), b(want);
    std::string la, lb;
    for (int line = 1;; ++line) {
        const bool ha = bool(std::getline(a, la)), hb = bool(std::getline(b, lb));
        if (!ha && !hb) return "identical lines (a trailing difference)";
        if (la != lb || ha != hb)
            return "line " + std::to_string(line) + "\n  ours:       " + la.substr(0, 300) + "\n  matplotlib: " + lb.substr(0, 300);
    }
}

class Svg : public testing::TestWithParam<std::string> {};

}  // namespace

TEST_P(Svg, WritesMatplotlibsBytes) {
    const fs::path dir = corpus() / GetParam();
    const auto result = calculate_plot::render::replaySvgTrace(readFile(dir / "svg-trace.json"),
                                                               std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    ASSERT_TRUE(result.error.empty()) << result.error;
    const std::string want = readFile(dir / "ref.svg");
    const auto images = embeddedImages(want);
    if (images.empty()) {
        if (result.svg != want) {
            std::ofstream(fs::temp_directory_path() / ("cplot-" + GetParam() + ".svg")) << result.svg;
            ADD_FAILURE() << GetParam() << ": " << firstDifference(result.svg, want);
        }
        return;
    }
    // Everything but the image bytes is identical; the images decode to the same pixels.
    EXPECT_EQ(withoutImageBytes(result.svg), withoutImageBytes(want))
        << firstDifference(withoutImageBytes(result.svg), withoutImageBytes(want));
    const auto ours = embeddedImages(result.svg);
    ASSERT_EQ(ours.size(), images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        std::string error;
        const auto a = calculate_plot::png::decodePng(*calculate_plot::support::decodeBase64(ours[i]), &error);
        const auto b = calculate_plot::png::decodePng(*calculate_plot::support::decodeBase64(images[i]), &error);
        ASSERT_TRUE(a && b) << error;
        EXPECT_EQ(a->width, b->width);
        EXPECT_EQ(a->height, b->height);
        EXPECT_TRUE(a->rgba == b->rgba) << "embedded image " << i << " differs";
    }
}

TEST_P(Svg, HashesWhatMatplotlibHashes) {
    const fs::path dir = corpus() / GetParam();
    const std::string traceText = readFile(dir / "svg-trace.json");
    const auto result = calculate_plot::render::replaySvgTrace(traceText, std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    const auto trace = calculate_plot::support::parseJson(traceText, nullptr);
    const auto& ids = trace->find("ids")->items();
    ASSERT_EQ(result.hashedContents.size(), ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto* content = ids[i].find("content");
        if (content->isNull()) continue;  // image bytes (Pillow's PNG)
        EXPECT_EQ(result.hashedContents[i], content->asString()) << "id " << i;
    }
}

std::vector<std::string> svgCases() {
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(corpus())) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

INSTANTIATE_TEST_SUITE_P(Corpus, Svg, testing::ValuesIn(svgCases()),
                         [](const testing::TestParamInfo<std::string>& param) { return param.param; });

TEST(SvgReplay, RefusesARandomHashSalt) {
    const auto result = calculate_plot::render::replaySvgTrace(
        R"({"version":1,"fonttype":"path","hashsalt":null,"metadata":{"Date":null},"svg_id":null,
            "events":[{"op":"begin","width":10,"height":10,"image_dpi":72}],"ids":[]})", ".");
    EXPECT_NE(result.error.find("hashsalt"), std::string::npos) << result.error;
}

TEST(SvgReplay, NeverWritesAClockTime) {
    // Without an explicit Date, matplotlib writes the current time; calculate-plot refuses.
    const auto result = calculate_plot::render::replaySvgTrace(
        R"({"version":1,"fonttype":"path","hashsalt":"s","metadata":{},"svg_id":null,
            "events":[{"op":"begin","width":10,"height":10,"image_dpi":72}],"ids":[]})", ".");
    EXPECT_NE(result.error.find("Date"), std::string::npos) << result.error;
}
