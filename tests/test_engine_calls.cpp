// What the engine answers to calls it cannot run: an error that names the call and says why, never
// a figure drawn differently from matplotlib's.
#include "engine/engine.hpp"
#include "render/svg_replay.hpp"
#include "model/figure_model.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

calculate_plot::engine::AggDrawing draw(const std::string& calls) {
    std::string error;
    const auto figure = calculate_plot::model::parseFigure(R"({"version": 1, "calls": [)" + calls + "]}", &error);
    EXPECT_TRUE(figure.has_value()) << error;
    if (!figure) return {};
    calculate_plot::engine::DrawOptions options;
    options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
    return calculate_plot::engine::drawAgg(*figure, options);
}

const std::string kSubplots = R"({"call": "subplots", "args": [1, 2], "as": ["fig", "axs"]},)";

struct Refusal {
    const char* call;
    const char* error;
};

class LayoutRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(LayoutRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, LayoutRefusals,
    testing::Values(
        Refusal{R"({"on": "fig", "call": "tight_layout", "args": [1.0]})", "fig.tight_layout: the arguments are keywords"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"padding": 1.0}})",
                "fig.tight_layout: unsupported keyword padding"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"pad": "big"}})", "fig.tight_layout: pad must be a number"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"pad": null}})", "fig.tight_layout: pad must be a number"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"h_pad": "big"}})",
                "fig.tight_layout: h_pad must be a number"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"w_pad": [1]}})", "fig.tight_layout: w_pad must be a number"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"rect": 0.5}})",
                "fig.tight_layout: rect must be (left, bottom, right, top)"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"rect": [0, 0, 1]}})",
                "fig.tight_layout: rect must be (left, bottom, right, top)"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"rect": [0, 0, 1, 1, 1]}})",
                "fig.tight_layout: rect must be (left, bottom, right, top)"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"rect": [0, 0, 1, "top"]}})",
                "fig.tight_layout: rect must be (left, bottom, right, top)"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "args": [0.1, 0.1, 0.9, 0.9, 0.2, 0.2, 0.2]})",
                "fig.subplots_adjust: too many positional arguments"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"middle": 0.5}})",
                "fig.subplots_adjust: unsupported keyword middle"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"left": "wide"}})",
                "fig.subplots_adjust: left must be a number"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "args": [0.1, 0.1, 0.9, 0.9, 0.2, "tall"]})",
                "fig.subplots_adjust: hspace must be a number"},
        // matplotlib raises ValueError for these
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"left": 0.9}})",
                "fig.subplots_adjust: left cannot be >= right"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"bottom": 0.9, "top": 0.5}})",
                "fig.subplots_adjust: bottom cannot be >= top"},
        // what the measuring cannot do yet stops the layout
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"pad": 2}},
                   {"on": "fig", "call": "pie"})",
                "unsupported call: fig.pie"}));

// What the measuring cannot do yet stops the layout, and says so by the call's name.
class TextRefusals : public testing::TestWithParam<Refusal> {};

