// The PDF writer against matplotlib's: replaying a trace recorded from matplotlib's PDF renderer
// (tests/figures/<case>/pdf-trace.json, by tools/oracle/make_figures.py) writes the same bytes as
// matplotlib wrote (ref.pdf), for every case listed in tests/pdf_cases.txt.
#include "render/pdf_replay.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path figures() { return fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures"; }

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

std::vector<std::string> pdfCases() {
    std::istringstream list(readFile(figures().parent_path() / "pdf_cases.txt"));
    std::vector<std::string> names;
    for (std::string name; list >> name;) names.push_back(name);
    return names;
}

class PdfReplay : public testing::TestWithParam<std::string> {};

TEST_P(PdfReplay, BytesEqualMatplotlibs) {
    const auto result = calculate_plot::render::replayPdfTrace(readFile(figures() / GetParam() / "pdf-trace.json"),
                                                               std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf");
    ASSERT_EQ(result.error, "");
    const std::string want = readFile(figures() / GetParam() / "ref.pdf");
    EXPECT_EQ(result.pdf.size(), want.size());
    EXPECT_TRUE(result.pdf == want);
}

INSTANTIATE_TEST_SUITE_P(Figures, PdfReplay, testing::ValuesIn(pdfCases()),
                         [](const testing::TestParamInfo<std::string>& p) { return p.param; });

TEST(PdfReplay, EveryListedCaseHasItsReferences) {
    for (const auto& name : pdfCases()) {
        EXPECT_TRUE(fs::exists(figures() / name / "pdf-trace.json")) << name;
        EXPECT_TRUE(fs::exists(figures() / name / "ref.pdf")) << name;
    }
}

}  // namespace
