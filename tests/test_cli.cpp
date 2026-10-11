#include "cli.hpp"

#include "calculate-plot/calculate-plot.hpp"
#include "png/png.hpp"

#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Outcome {
    int status;
    std::string out;
    std::string err;
};

Outcome cplot(std::vector<std::string> args) {
    std::ostringstream out, err;
    const int status = calculate_plot::cli::run(args, out, err);
    return {status, out.str(), err.str()};
}

}  // namespace

TEST(Cli, Version) {
    const Outcome r = cplot({"--version"});
    EXPECT_EQ(r.status, 0);
    EXPECT_EQ(r.out, "cplot " + std::string(calculate_plot::versionString()) + "\n");
    EXPECT_EQ(r.err, "");
}

TEST(Cli, LicensesPrintsEveryNoticeVerbatim) {
    const Outcome r = cplot({"--licenses"});
    EXPECT_EQ(r.status, 0);
    std::string expected;
    for (const auto& item : calculate_plot::licenceTexts())
        expected += "==> " + std::string(item.path) + " <==\n" + std::string(item.text) + "\n";
    EXPECT_EQ(r.out, expected);
}

TEST(Cli, HelpMentionsTheLicences) {
    const Outcome r = cplot({"--help"});
    EXPECT_EQ(r.status, 0);
    EXPECT_NE(r.out.find("--licenses"), std::string::npos);
    EXPECT_NE(r.out.find("--version"), std::string::npos);
}

TEST(Cli, NoArgumentsShowsUsageAndFails) {
    const Outcome r = cplot({});
    EXPECT_EQ(r.status, 2);
    EXPECT_NE(r.err.find("usage"), std::string::npos);
}

TEST(Cli, AnUnknownOptionFails) {
    const Outcome r = cplot({"--frobnicate"});
    EXPECT_EQ(r.status, 2);
    EXPECT_NE(r.err.find("--frobnicate"), std::string::npos);
    EXPECT_EQ(r.out, "");
}

namespace {

std::string corpusFile(const std::string& name) {
    return std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/corpus/" + name;
}

std::string fileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string tempFile(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("cplot-cli-" + std::to_string(std::random_device{}()) + name)).string();
}

const std::string kFontDir = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

}  // namespace

TEST(Cli, ReplayWritesMatplotlibsPixels) {
    const std::string out = tempFile(".png");
    const Outcome r = cplot({"replay", corpusFile("line_basic/trace.json"), out, "--fonts", kFontDir});
    ASSERT_EQ(r.status, 0) << r.err;
    std::string error;
    const auto got = calculate_plot::png::decodePng(fileText(out), &error);
    const auto want = calculate_plot::png::decodePng(fileText(corpusFile("line_basic/ref.png")), &error);
    ASSERT_TRUE(got && want) << error;
    EXPECT_TRUE(got->rgba == want->rgba);
    std::filesystem::remove(out);
}

TEST(Cli, ReplaySvgWritesMatplotlibsBytes) {
    const std::string out = tempFile(".svg");
    const Outcome r = cplot({"replay-svg", corpusFile("text_rotations/svg-trace.json"), out, "--fonts", kFontDir});
    ASSERT_EQ(r.status, 0) << r.err;
    EXPECT_EQ(fileText(out), fileText(corpusFile("text_rotations/ref.svg")));
    std::filesystem::remove(out);
}

TEST(Cli, ReplayReportsAMissingFile) {
    const Outcome r = cplot({"replay", "/no/such/trace.json", tempFile(".png"), "--fonts", kFontDir});
    EXPECT_EQ(r.status, 1);
    EXPECT_NE(r.err.find("/no/such/trace.json"), std::string::npos);
}

TEST(Cli, ReplayNeedsItsArguments) {
    EXPECT_EQ(cplot({"replay"}).status, 2);
    EXPECT_EQ(cplot({"replay-svg", "a.json"}).status, 2);
}

TEST(Cli, ExportPyWritesTheScript) {
    const std::string in = tempFile(".json"), out = tempFile(".py");
    std::ofstream(in) << R"({"version":1,"calls":[{"call":"figure","as":"fig"}]})";
    const Outcome r = cplot({"export-py", in, out});
    ASSERT_EQ(r.status, 0) << r.err;
    const std::string script = fileText(out);
    EXPECT_NE(script.find("fig = plt.figure()\n"), std::string::npos) << script;
    std::filesystem::remove(in);
    std::filesystem::remove(out);
}

TEST(Cli, ExportPyRefusesABadFigure) {
    const std::string in = tempFile(".json");
    std::ofstream(in) << R"({"version":1,"calls":[{"call":"__import__"}]})";
    const Outcome r = cplot({"export-py", in, tempFile(".py")});
    EXPECT_EQ(r.status, 1);
    EXPECT_NE(r.err.find("__import__"), std::string::npos) << r.err;
    std::filesystem::remove(in);
}

namespace {

std::string figureFile(const std::string& name) {
    return std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/figures/" + name;
}

}  // namespace