// Axes.text refuses what matplotlib's Text refuses, and what a figure file cannot say yet.
TEST_P(TextRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, TextRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "text", "args": ["a", 2, "t"], "kwargs": {}})",
                "axs[0].text: x and y must be numbers"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, [2], "t"], "kwargs": {}})",
                "axs[0].text: x and y must be numbers"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1], "kwargs": {}})",
                "axs[0].text: x and y must be numbers"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [], "kwargs": {}})",
                "axs[0].text: x and y must be numbers"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2], "kwargs": {}})",
                "axs[0].text: the text must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, 3], "kwargs": {}})",
                "axs[0].text: the text must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t", null, 5], "kwargs": {}})",
                "axs[0].text: too many positional arguments"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"x": 3}})",
                "axs[0].text: x is given twice"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"y": 3}})",
                "axs[0].text: y is given twice"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"s": "u"}})",
                "axs[0].text: s is given twice"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t", null], "kwargs": {"fontdict": null}})",
                "axs[0].text: fontdict is given twice"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontdict": 3}})",
                "axs[0].text: fontdict is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t", "bold"], "kwargs": {}})",
                "axs[0].text: fontdict is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"language": 5}})",
                "axs[0].text: language must be a string or None"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"language": true}})",
                "axs[0].text: language must be a string or None"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"language": [{"$tuple": ["en", 0, 1]}]}})",
                "axs[0].text: language as a list of ranges is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"language": {"$tuple": []}}})",
                "axs[0].text: language as a list of ranges is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"colour": "red"}})",
                "axs[0].text: unsupported keyword colour"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"bbox": null}})",
                "axs[0].text: unsupported keyword bbox"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"transform": null}})",
                "axs[0].text: unsupported keyword transform"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontproperties": null}})",
                "axs[0].text: unsupported keyword fontproperties"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"color": 3}})",
                "axs[0].text: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"color": "no such colour"}})",
                "axs[0].text: color is not a colour: Invalid RGBA argument: no such colour"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"c": null}})",
                "axs[0].text: c is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"c": [1, 2]}})",
                "axs[0].text: c is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"ha": "middle"}})",
                "axs[0].text: ha is not an alignment: middle"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"horizontalalignment": "top"}})",
                "axs[0].text: horizontalalignment is not an alignment: top"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"horizontalalignment": null}})",
                "axs[0].text: horizontalalignment must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"va": "left"}})",
                "axs[0].text: va is not an alignment: left"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"verticalalignment": 3}})",
                "axs[0].text: verticalalignment must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"ma": "baseline"}})",
                "axs[0].text: ma is not an alignment: baseline"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"ma": null}})",
                "axs[0].text: ma must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"multialignment": "bogus"}})",
                "axs[0].text: multialignment is not an alignment: bogus"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"multialignment": 3}})",
                "axs[0].text: multialignment must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"rotation": "diagonal"}})",
                "axs[0].text: rotation must be 'vertical', 'horizontal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"rotation": [30]}})",
                "axs[0].text: rotation must be 'vertical', 'horizontal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"linespacing": "wide"}})",
                "axs[0].text: linespacing must be 'normal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"linespacing": true}})",
                "axs[0].text: linespacing must be 'normal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"rotation_mode": "free"}})",
                "axs[0].text: rotation_mode is not a rotation mode: free"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"rotation_mode": 1}})",
                "axs[0].text: rotation_mode must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"usetex": true}})",
                "axs[0].text: usetex is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"wrap": true}})",
                "axs[0].text: wrap is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"transform_rotates_text": true}})",
                "axs[0].text: transform_rotates_text is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"parse_math": "yes"}})",
                "axs[0].text: parse_math must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"antialiased": 1}})",
                "axs[0].text: antialiased must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"alpha": "half"}})",
                "axs[0].text: alpha must be a number or None"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"alpha": 1.5}})",
                "axs[0].text: alpha is outside the 0-1 range"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"alpha": -0.1}})",
                "axs[0].text: alpha is outside the 0-1 range"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"alpha": {"$float": "nan"}}})",
                "axs[0].text: alpha is outside the 0-1 range"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": "huge"}})",
                "axs[0].text: fontsize is not a font size: huge"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": "12pt"}})",
                "axs[0].text: size is not a font size: 12pt"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": "1e"}})",
                "axs[0].text: size is not a font size: 1e"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": ""}})",
                "axs[0].text: size is not a font size: "},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": "."}})",
                "axs[0].text: size is not a font size: ."},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": "- 3"}})",
                "axs[0].text: size is not a font size: - 3"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": "e5"}})",
                "axs[0].text: size is not a font size: e5"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": [12]}})",
                "axs[0].text: fontsize must be a number or the name of a size"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": {"$float": "nan"}}})",
                "axs[0].text: fontsize is not a font size"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": {"$float": "inf"}}})",
                "axs[0].text: fontsize is not a font size"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": "inf"}})",
                "axs[0].text: fontsize is not a font size"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": "-nan"}})",
                "axs[0].text: fontsize is not a font size"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"math_fontfamily": "arial"}})",
                "axs[0].text: math_fontfamily is not a math font family"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"math_fontfamily": 3}})",
                "axs[0].text: math_fontfamily is not a math font family"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontweight": "heavyish"}})",
                "axs[0].text: fontweight is invalid: heavyish"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": "7.5"}})",
                "axs[0].text: weight is invalid: 7.5"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": "1e2"}})",
                "axs[0].text: weight is invalid: 1e2"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": 1001}})",
                "axs[0].text: weight is invalid"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": -1}})",
                "axs[0].text: weight is invalid"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": {"$float": "inf"}}})",
                "axs[0].text: weight is invalid"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": [400]}})",
                "axs[0].text: weight must be a name or a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontstretch": "wide"}})",
                "axs[0].text: fontstretch is invalid: wide"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"stretch": 1001.5}})",
                "axs[0].text: stretch is invalid"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"stretch": 2000}})",
                "axs[0].text: stretch is invalid"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"stretch": "bold"}})",
                "axs[0].text: stretch is invalid: bold"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": "condensed"}})",
                "axs[0].text: weight is invalid: condensed"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontstyle": "slanted"}})",
                "axs[0].text: fontstyle is not one of the choices: slanted"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"style": 1}})",
                "axs[0].text: style must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontvariant": "caps"}})",
                "axs[0].text: fontvariant is not one of the choices: caps"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"variant": "italic"}})",
                "axs[0].text: variant is not one of the choices: italic"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"style": "small-caps"}})",
                "axs[0].text: style is not one of the choices: small-caps"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontfamily": 3}})",
                "axs[0].text: fontfamily must be a name or a list of names"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"family": ["serif", 3]}})",
                "axs[0].text: family must be a name or a list of names"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"name": true}})",
                "axs[0].text: name must be a name or a list of names"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"zorder": "top"}})",
                "axs[0].text: zorder must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"clip_on": 1}})",
                "axs[0].text: clip_on must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"clip_on": null}})",
                "axs[0].text: clip_on must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"visible": "no"}})",
                "axs[0].text: visible must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"snap": 1}})",
                "axs[0].text: snap must be True, False or None"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontsize": 10, "size": 12}})",
                "axs[0].text: size is given twice (it is an alias of fontsize)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": 10, "fontsize": 12}})",
                "axs[0].text: fontsize is given twice (it is an alias of fontsize)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"weight": "bold", "fontweight": "bold"}})",
                "axs[0].text: fontweight is given twice (it is an alias of fontweight)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"style": "italic", "fontstyle": "italic"}})",
                "axs[0].text: fontstyle is given twice (it is an alias of fontstyle)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontfamily": "serif", "family": "serif"}})",
                "axs[0].text: family is given twice (it is an alias of fontfamily)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontname": "serif", "name": "serif"}})",
                "axs[0].text: name is given twice (it is an alias of fontname)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"variant": "normal", "fontvariant": "normal"}})",
                "axs[0].text: fontvariant is given twice (it is an alias of fontvariant)"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"stretch": 500, "fontstretch": 500}})",
                "axs[0].text: fontstretch is given twice (it is an alias of fontstretch)"}));

