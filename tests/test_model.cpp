#include "model/figure_model.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace m = calculate_plot::model;

namespace {

m::Figure parsed(const std::string& json) {
    std::string error;
    auto f = m::parseFigure(json, &error);
    EXPECT_TRUE(f.has_value()) << error;
    return f ? *f : m::Figure{};
}

std::string refused(const std::string& json) {
    std::string error;
    EXPECT_FALSE(m::parseFigure(json, &error).has_value()) << json;
    return error;
}

}  // namespace

TEST(FigureModel, ReadsCallsInOrder) {
    const auto f = parsed(R"({"version":1,"calls":[
        {"call":"figure","kwargs":{"figsize":[3.2,2.4]},"as":"fig"},
        {"on":"fig","call":"add_subplot","as":"ax"},
        {"on":"ax","call":"plot","args":[[0,1,2],[0,1,4]],"kwargs":{"color":"C1","lw":2}}]})");
    ASSERT_EQ(f.calls.size(), 3u);
    EXPECT_EQ(f.calls[0].method, "figure");
    EXPECT_TRUE(f.calls[0].on.empty());
    EXPECT_EQ(f.calls[0].as, std::vector<std::string>{"fig"});
    EXPECT_EQ(f.calls[2].on, "ax");
    ASSERT_EQ(f.calls[2].args.size(), 2u);
    ASSERT_EQ(f.calls[2].kwargs.size(), 2u);
    EXPECT_EQ(f.calls[2].kwargs[0].first, "color");
    EXPECT_EQ(f.calls[2].kwargs[1].first, "lw");
}

TEST(FigureModel, ExportsAPlainMatplotlibScript) {
    const auto f = parsed(R"({"version":1,"rc":{"lines.linewidth":2.5},"calls":[
        {"call":"subplots","args":[2,1],"kwargs":{"sharex":true,"figsize":{"$tuple":[4,3]}},"as":["fig","axs"]},
        {"on":"axs[0]","call":"plot","args":[[0.1,0.30000000000000004,1e-05],[1,2,3]],"kwargs":{"label":"it's","ls":"--"}},
        {"on":"axs[1]","call":"set_title","args":["Título"],"kwargs":{"loc":null}}]})");
    EXPECT_EQ(m::exportPython(f, "0.1.0"),
              "# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.\n"
              "import locale\n"
              "\n"
              "import matplotlib.pyplot as plt\n"
              "\n"
              "# Text is shaped for the language of the locale: this is the one the figure was made with.\n"
              "try:\n"
              "    locale.setlocale(locale.LC_CTYPE, \"C.UTF-8\")\n"
              "except locale.Error:\n"
              "    locale.setlocale(locale.LC_CTYPE, \"C\")\n"
              "\n"
              "plt.rcParams.update({\n"
              "    'lines.linewidth': 2.5,\n"
              "})\n"
              "fig, axs = plt.subplots(2, 1, sharex=True, figsize=(4, 3))\n"
              "axs[0].plot([0.1, 0.30000000000000004, 1e-05], [1, 2, 3], label=\"it's\", ls='--')\n"
              "axs[1].set_title('T\xc3\xadtulo', loc=None)\n"
              "\n"
              "if __name__ == \"__main__\":\n"
              "    plt.show()\n");
}

TEST(FigureModel, IntegersStayIntegersAndFloatsStayExact) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"figure","args":[1, 1.0, 2.5e-300, -0.0, 123456789012345678]}]})");
    EXPECT_NE(m::exportPython(f, "x").find("plt.figure(1, 1.0, 2.5e-300, -0.0, 123456789012345678)"), std::string::npos)
        << m::exportPython(f, "x");
}

TEST(FigureModel, MinusZeroWithoutAPointIsAnInteger) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"figure","args":[-0, -0.0]}]})");
    EXPECT_NE(m::exportPython(f, "x").find("plt.figure(-0, -0.0)"), std::string::npos) << m::exportPython(f, "x");
}

TEST(FigureModel, OneElementTuplesKeepTheirComma) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"figure","args":[{"$tuple":[5]}, {"$tuple":[]}]}]})");
    EXPECT_NE(m::exportPython(f, "x").find("plt.figure((5,), ())"), std::string::npos) << m::exportPython(f, "x");
}

TEST(FigureModel, RefusesWhatIsNotAPlainCallList) {
    EXPECT_NE(refused("[]").find("object"), std::string::npos);
    EXPECT_NE(refused(R"({"version":2,"calls":[]})").find("version"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"__import__"}]})").find("__import__"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"plot;import os"}]})").find("name"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"on":"ax","call":"plot"}]})").find("ax"), std::string::npos);  // unknown object
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"figure","as":"1x"}]})").find("name"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"figure","kwargs":{"a b":1}}]})").find("name"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"figure","args":[{"$code":"x"}]}]})").find("$code"), std::string::npos);
}

TEST(FigureModel, TheCorpusScriptsAreWhatTheExporterWrites) {
    // tests/figures/*/fig.py were written by `cplot export-py` and run by matplotlib for the references.
    namespace fs = std::filesystem;
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures")) {
        auto read = [](const fs::path& p) {
            std::ifstream in(p, std::ios::binary);
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        };
        std::string error;
        const auto figure = m::parseFigure(read(entry.path() / "fig.json"), &error);
        ASSERT_TRUE(figure.has_value()) << entry.path() << ": " << error;
        EXPECT_EQ(m::exportPython(*figure, CALCULATE_PLOT_VERSION_FOR_TESTS), read(entry.path() / "fig.py")) << entry.path();
        ++checked;
    }
    EXPECT_GE(checked, 1);
}

// JSON has no NaN or infinities; {"$float": "nan" | "inf" | "-inf"} stands for them, exported as
// Python's float('nan') etc.
TEST(FigureModel, NonFiniteFloatsHaveTheirOwnForm) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"figure","args":[[1, {"$float":"nan"}, {"$float":"inf"}, {"$float":"-inf"}]]}]})");
    EXPECT_NE(m::exportPython(f, "x").find("plt.figure([1, float('nan'), float('inf'), float('-inf')])"), std::string::npos)
        << m::exportPython(f, "x");
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"figure","args":[{"$float":"1.5"}]}]})").find("$float"), std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"call":"figure","args":[{"$float":1}]}]})").find("$float"), std::string::npos);
}