// cplot draw FIGURE.json OUT --fonts DIR: the figure as Figure.savefig would write it; the kind of
// file follows OUT's ending.
TEST(Cli, DrawWritesMatplotlibsPixels) {
    const std::string out = tempFile(".png");
    const Outcome r = cplot({"draw", figureFile("savefig_tight_pad/fig.json"), out, "--fonts", kFontDir});
    ASSERT_EQ(r.status, 0) << r.err;
    std::string error;
    const auto got = calculate_plot::png::decodePng(fileText(out), &error);
    const auto want = calculate_plot::png::decodePng(fileText(figureFile("savefig_tight_pad/ref.png")), &error);
    ASSERT_TRUE(got && want) << error;
    EXPECT_EQ(got->width, want->width);
    EXPECT_EQ(got->height, want->height);
    EXPECT_TRUE(got->rgba == want->rgba);
    std::filesystem::remove(out);
}

TEST(Cli, DrawWritesMatplotlibsSvgBytes) {
    const std::string out = tempFile(".svg");
    const Outcome r = cplot({"draw", figureFile("legend_basic/fig.json"), out, "--fonts", kFontDir});
    ASSERT_EQ(r.status, 0) << r.err;
    EXPECT_EQ(fileText(out), fileText(figureFile("legend_basic/ref.svg")));
    std::filesystem::remove(out);
}

TEST(Cli, DrawWritesTheDrawCalls) {
    const std::string out = tempFile(".json");
    const Outcome r = cplot({"draw", figureFile("plot_line/fig.json"), out, "--fonts", kFontDir});
    ASSERT_EQ(r.status, 0) << r.err;
    const auto got = calculate_plot::support::parseJson(fileText(out), nullptr);
    const auto want = calculate_plot::support::parseJson(fileText(figureFile("plot_line/trace.json")), nullptr);
    ASSERT_TRUE(got && want);
    EXPECT_EQ(calculate_plot::testing_support::firstDifference(*got, *want, "trace"), "");
    std::filesystem::remove(out);
}

TEST(Cli, DrawRefusesAnotherKindOfFile) {
    const Outcome r = cplot({"draw", figureFile("plot_line/fig.json"), tempFile(".pdf"), "--fonts", kFontDir});
    EXPECT_EQ(r.status, 2);
    EXPECT_NE(r.err.find(".png, .svg or .json"), std::string::npos) << r.err;
}

TEST(Cli, DrawSaysWhatItCannotDraw) {
    const std::string in = tempFile(".json");
    std::ofstream(in) << R"({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},{"on":"ax","call":"pie","args":[[1,2]]}]})";
    for (const char* ending : {".png", ".svg", ".json"}) {
        const std::string out = tempFile(ending);
        const Outcome r = cplot({"draw", in, out, "--fonts", kFontDir});
        EXPECT_EQ(r.status, 1);
        EXPECT_NE(r.err.find("ax.pie"), std::string::npos) << r.err;
        EXPECT_FALSE(std::filesystem::exists(out)) << "nothing is written for a figure that cannot be drawn";
    }
    std::filesystem::remove(in);
}

// A figure of no pixels (a tiny dpi) cannot be a PNG, as in matplotlib ("cannot write empty image as
// PNG"); the SVG and the draw calls are written all the same.
TEST(Cli, DrawRefusesAPngWithoutPixels) {
    const std::string in = tempFile(".json");
    std::ofstream(in) << R"({"version":1,"rc":{"figure.dpi":0.001},"calls":[{"call":"subplots","as":["fig","ax"]}]})";
    const std::string png = tempFile(".png");
    const Outcome r = cplot({"draw", in, png, "--fonts", kFontDir});
    EXPECT_EQ(r.status, 1);
    EXPECT_NE(r.err.find("the figure has no pixels: it cannot be written as a PNG"), std::string::npos) << r.err;
    EXPECT_FALSE(std::filesystem::exists(png));
    for (const char* ending : {".svg", ".json"}) {
        const std::string out = tempFile(ending);
        const Outcome ok = cplot({"draw", in, out, "--fonts", kFontDir});
        EXPECT_EQ(ok.status, 0) << ok.err;
        std::filesystem::remove(out);
    }
    std::filesystem::remove(in);
}

TEST(Cli, DrawReportsABadFigureFile) {
    const std::string in = tempFile(".json");
    std::ofstream(in) << R"({"version":1,"calls":[{"call":"__import__"}]})";
    const Outcome r = cplot({"draw", in, tempFile(".png"), "--fonts", kFontDir});
    EXPECT_EQ(r.status, 1);
    EXPECT_NE(r.err.find("__import__"), std::string::npos) << r.err;
    std::filesystem::remove(in);
    const Outcome missing = cplot({"draw", "/no/such/fig.json", tempFile(".png"), "--fonts", kFontDir});
    EXPECT_EQ(missing.status, 1);
    EXPECT_NE(missing.err.find("/no/such/fig.json"), std::string::npos);
}

TEST(Cli, DrawNeedsItsArguments) {
    EXPECT_EQ(cplot({"draw"}).status, 2);
    EXPECT_EQ(cplot({"draw", "fig.json", "out.png"}).status, 2);
    EXPECT_EQ(cplot({"draw", "fig.json", "out.png", "--font", kFontDir}).status, 2);
    EXPECT_NE(cplot({"--help"}).out.find("draw"), std::string::npos);
    EXPECT_NE(cplot({}).err.find("cplot draw FIGURE.json"), std::string::npos);
}