class TextKeywordRefusals : public testing::TestWithParam<Refusal> {};

// Titles, axis labels, the suptitle and figure texts take the keywords of a text, each in its own way.
TEST_P(TextKeywordRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, TextKeywordRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"fontdict": 3}})",
                "axs[0].set_title: fontdict is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"colour": "red"}})",
                "axs[0].set_title: unsupported keyword colour"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"fontsize": "huge"}})",
                "axs[0].set_title: fontsize is not a font size: huge"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"x": "left"}})",
                "axs[0].set_title: x must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"color": null}})",
                "axs[0].set_title: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"multialignment": "bogus"}})",
                "axs[0].set_title: multialignment is not an alignment: bogus"},
        Refusal{R"({"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"linespacing": null}})",
                "axs[0].set_title: linespacing must be 'normal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"loc": "left", "x": 0.2}})",
                "axs[0].set_xlabel: loc cannot be given together with x, horizontalalignment or ha"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"loc": "left", "ha": "right"}})",
                "axs[0].set_xlabel: loc cannot be given together with x, horizontalalignment or ha"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": ["t"], "kwargs": {"loc": "top", "y": 0.2}})",
                "axs[0].set_ylabel: loc cannot be given together with y, horizontalalignment or ha"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": ["t"], "kwargs": {"loc": "top", "horizontalalignment": "left"}})",
                "axs[0].set_ylabel: loc cannot be given together with y, horizontalalignment or ha"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"size": 8, "fontsize": 9}})",
                "axs[0].set_xlabel: fontsize is given twice (it is an alias of fontsize)"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"color": "red", "c": "blue"}})",
                "axs[0].set_xlabel: c is given twice (it is an alias of color)"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": ["t"], "kwargs": {"ha": "left", "horizontalalignment": "left"}})",
                "axs[0].set_ylabel: horizontalalignment is given twice (it is an alias of horizontalalignment)"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"loc": "top"}})",
                "axs[0].set_xlabel: bad loc top"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": ["t"], "kwargs": {"loc": "left"}})",
                "axs[0].set_ylabel: bad loc left"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"loc": 3}})",
                "axs[0].set_xlabel: bad loc"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"labelpad": "big"}})",
                "axs[0].set_xlabel: labelpad must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"fontdict": {"$tuple": [1]}}})",
                "axs[0].set_xlabel: fontdict is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"y": "low"}})",
                "axs[0].set_xlabel: y must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"color": null}})",
                "axs[0].set_xlabel: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": ["t"], "kwargs": {"rotation": "diagonal"}})",
                "axs[0].set_ylabel: rotation must be 'vertical', 'horizontal' or a number"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylabel", "args": [3], "kwargs": {}})",
                "axs[0].set_ylabel: the label must be a string"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"size": 8, "fontsize": 9}})",
                "fig.suptitle: fontsize is given twice (it is an alias of fontsize)"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"ha": "left", "horizontalalignment": "left"}})",
                "fig.suptitle: horizontalalignment is given twice (it is an alias of horizontalalignment)"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"x": "a"}})",
                "fig.suptitle: x must be a number"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"y": [1]}})",
                "fig.suptitle: y must be a number"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"t": "u"}})",
                "fig.suptitle: t is given twice"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t", "u"], "kwargs": {}})",
                "fig.suptitle: too many positional arguments"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": [3], "kwargs": {}})",
                "fig.suptitle: the title must be a string"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"colour": "red"}})",
                "fig.suptitle: unsupported keyword colour"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"fontdict": 1}})",
                "fig.suptitle: fontdict is not supported yet"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"fontweight": "heavyish"}})",
                "fig.suptitle: fontweight is invalid: heavyish"},
        Refusal{R"({"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"rotation": "diagonal"}})",
                "fig.suptitle: rotation must be 'vertical', 'horizontal' or a number"},
        Refusal{R"({"on": "fig", "call": "text", "args": ["a", 2, "t"], "kwargs": {}})",
                "fig.text: x and y must be numbers"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, null, "t"], "kwargs": {}})",
                "fig.text: x and y must be numbers"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2], "kwargs": {}})",
                "fig.text: the text must be a string"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t", 3], "kwargs": {}})",
                "fig.text: fontdict is not supported yet"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"fontdict": 3}})",
                "fig.text: fontdict is not supported yet"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t", null, 5], "kwargs": {}})",
                "fig.text: too many positional arguments"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"s": "u"}})",
                "fig.text: s is given twice"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"x": 3}})",
                "fig.text: x is given twice"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"transform": null}})",
                "fig.text: unsupported keyword transform"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"ha": "middle"}})",
                "fig.text: ha is not an alignment: middle"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"size": 8, "fontsize": 9}})",
                "fig.text: fontsize is given twice (it is an alias of fontsize)"}));

