// Drawings on several threads at once: an application draws from more than one thread (a worker for its
// canvas, its interface thread for small pictures), so each drawing must give the same image as alone.
#include "calculate-plot/calculate-plot.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace cp = calculate_plot;

namespace {

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const fs::path kFigures = fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures";
const std::string kFonts = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

// A line with markers, a legend and a title in mathtext, in every bundled style; and figures that ask for many
// marker shapes, fonts and mathtext symbols.
std::vector<std::string> figures() {
    const std::string base =
        R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
        R"( {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [1, 3, 2, 4]], "kwargs": {"marker": "o", "label": "a"}},)"
        R"( {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [2, 1, 3, 0]], "kwargs": {"marker": "^", "label": "b"}},)"
        R"( {"on": "ax", "call": "set_title", "args": ["$\\alpha^2 + \\sum_i x_i$"]},)"
        R"( {"on": "ax", "call": "legend"}]})";
    std::vector<std::string> out;
    std::string error;
    out.push_back(base);
    for (const std::string& style : cp::styleNames()) {
        out.push_back(cp::setStyle(base, "\"" + style + "\"", &error));
        EXPECT_EQ(error, "") << style;
    }
    for (const char* name : {"corpus_markers_all", "corpus_mathtext_labels", "corpus_legend_box", "corpus_text_rotations"})
        out.push_back(readFile(kFigures / name / "fig.json"));
    return out;
}

// What an application asks of a figure, on whichever thread: the canvas (a small one, as for a thumbnail), the
// PNG and SVG files, the check of its calls and the pointer's coordinates.
struct Outputs {
    std::vector<std::uint8_t> canvas;
    std::string png, svg, check, errors;
    std::vector<std::string> pointer;
    bool operator==(const Outputs& o) const {
        return canvas == o.canvas && png == o.png && svg == o.svg && check == o.check && errors == o.errors && pointer == o.pointer;
    }
};

Outputs outputs(const std::string& figure) {
    Outputs out;
    std::string error;
    const cp::Canvas canvas{640, 480, 0.25};
    out.canvas = cp::drawCanvas(figure, canvas, kFonts, &error).rgba;
    out.errors += error;
    out.png = cp::drawPng(figure, kFonts, &error);
    out.errors += error;
    out.svg = cp::drawSvg(figure, kFonts, &error);
    out.errors += error;
    out.check = cp::checkFigure(figure, kFonts);
    out.pointer = cp::pointerTexts(figure, canvas, {{320, 240}, {100, 400}}, kFonts, &error);
    out.errors += error;
    return out;
}

#endif

}  // namespace

TEST(Threads, DrawingsOnSeveralThreadsAtOnceEqualDrawingsAlone) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    GTEST_SKIP() << "no threads in this build";
#else
    const std::vector<std::string> files = figures();
    // the threads draw first, so they also meet whatever is made on first use (caches, fonts); each starts at
    // another figure, so different figures (and styles) are drawn at the same time
    const int threads = 8;
    std::vector<std::vector<Outputs>> got(threads, std::vector<Outputs>(files.size()));
    std::vector<std::string> schemas(threads);
    std::vector<std::thread> running;
    for (int t = 0; t < threads; ++t)
        running.emplace_back([&files, &got, &schemas, t, threads] {
            for (std::size_t k = 0; k < files.size(); ++k) {
                const std::size_t i = (k + std::size_t(t) * files.size() / std::size_t(threads)) % files.size();
                got[std::size_t(t)][i] = outputs(files[i]);
                if (k == files.size() / 2) schemas[std::size_t(t)] = cp::schema(kFonts);
            }
        });
    for (std::thread& thread : running) thread.join();
    for (std::size_t i = 0; i < files.size(); ++i) {
        const Outputs alone = outputs(files[i]);
        ASSERT_EQ(alone.errors, "") << "figure " << i;
        ASSERT_FALSE(alone.canvas.empty() || alone.png.empty() || alone.svg.empty()) << "figure " << i;
        for (int t = 0; t < threads; ++t) EXPECT_TRUE(got[std::size_t(t)][i] == alone) << "thread " << t << ", figure " << i;
    }
    const std::string schema = cp::schema(kFonts);
    ASSERT_FALSE(schema.empty());
    for (int t = 0; t < threads; ++t) EXPECT_EQ(schemas[std::size_t(t)], schema) << "thread " << t;
#endif
}
