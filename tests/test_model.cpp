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

TEST(FigureModel, AStyleIsUsedBeforeTheRcParams) {
    const auto f = parsed(R"({"version":1,"style":["ggplot","dark_background"],"rc":{"lines.linewidth":2.5},
        "calls":[{"call":"figure"}]})");
    ASSERT_TRUE(f.style.isArray());
    EXPECT_NE(m::exportPython(f, "x").find("plt.style.use(['ggplot', 'dark_background'])\n"
                                           "plt.rcParams.update({\n"),
              std::string::npos)
        << m::exportPython(f, "x");
    const auto one = parsed(R"({"version":1,"style":"classic","calls":[{"call":"figure"}]})");
    EXPECT_NE(m::exportPython(one, "x").find("\nplt.style.use('classic')\nplt.figure()\n"), std::string::npos)
        << m::exportPython(one, "x");
    const auto none = parsed(R"({"version":1,"calls":[{"call":"figure"}]})");
    EXPECT_TRUE(none.style.isNull());
    EXPECT_EQ(m::exportPython(none, "x").find("style"), std::string::npos);
}

TEST(FigureModel, AStyleIsANameOrAListOfNames) {
    for (const char* bad : {"3", "null", "true", "[1]", "[[\"ggplot\"]]", "{\"lines.linewidth\": 2}"}) {
        const std::string json = std::string(R"({"version":1,"style":)") + bad + R"(,"calls":[]})";
        EXPECT_NE(refused(json).find("style"), std::string::npos) << bad;
    }
}

TEST(FigureModel, ACallMayBeOnAnAttributeOfAnObject) {
    const auto f = parsed(R"({"version":1,"calls":[
        {"call":"subplots","args":[1,2],"as":["fig","axs"]},
        {"on":"axs[1].xaxis","call":"set_major_formatter","args":[{"$new":"NullFormatter"}]}]})");
    ASSERT_EQ(f.calls.size(), 2u);
    EXPECT_EQ(f.calls[1].on, "axs[1].xaxis");
    EXPECT_NE(m::exportPython(f, "x").find("axs[1].xaxis.set_major_formatter(plt.NullFormatter())\n"), std::string::npos)
        << m::exportPython(f, "x");
    for (const char* on : {"axs.", "axs..xaxis", "axs.__class__", "axs.1", "axs[1].x axis", "xaxis", "axs[1]x"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"subplots","as":["fig","axs"]},{"on":")") +
                                 on + R"(","call":"grid"}]})";
        EXPECT_FALSE(m::parseFigure(json, nullptr).has_value()) << on;
    }
}

TEST(FigureModel, ANewObjectIsAPyplotClassWithoutArguments) {
    for (const char* bad : {R"({"$new":"os.system"})", R"({"$new":"__import__"})", R"({"$new":3})",
                            R"({"$new":"NullFormatter","x":1})", R"({"$new":""})"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"figure","args":[)") + bad + "]}]}";
        EXPECT_NE(refused(json).find("$new"), std::string::npos) << bad;
    }
}

// {"$ref": "name"} is an object an earlier call named (a patch made by plt.Polygon, say), exported as
// the bare name; only names made before the call may be used.
TEST(FigureModel, AReferenceNamesAnEarlierResult) {
    const auto f = parsed(R"({"version":1,"calls":[
        {"call":"subplots","as":["fig","ax"]},
        {"call":"Polygon","args":[[[0,0],[1,1],[1,0]]],"as":"tri"},
        {"on":"ax","call":"add_patch","args":[{"$ref":"tri"}]}]})");
    EXPECT_NE(m::exportPython(f, "x").find("tri = plt.Polygon([[0, 0], [1, 1], [1, 0]])\nax.add_patch(tri)\n"), std::string::npos)
        << m::exportPython(f, "x");
    for (const char* bad : {R"({"$ref":"later"})", R"({"$ref":"nobody"})", R"({"$ref":"__import__"})", R"({"$ref":3})",
                            R"({"$ref":"tri","x":1})", R"({"$ref":"ax.figure"})"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},)") +
                                 R"({"call":"Polygon","args":[[[0,0]]],"as":"tri"},{"on":"ax","call":"add_patch","args":[)" + bad +
                                 R"(]},{"call":"Polygon","args":[[[0,0]]],"as":"later"}]})";
        EXPECT_NE(refused(json).find("$ref"), std::string::npos) << bad;
    }
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

// {"$dict": {...}} is a Python dict with string keys (arrowprops, say), exported in its order.
TEST(FigureModel, ADictHasStringKeysInItsOrder) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},
        {"on":"ax","call":"annotate","args":["a",[0,1]],"kwargs":{"arrowprops":{"$dict":{"arrowstyle":"->","lw":2,"x":{"$dict":{}},"t":{"$tuple":[1]}}}}}]})");
    EXPECT_NE(m::exportPython(f, "x").find("ax.annotate('a', [0, 1], arrowprops={'arrowstyle': '->', 'lw': 2, 'x': {}, 't': (1,)})"),
              std::string::npos)
        << m::exportPython(f, "x");
    for (const char* bad : {R"({"$dict":[1]})", R"({"$dict":{"a":1},"b":2})", R"({"$dict":{"a":{"$code":1}}})", R"({})"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"figure","args":[)") + bad + "]}]}";
        EXPECT_FALSE(refused(json).empty()) << bad;
    }
}