// None is no value: the defaults stay.
TEST(TextKeywordCalls, NoneKeywordsAreTheDefaults) {
    const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": "set_xlabel", "args": ["t"], "kwargs": {"labelpad": null, "loc": null, "fontdict": null}},
        {"on": "axs[0]", "call": "set_title", "args": ["t"], "kwargs": {"fontdict": null, "rotation": null, "alpha": null}},
        {"on": "fig", "call": "suptitle", "args": ["t"], "kwargs": {"x": null, "y": null, "fontdict": null, "color": null}},
        {"on": "fig", "call": "text", "args": [0.5, 0.5, "t", null], "kwargs": {"color": null, "rotation": null}})");
    EXPECT_EQ(drawing.error, "");
}

class SpanRefusals : public testing::TestWithParam<Refusal> {};

// A span's colours are what to_rgba takes: a name or three or four parts.
TEST_P(SpanRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, SpanRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [0, 1], "kwargs": {"color": [1, 2]}})",
                "axs[0].axvspan: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [0, 1], "kwargs": {"color": 3}})",
                "axs[0].axvspan: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [0, 1], "kwargs": {"fc": "no such colour"}})",
                "axs[0].axhspan: fc is not a colour: Invalid RGBA argument: no such colour"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [0, 1], "kwargs": {"facecolor": [0.1, 0.2, 0.3, 0.4, 0.5]}})",
                "axs[0].axhspan: facecolor is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [0, 1], "kwargs": {"edgecolor": [0.1, 0.2, 7]}})",
                "axs[0].axhspan: edgecolor is not a colour: RGBA values should be within 0-1 range"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [0, 1], "kwargs": {"ec": null}})",
                "axs[0].axvspan: ec is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"facecolor": "no such colour"}})",
                "axs[0].legend: bad facecolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"edgecolor": [1, 2]}})", "axs[0].legend: bad edgecolor"}));

class LegendRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(LegendRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}},)" +
                              GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, LegendRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "legend", "args": [["a"]]})",
                "axs[0].legend: handles and labels as arguments are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": 3}})", "axs[0].legend: loc must be a location name"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"title": 3}})", "axs[0].legend: title must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fontsize": "huge"}})", "axs[0].legend: unknown font size huge"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fontsize": [8]}})", "axs[0].legend: bad fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fontsize": true}})", "axs[0].legend: bad fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fontsize": {"$float": "nan"}}})", "axs[0].legend: bad fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fontsize": "inf"}})", "axs[0].legend: bad fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"title_fontsize": "Large"}})",
                "axs[0].legend: unknown font size Large"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"title_fontsize": [8]}})", "axs[0].legend: bad title_fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"title_fontsize": {"$float": "inf"}}})",
                "axs[0].legend: bad title_fontsize"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"frameon": 1}})", "axs[0].legend: frameon must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"fancybox": "yes"}})",
                "axs[0].legend: fancybox must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"markerfirst": 0}})",
                "axs[0].legend: markerfirst must be True or False"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"ncols": 0}})", "axs[0].legend: ncols must be a positive integer"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"ncol": "2"}})", "axs[0].legend: ncols must be a positive integer"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"borderpad": "1"}})", "axs[0].legend: borderpad must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"columnspacing": [1]}})",
                "axs[0].legend: columnspacing must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": 5}})", "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": true}})", "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": "nocolor"}})", "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": ["red", "nocolor"]}})",
                "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": [0.1, 0.2]}})", "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": [0.1, 0.2, 7]}})", "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"labelcolor": ["red", ["green"]]}})",
                "axs[0].legend: Invalid labelcolor"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"shadow": true}})", "axs[0].legend: unsupported keyword shadow"}));

// What matplotlib breaks on in a legend entry for vlines or hlines: lines with no colour (the entry
// takes the first one), and white lines with labelcolor='linecolor' (their four equal numbers are
// read as four rows alike, and the first number is no colour for the text).
TEST(LegendCalls, EntriesForLinesThatMatplotlibBreaksOn) {
    const auto legend = [&](const std::string& colour, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "vlines", "args": [[1, 2], 0, 1], "kwargs": {"label": "v", "colors": )" + colour +
                    R"(}}, {"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(legend("\"none\"", "{}"), "axs[0].legend: a legend entry for lines without a colour cannot be made");
    EXPECT_EQ(legend("\"w\"", R"({"labelcolor": "linecolor"})"),
              "axs[0].legend: labelcolor: the colour of the lines of this entry cannot be read (matplotlib breaks on it)");
    EXPECT_EQ(legend("\"w\"", R"({"labelcolor": "mec"})"),
              "axs[0].legend: labelcolor: the colour of the lines of this entry cannot be read (matplotlib breaks on it)");
    EXPECT_EQ(legend("[[0.5, 0.5, 0.5, 0.5]]", R"({"labelcolor": "mfc"})"),
              "axs[0].legend: labelcolor: the colour of the lines of this entry cannot be read (matplotlib breaks on it)");
    EXPECT_EQ(legend("\"w\"", R"({"labelcolor": "red"})"), "");
    EXPECT_EQ(legend("\"w\"", "{}"), "");
    EXPECT_EQ(legend("\"k\"", R"({"labelcolor": "linecolor"})"), "");
    EXPECT_EQ(legend("[[1, 1, 1, 0.99999]]", R"({"labelcolor": "linecolor"})"),
              "axs[0].legend: labelcolor: the colour of the lines of this entry cannot be read (matplotlib breaks on it)");
    EXPECT_EQ(legend("[[1, 1, 1, 0.9999]]", R"({"labelcolor": "linecolor"})"), "");
}

// A label colour that leaves the texts as they are: 'none' in capitals and an empty list are no
// colours, and None is the rcParams' (None there: the colour of text).
TEST(LegendCalls, LabelColoursThatChangeNothing) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a", "color": "red"}},)";
    const auto legend = [&](const std::string& kwargs) {
        const auto drawing = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << kwargs;
        return drawing.trace;
    };
    const std::string plain = legend("{}");
    EXPECT_EQ(legend(R"({"labelcolor": "NONE"})"), plain);
    EXPECT_EQ(legend(R"({"labelcolor": []})"), plain);
    EXPECT_EQ(legend(R"({"labelcolor": null})"), plain);
    EXPECT_EQ(legend(R"({"labelcolor": "k"})"), plain);
    EXPECT_NE(legend(R"({"labelcolor": "none"})"), plain);
    EXPECT_NE(legend(R"({"labelcolor": "linecolor"})"), plain);
    EXPECT_EQ(legend(R"({"labelcolor": "linecolor"})"), legend(R"({"labelcolor": "red"})"));
    EXPECT_EQ(legend(R"({"labelcolor": "mfc"})"), legend(R"({"labelcolor": [1, 0, 0]})"));
    EXPECT_EQ(legend(R"({"labelcolor": "mec"})"), legend(R"({"labelcolor": {"$tuple": [true, false, false, 1.0]}})"));
}

// A legend keyword that is None is the default.
TEST(LegendCalls, NoneKeywordsAreTheDefaults) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}},)";
    const auto plain = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend"})");
    const auto none = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": null, "title": null,
        "fontsize": null, "title_fontsize": null, "frameon": null, "fancybox": null, "ncols": null, "borderpad": null,
        "facecolor": null, "markerfirst": null, "framealpha": null}})");
    ASSERT_EQ(plain.error, "");
    ASSERT_EQ(none.error, "");
    EXPECT_EQ(plain.trace, none.trace);
}

// The legend's font sizes are read as FontProperties.set_size reads them: a number also as text.
TEST(LegendCalls, FontSizesMayBeNumbersAsText) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}},)";
    const auto legend = [&](const std::string& kwargs) {
        const auto drawing = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << kwargs;
        return drawing.trace;
    };
    EXPECT_EQ(legend(R"({"title": "T", "fontsize": " 14.5 ", "title_fontsize": "7"})"),
              legend(R"({"title": "T", "fontsize": 14.5, "title_fontsize": 7})"));
    EXPECT_NE(legend(R"({"title": "T", "fontsize": 14.5, "title_fontsize": 7})"), legend(R"({"title": "T", "fontsize": 14.5})"));
    EXPECT_NE(legend(R"({"title": "T", "fontsize": 14.5})"), legend(R"({"title": "T"})"));
}

// A collection of one path is drawn as markers when the path is smaller than the figure; for a
// path with curves that the hull of its control points does not settle, the question is refused.
TEST(CollectionCalls, OneCurvedPathLargerThanTheFigureIsRefused) {
    const auto huge = draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1.0], [1.0]], "kwargs": {"s": 2000000}})");
    EXPECT_EQ(huge.error, "collections of one curved path larger than the figure are not supported yet");
    const auto large = draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1.0], [1.0]], "kwargs": {"s": 20000}})");
    EXPECT_EQ(large.error, "");
    const auto square = draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1.0], [1.0]], "kwargs": {"s": 2000000, "marker": "s"}})");
    EXPECT_EQ(square.error, "");
}

TEST(LayoutCalls, WhatCannotBeMeasuredStopsTheLayout) {
    std::string error;
    const auto figure = calculate_plot::model::parseFigure(
        R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}, {"on": "fig", "call": "tight_layout"}]})", &error);
    ASSERT_TRUE(figure.has_value()) << error;
    // (the tick labels cannot be measured without their font)
    calculate_plot::engine::DrawOptions options;
    options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/no such directory";
    EXPECT_EQ(calculate_plot::engine::drawAgg(*figure, options).error,
              "fig.tight_layout: LastResortHE-Regular.ttf: FT_New_Face failed with FreeType error 1");
}