// {"$lambda": "<body>"} is a function of x, `lambda x: <body>`, in the restricted form of support::PyLambda;
// numpy is imported when a body uses it.
TEST(FigureModel, AFunctionIsTheBodyOfALambda) {
    const auto f = parsed(R"j({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},
        {"on":"ax","call":"secondary_xaxis","args":["top"],"kwargs":{"functions":{"$tuple":[{"$lambda":"x * 1.8 + 32"},{"$lambda":"(x - 32) / 1.8"}]}}}]})j");
    const std::string script = m::exportPython(f, "x");
    EXPECT_NE(script.find("ax.secondary_xaxis('top', functions=(lambda x: x * 1.8 + 32, lambda x: (x - 32) / 1.8))"), std::string::npos)
        << script;
    EXPECT_EQ(script.find("import numpy"), std::string::npos);
    const auto g = parsed(R"j({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},
        {"on":"ax","call":"set_xscale","args":["function"],"kwargs":{"functions":[{"$lambda":"np.sqrt(x)"},{"$lambda":"x ** 2"}]}}]})j");
    EXPECT_NE(m::exportPython(g, "x").find("import numpy as np\n"), std::string::npos) << m::exportPython(g, "x");
    EXPECT_NE(m::exportPython(g, "x").find("functions=[lambda x: np.sqrt(x), lambda x: x ** 2]"), std::string::npos);
    for (const char* bad : {R"j({"$lambda":"y"})j", R"j({"$lambda":1})j", R"j({"$lambda":"x","b":2})j",
                            R"j({"$lambda":"np.tanh(x)"})j", R"j({"$lambda":"x\n+1"})j"}) {
        const std::string json = std::string(R"j({"version":1,"calls":[{"call":"figure","args":[)j") + bad + "]}]}";
        EXPECT_NE(refused(json).find("$lambda"), std::string::npos) << bad;
    }
}

// matplotlib.colors' classes are called on "colors" (norms, colormaps): `from matplotlib import colors` is imported
// when a call is on it.
TEST(FigureModel, ColorsObjectsAreMadeOnTheColorsModule) {
    const auto f = parsed(R"j({"version":1,"calls":[{"on":"colors","call":"LogNorm","kwargs":{"vmin":0.1,"vmax":10},"as":"norm"},
        {"call":"subplots","as":["fig","ax"]},{"on":"ax","call":"imshow","args":[[[1,2],[3,4]]],"kwargs":{"norm":{"$ref":"norm"}}}]})j");
    const std::string script = m::exportPython(f, "x");
    EXPECT_NE(script.find("from matplotlib import colors\n"), std::string::npos) << script;
    EXPECT_NE(script.find("norm = colors.LogNorm(vmin=0.1, vmax=10)\n"), std::string::npos) << script;
    EXPECT_NE(script.find("ax.imshow([[1, 2], [3, 4]], norm=norm)\n"), std::string::npos) << script;
    EXPECT_EQ(m::exportPython(parsed(R"({"version":1,"calls":[{"call":"figure"}]})"), "x").find("from matplotlib import"),
              std::string::npos);
    EXPECT_NE(refused(R"({"version":1,"calls":[{"on":"colors","call":"LogNorm","as":"colors"}]})").find("colors"), std::string::npos);
}

// {"$array": {"dtype": ..., "data": [...]}} is an array of a NumPy dtype other than the one np.asarray picks;
// numpy is imported for it.
TEST(FigureModel, AnArrayMayHaveADtype) {
    const auto f = parsed(R"j({"version":1,"calls":[{"call":"subplots","as":["fig","ax"]},
        {"on":"ax","call":"imshow","args":[{"$array":{"dtype":"uint8","data":[[[1,2,3],[4,5,6]]]}}]}]})j");
    const std::string script = m::exportPython(f, "x");
    EXPECT_NE(script.find("import numpy as np\n"), std::string::npos) << script;
    EXPECT_NE(script.find("ax.imshow(np.array([[[1, 2, 3], [4, 5, 6]]], dtype=np.uint8))"), std::string::npos) << script;
    for (const char* bad : {R"({"$array":{"dtype":"complex128","data":[1]}})", R"({"$array":{"dtype":"uint8"}})",
                            R"({"$array":{"dtype":"uint8","data":3}})", R"({"$array":{"dtype":"uint8","data":[1],"x":1}})",
                            R"({"$array":[1]})"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"figure","args":[)") + bad + "]}]}";
        EXPECT_NE(refused(json).find("$array"), std::string::npos) << bad;
    }
}

// A reference may name an element of an earlier result, as "on" does (colorbar(ax=axs[0])).
TEST(FigureModel, AReferenceMayIndexAResult) {
    const auto f = parsed(R"({"version":1,"calls":[{"call":"subplots","args":[1,2],"as":["fig","axs"]},
        {"call":"Polygon","args":[[[0,0]]],"as":"im"},
        {"on":"fig","call":"colorbar","args":[{"$ref":"im"}],"kwargs":{"ax":{"$ref":"axs[1]"}}}]})");
    EXPECT_NE(m::exportPython(f, "x").find("fig.colorbar(im, ax=axs[1])"), std::string::npos) << m::exportPython(f, "x");
    for (const char* bad : {R"({"$ref":"axs[1].figure"})", R"({"$ref":"bxs[1]"})", R"({"$ref":"axs[1"})"}) {
        const std::string json = std::string(R"({"version":1,"calls":[{"call":"subplots","args":[1,2],"as":["fig","axs"]},)") +
                                 R"({"on":"fig","call":"colorbar","kwargs":{"ax":)" + bad + R"(}}]})";
        EXPECT_NE(refused(json).find("$ref"), std::string::npos) << bad;
    }
}