// Settings that matplotlib takes and then breaks on when they are used stop the drawing with the reason.
TEST(RcCalls, SettingsThatBreakWhenUsed) {
    const auto errors = [](const std::string& rc, const std::string& calls) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": )" + rc + R"(, "calls": [{"call": "subplots", "as": ["fig", "ax"]})" + calls + "]}", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::pair<std::string, std::string>();
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        // (the SVG renderer's own errors come when the calls are replayed)
        const auto svg = calculate_plot::engine::drawSvg(*figure, options);
        const std::string svgError = svg.error.empty() ? calculate_plot::render::replaySvgTrace(svg.trace, options.fontDirectory).error : svg.error;
        return std::make_pair(calculate_plot::engine::drawAgg(*figure, options).error, svgError);
    };
    const auto both = [&](const std::string& rc, const std::string& calls, const std::string& error) {
        const auto got = errors(rc, calls);
        EXPECT_EQ(got.first, error) << rc;
        EXPECT_EQ(got.second, error) << rc;
    };
    // SubplotParams, when the figure is made
    both(R"({"figure.subplot.left": 0.95})", "", "plt.subplots: left cannot be >= right");
    both(R"({"figure.subplot.right": 0.125})", "", "plt.subplots: left cannot be >= right");
    both(R"({"figure.subplot.top": 0.1})", "", "plt.subplots: bottom cannot be >= top");
    both(R"({"figure.subplot.bottom": 0.88})", "", "plt.subplots: bottom cannot be >= top");
    both(R"({"figure.subplot.bottom": 0.87, "figure.subplot.left": 0.89})", "", "");
    {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": {"figure.subplot.left": 0.95}, "calls": [{"call": "figure", "as": ["fig"]}]})", &parseError);
        ASSERT_TRUE(figure.has_value()) << parseError;
        EXPECT_EQ(calculate_plot::engine::drawAgg(*figure, {}).error, "plt.figure: left cannot be >= right");
    }
    // the legend's frame
    const std::string legend = R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}}, {"on": "ax", "call": "legend")";
    both(R"({"legend.framealpha": 2})", legend + "}", "ax.legend: framealpha is outside the 0-1 range");
    both(R"({"legend.framealpha": -0.5})", legend + "}", "ax.legend: framealpha is outside the 0-1 range");
    both(R"({"legend.framealpha": 2})", legend + R"(, "kwargs": {"framealpha": 0.5}})", "");
    both("{}", legend + R"(, "kwargs": {"framealpha": 1.5}})", "ax.legend: framealpha is outside the 0-1 range");
    both(R"({"legend.framealpha": 2})", "", "");  // (no legend, no harm)
    both(R"({"legend.framealpha": 1})", legend + "}", "");
    // the edge colour of scatter's markers
    const std::string scatter = R"(, {"on": "ax", "call": "scatter", "args": [[0, 1], [0, 1]])";
    both(R"({"scatter.edgecolors": "nocolor"})", scatter + "}", "ax.scatter: rcParams[\"scatter.edgecolors\"] is not a colour");
    both(R"({"scatter.edgecolors": "t"})", scatter + "}", "ax.scatter: rcParams[\"scatter.edgecolors\"] is not a colour");
    both(R"({"scatter.edgecolors": "nocolor"})", scatter + R"(, "kwargs": {"edgecolors": "red"}})", "");
    both(R"({"scatter.edgecolors": "nocolor"})", "", "");
    both(R"({"scatter.edgecolors": "C3"})", scatter + "}", "");
    // dashes: none may be negative, and one must be positive
    const std::string dashed = R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1], "--"]})";
    both(R"({"lines.dashed_pattern": [-1, 2]})", dashed, "All values in the dash list must be non-negative");
    both(R"({"lines.dashed_pattern": [0, 0]})", dashed, "At least one value in the dash list must be positive");
    both(R"({"lines.dashed_pattern": [0, 2]})", dashed, "");
    both(R"({"lines.dashed_pattern": [-1, 2]})", R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1], "-."]})", "");
    // a patch sets its dashes when it has an edge to draw
    const std::string span = R"(, {"on": "ax", "call": "axvspan", "args": [0, 1], "kwargs": {"linestyle": "--")";
    both(R"({"lines.dashed_pattern": [-1, 2]})", span + R"(, "linewidth": 2, "edgecolor": "k"}})",
         "All values in the dash list must be non-negative");
    both(R"({"lines.dashed_pattern": [0, 0]})", span + R"(, "linewidth": 2, "edgecolor": "k"}})",
         "At least one value in the dash list must be positive");
    both(R"({"lines.dashed_pattern": [-1, 2]})", span + R"(, "linewidth": 2}})", "");  // (no edge colour: no edge)
    both(R"({"lines.dashed_pattern": [-1, 2]})", span + "}}", "");
    // a collection of several lines is drawn by the Agg renderer without a look at its dashes; the
    // SVG renderer looks
    const std::string lines = R"(, {"on": "ax", "call": "hlines", "args": [[1, 2], 0, 1], "kwargs": {"linestyles": "dashed"}})";
    const auto zero = errors(R"({"lines.linewidth": 0})", lines);
    EXPECT_EQ(zero.first, "");
    EXPECT_EQ(zero.second, "At least one value in the dash list must be positive");
    const auto negative = errors(R"({"lines.dashed_pattern": [3, -1]})", lines);
    EXPECT_EQ(negative.first, "");
    EXPECT_EQ(negative.second, "All values in the dash list must be non-negative");
}

// None for the optional keywords is the same as leaving them out.
TEST(LayoutCalls, NoneKeywordsAreTheDefaults) {
    const auto plain = draw(kSubplots + R"({"on": "fig", "call": "tight_layout"})");
    const auto none = draw(kSubplots + R"({"on": "fig", "call": "tight_layout", "kwargs": {"h_pad": null, "w_pad": null, "rect": null}})");
    const auto untouched = draw(kSubplots.substr(0, kSubplots.size() - 1));
    EXPECT_EQ(plain.error, "");
    EXPECT_EQ(none.error, "");
    EXPECT_EQ(none.trace, plain.trace);
    EXPECT_NE(plain.trace, untouched.trace);
    const auto adjusted = draw(kSubplots + R"({"on": "fig", "call": "subplots_adjust", "args": [null, null, null, null, null, null]})");
    EXPECT_EQ(adjusted.error, "");
    EXPECT_EQ(adjusted.trace, untouched.trace);
}

// plt.subplots(sharex=, sharey=): True, False or one of matplotlib's four words.
TEST(SubplotsCalls, RefuseUnknownSharing) {
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [2, 2], "kwargs": {"sharex": "diagonal"}})").error,
              "plt.subplots: sharex must be True, False, 'all', 'row', 'col' or 'none'");
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [2, 2], "kwargs": {"sharey": 1}})").error,
              "plt.subplots: sharey must be True, False, 'all', 'row', 'col' or 'none'");
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [2, 2], "kwargs": {"sharey": null}})").error,
              "plt.subplots: sharey must be True, False, 'all', 'row', 'col' or 'none'");
}

TEST(SubplotsCalls, FalseAndNoneShareNothing) {
    const std::string plot = R"(, {"on": "axs[0]", "call": "plot", "args": [[0, 5], [1, 9]]})";
    const auto plain = draw(R"({"call": "subplots", "args": [2, 1], "as": ["fig", "axs"]})" + plot);
    const auto no = draw(R"({"call": "subplots", "args": [2, 1], "kwargs": {"sharex": false, "sharey": "none"}, "as": ["fig", "axs"]})" + plot);
    const auto all = draw(R"({"call": "subplots", "args": [2, 1], "kwargs": {"sharex": true}, "as": ["fig", "axs"]})" + plot);
    const auto word = draw(R"({"call": "subplots", "args": [2, 1], "kwargs": {"sharex": "all"}, "as": ["fig", "axs"]})" + plot);
    EXPECT_EQ(plain.error, "");
    EXPECT_EQ(no.error, "");
    EXPECT_EQ(all.error, "");
    EXPECT_EQ(no.trace, plain.trace);
    EXPECT_NE(all.trace, plain.trace);
    EXPECT_EQ(word.trace, all.trace);
}

// rcParams["savefig.*"] that matplotlib would not take (or could not save with) stop the drawing.
TEST(SaveOptions, RefuseWhatCannotBeSaved) {
    const auto error = [](const std::string& rc) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": )" + rc + R"(, "calls": [{"call": "subplots", "as": ["fig", "ax"]}]})", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string();
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const std::string agg = calculate_plot::engine::drawAgg(*figure, options).error;
        EXPECT_EQ(calculate_plot::engine::drawSvg(*figure, options).error, agg);
        return agg;
    };
    EXPECT_EQ(error(R"({"savefig.dpi": 0})"), "rc savefig.dpi must be positive");
    EXPECT_EQ(error(R"({"savefig.dpi": -72})"), "rc savefig.dpi must be positive");
    EXPECT_EQ(error(R"({"savefig.dpi": {"$float": "nan"}})"),
              "rc savefig.dpi: this value of savefig.dpi is not supported yet");
    EXPECT_EQ(error(R"({"savefig.dpi": "screen"})"),
              "rc savefig.dpi: the value given for savefig.dpi is not valid: the value is not the string \"figure\" and could "
              "not be converted to float");
    EXPECT_EQ(error(R"({"savefig.bbox": "loose"})"),
              "rc savefig.bbox: the value given for savefig.bbox is not valid: bbox should be 'tight' or 'standard'");
    EXPECT_EQ(error(R"({"savefig.bbox": 1})"),
              "rc savefig.bbox: the value given for savefig.bbox is not valid: bbox should be 'tight' or 'standard'");
    EXPECT_EQ(error(R"({"savefig.facecolor": "not a colour"})"),
              "rc savefig.facecolor: the value given for savefig.facecolor is not valid: the value does not look like a color arg");
    EXPECT_EQ(error(R"({"savefig.edgecolor": "not a colour"})"),
              "rc savefig.edgecolor: the value given for savefig.edgecolor is not valid: the value does not look like a color arg");
    EXPECT_EQ(error(R"({"savefig.bbox": null, "savefig.dpi": 50.5, "savefig.facecolor": "auto", "savefig.edgecolor": "C1"})"), "");
    // the other spellings matplotlib takes
    EXPECT_EQ(error(R"({"savefig.bbox": "Standard", "savefig.dpi": "150", "savefig.facecolor": "Auto", "savefig.edgecolor": "ff0000"})"),
              "rc savefig.facecolor: the value given for savefig.facecolor is not valid: the value does not look like a color arg");
    EXPECT_EQ(error(R"({"savefig.bbox": "Standard", "savefig.dpi": "150", "savefig.facecolor": "auto", "savefig.edgecolor": "ff0000"})"), "");
    EXPECT_EQ(error(R"({"savefig.bbox": "TIGHT", "savefig.dpi": "Figure"})"),
              "rc savefig.dpi: the value given for savefig.dpi is not valid: the value is not the string \"figure\" and could "
              "not be converted to float");
}

}  // namespace
