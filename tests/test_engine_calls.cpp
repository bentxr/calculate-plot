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
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"left": {"$float": "nan"}}})",
                "fig.subplots_adjust: left must be finite"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "args": [0.1, 0.1, {"$float": "inf"}]})",
                "fig.subplots_adjust: right must be finite"},
        Refusal{R"({"on": "fig", "call": "subplots_adjust", "kwargs": {"wspace": {"$float": "-inf"}}})",
                "fig.subplots_adjust: wspace must be finite"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"pad": {"$float": "nan"}}})", "fig.tight_layout: pad must be finite"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"h_pad": {"$float": "inf"}}})",
                "fig.tight_layout: h_pad must be finite"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"w_pad": {"$float": "-inf"}}})",
                "fig.tight_layout: w_pad must be finite"},
        Refusal{R"({"on": "fig", "call": "tight_layout", "kwargs": {"rect": [0, 0, {"$float": "nan"}, 1]}})",
                "fig.tight_layout: rect must be (left, bottom, right, top)"},
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
                "axs[0].text: got multiple values for argument 'x'"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"y": 3}})",
                "axs[0].text: got multiple values for argument 'y'"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"s": "u"}})",
                "axs[0].text: got multiple values for argument 's'"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t", null], "kwargs": {"fontdict": null}})",
                "axs[0].text: got multiple values for argument 'fontdict'"},
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
                "fig.suptitle: got multiple values for argument 't'"},
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
                "fig.text: got multiple values for argument 's'"},
        Refusal{R"({"on": "fig", "call": "text", "args": [1, 2, "t"], "kwargs": {"x": 3}})",
                "fig.text: got multiple values for argument 'x'"},
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
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": 11}})",
                "axs[0].legend: loc must be string, coordinate tuple, or an integer 0-10"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": -1}})",
                "axs[0].legend: loc must be string, coordinate tuple, or an integer 0-10"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": 3.0}})",
                "axs[0].legend: loc must be string, coordinate tuple, or an integer 0-10"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": [0.5]}})",
                "axs[0].legend: loc must be string, coordinate tuple, or an integer 0-10"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"loc": [0.5, "top"]}})",
                "axs[0].legend: loc must be string, coordinate tuple, or an integer 0-10"},
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
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"ncol": 0}})", "axs[0].legend: ncol must be 1 or more"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"ncol": "2"}})", "axs[0].legend: ncol must be a number"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"alignment": null}})", "axs[0].legend: unsupported keyword alignment"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"bogus": null}})", "axs[0].legend: unsupported keyword bogus"},
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
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"shadow": 2}})", "axs[0].legend: Legend shadow must be a dict or bool"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"shadow": "yes"}})", "axs[0].legend: Legend shadow must be a dict or bool"}));

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
        "facecolor": null, "framealpha": null, "edgecolor": null, "labelcolor": null, "labelspacing": null, "handlelength": null,
        "handletextpad": null, "borderaxespad": null, "columnspacing": null}})");
    ASSERT_EQ(plain.error, "");
    ASSERT_EQ(none.error, "");
    EXPECT_EQ(plain.trace, none.trace);
}

// The number of columns is ncols, or ncol when ncols is 1 (as it is, whatever was given, with fewer
// than two entries). numpy.array_split reads it: truncated to an integer that must be 1 or more;
// more columns than entries leave the rest empty.
TEST(LegendCalls, TheColumnCountIsReadAsMatplotlibReadsIt) {
    const auto legend = [](int lines, const std::string& kwargs) {
        std::string calls = kSubplots;
        for (int i = 0; i < lines; ++i)
            calls += R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, )" + std::to_string(i + 1) +
                     R"(]], "kwargs": {"label": "line )" + std::to_string(i) + R"("}},)";
        return draw(calls + R"({"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}");
    };
    const auto trace = [&](int lines, const std::string& kwargs) {
        const auto drawing = legend(lines, kwargs);
        EXPECT_EQ(drawing.error, "") << lines << " " << kwargs;
        return drawing.trace;
    };
    // one entry: ncols is not read
    const std::string one = trace(1, "{}");
    for (const char* ncols : {"0", "null", "\"2\"", "7", "false", R"({"$float": "nan"})", "1180591620717411303424"})
        EXPECT_EQ(trace(1, std::string(R"({"ncols": )") + ncols + "}"), one) << ncols;
    EXPECT_EQ(trace(1, R"({"ncol": 5})"), one);
    EXPECT_EQ(trace(1, R"({"ncols": 3, "ncol": true})"), one);
    EXPECT_EQ(legend(1, R"({"ncol": null})").error, "axs[0].legend: ncol must be a number");
    EXPECT_EQ(legend(1, R"({"ncols": 3, "ncol": 0.5})").error, "axs[0].legend: ncol must be 1 or more");
    EXPECT_EQ(legend(1, R"({"ncol": false})").error, "axs[0].legend: ncol must be 1 or more");
    // no entry at all
    EXPECT_EQ(trace(0, R"({"ncols": 3})"), trace(0, "{}"));
    EXPECT_EQ(trace(0, R"({"ncol": 3})"), trace(0, "{}"));
    // three entries
    const std::string plain = trace(3, "{}"), two = trace(3, R"({"ncols": 2})"), three = trace(3, R"({"ncols": 3})");
    EXPECT_NE(two, plain);
    EXPECT_NE(three, two);
    EXPECT_NE(three, plain);
    EXPECT_EQ(trace(3, R"({"ncols": 2.9})"), two);
    EXPECT_EQ(trace(3, R"({"ncols": 1, "ncol": 2})"), two);
    EXPECT_EQ(trace(3, R"({"ncol": 2, "ncols": 1.0})"), two);
    EXPECT_EQ(trace(3, R"({"ncols": true, "ncol": 3})"), three);
    EXPECT_EQ(trace(3, R"({"ncols": 2, "ncol": 0})"), two);
    EXPECT_EQ(trace(3, R"({"ncols": 2, "ncol": "x"})"), two);
    EXPECT_EQ(trace(3, R"({"ncol": true})"), plain);
    EXPECT_EQ(trace(3, R"({"ncols": 7})"), three);
    EXPECT_EQ(trace(3, R"({"ncols": 2147483647})"), three);
    EXPECT_EQ(trace(3, R"({"ncols": 2147483647.9})"), three);
    for (const char* ncols : {"0", "-3", "0.5", "false", "0.999"})
        EXPECT_EQ(legend(3, std::string(R"({"ncols": )") + ncols + "}").error, "axs[0].legend: ncols must be 1 or more") << ncols;
    for (const char* ncols : {"null", "\"2\"", "[2]"})
        EXPECT_EQ(legend(3, std::string(R"({"ncols": )") + ncols + "}").error, "axs[0].legend: ncols must be a number") << ncols;
    for (const char* ncols : {R"({"$float": "nan"})", R"({"$float": "inf"})", R"({"$float": "-inf"})"})
        EXPECT_EQ(legend(3, std::string(R"({"ncols": )") + ncols + "}").error, "axs[0].legend: ncols must be finite") << ncols;
    for (const char* ncols : {"2147483648", "1e300", "1180591620717411303424"})
        EXPECT_EQ(legend(3, std::string(R"({"ncols": )") + ncols + "}").error,
                  "axs[0].legend: more than 2147483647 columns are not supported yet")
            << ncols;
    EXPECT_EQ(legend(3, R"({"ncols": 1, "ncol": 0})").error, "axs[0].legend: ncol must be 1 or more");
    EXPECT_EQ(legend(3, R"({"ncols": 1, "ncol": null})").error, "axs[0].legend: ncol must be a number");
}

// None is the default of the legend's keywords that this engine does not take yet, too.
TEST(LegendCalls, NoneForAKeywordNotTakenYetIsItsDefault) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}},)";
    const auto plain = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend"})");
    ASSERT_EQ(plain.error, "");
    for (const char* name : {"handles", "labels", "reverse", "scatteryoffsets", "prop",
                             "handleheight", "mode", "linewidth", "bbox_to_anchor", "bbox_transform", "handler_map",
                             "title_fontproperties", "draggable"}) {
        const auto none = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": {")" + name + R"(": null}})");
        EXPECT_EQ(none.error, "") << name;
        EXPECT_EQ(none.trace, plain.trace) << name;
        EXPECT_EQ(draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": {")" + name + R"(": 1}})").error,
                  std::string("axs[0].legend: unsupported keyword ") + name);
    }
}

// numpoints, scatterpoints and markerscale: the keyword, or else the rcParams; the counts are integers.
TEST(LegendCalls, PointCountsAreIntegers) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a", "marker": "o"}},)";
    const auto legend = [&](const std::string& kwargs) {
        return draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(legend(R"({"numpoints": 3, "scatterpoints": 2, "markerscale": 1.5})"), "");
    EXPECT_EQ(legend(R"({"numpoints": 2.5})"), "axs[0].legend: numpoints must be an integer");
    EXPECT_EQ(legend(R"({"scatterpoints": "2"})"), "axs[0].legend: scatterpoints must be an integer");
    EXPECT_EQ(legend(R"({"numpoints": 0})"), "axs[0].legend: numpoints must be > 0; it was 0");
    EXPECT_EQ(legend(R"({"scatterpoints": 0})"), "axs[0].legend: scatterpoints below 1 are not supported yet");
    EXPECT_EQ(legend(R"({"markerscale": "big"})"), "axs[0].legend: markerscale must be a number");
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
    // the marker every line starts with (matplotlib breaks when the first line without one is made:
    // here no figure is drawn with such a setting)
    both(R"({"lines.marker": "Auto"})", "", "rc lines.marker: marker 'Auto' is not supported yet");
    both(R"({"lines.marker": "both"})", R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1]]})",
         "rc lines.marker: marker 'both' is not supported yet");
    both(R"({"lines.marker": "s", "markers.fillstyle": "none"})", R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1]]})", "");
    both(R"({"lines.marker": "", "markers.fillstyle": "left"})", R"(, {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1]]})", "");
    // the colours of the grid: a colour with its alpha is not read yet
    both(R"({"grid.major.color": {"$tuple": ["C1", 0.5]}})", "", "rc grid.major.color: this value of grid.major.color is not supported yet");
    both(R"({"grid.minor.color": [0.1, 0.2, 0.3]})", "", "rc grid.minor.color: this value of grid.minor.color is not supported yet");
    both(R"({"grid.minor.color": "C1", "grid.major.color": "#ff000080"})", "", "");
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

// An Axes of no width has a transform that cannot be inverted, and matplotlib stops with numpy's
// "Singular matrix" where the inverse is first needed: not for an axhline on fresh limits (its
// transform then equals the Axes' own and no inverse is taken), but after a plot has changed them.
// A property cycle gives a new line every property it has: one that a line does not have, or a
// value a line does not take, breaks plot and errorbar. The fills and scatter take its colour only.
TEST(RcCalls, APropertyCycleWithWhatALineDoesNotTakeStopsPlot) {
    const auto error = [](const std::string& cycle, const std::string& call) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": {"axes.prop_cycle": ")" + cycle + R"("}, "calls": [{"call": "subplots", "as": ["fig", "ax"]}, {"on": "ax", "call": ")" +
                call + R"(", "args": [[1, 2], [1, 2]]}]})",
            &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string("?");
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        return calculate_plot::engine::drawAgg(*figure, options).error;
    };
    EXPECT_EQ(error("cycler(fc='rg')", "plot"), "ax.plot: unsupported keyword facecolor");
    EXPECT_EQ(error("cycler(ec='rg')", "errorbar"), "ax.errorbar: unsupported keyword edgecolor");
    EXPECT_EQ(error("cycler(fc='rg')", "scatter"), "");
    EXPECT_EQ(error("cycler(fc='rg')", "fill_between"), "");
    EXPECT_EQ(error("cycler(alpha=[2])", "plot"), "ax.plot: alpha must be a number from 0 to 1");
    EXPECT_EQ(error("cycler(alpha=[2])", "errorbar"), "ax.errorbar: alpha must be a number from 0 to 1");
    EXPECT_EQ(error("cycler(alpha=[2])", "scatter"), "");
    EXPECT_EQ(error("cycler(color='rg', lw=[1, 2])", "plot"), "");
}

// The number of points of a legend entry: at least one (matplotlib raises for less along a line;
// what it draws with no scatter point is not ported).
TEST(RcCalls, ALegendEntryHasAtLeastOnePoint) {
    const auto error = [](const std::string& rc) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": )" + rc + R"(, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
                {"on": "ax", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"label": "a"}}, {"on": "ax", "call": "legend"}]})",
            &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string("?");
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        return calculate_plot::engine::drawAgg(*figure, options).error;
    };
    EXPECT_EQ(error(R"({"legend.numpoints": 0})"), "ax.legend: numpoints must be > 0; it was 0");
    EXPECT_EQ(error(R"({"legend.numpoints": -2})"), "ax.legend: numpoints must be > 0; it was -2");
    EXPECT_EQ(error(R"({"legend.scatterpoints": 0})"), "ax.legend: scatterpoints below 1 are not supported yet");
    EXPECT_EQ(error(R"({"legend.scatterpoints": -1})"), "ax.legend: scatterpoints below 1 are not supported yet");
    EXPECT_EQ(error(R"({"legend.numpoints": 1, "legend.scatterpoints": 1, "legend.markerscale": 0.5})"), "");
    EXPECT_EQ(error(R"({"legend.numpoints": 4, "legend.scatterpoints": 7})"), "");
}

TEST(RcCalls, ATransformThatCannotBeInvertedStopsTheDrawing) {
    const auto error = [](const std::string& calls) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "rc": {"figure.subplot.wspace": 1e300}, "calls": [{"call": "subplots", "args": [2, 2], "as": ["fig", "axs"]})" +
                calls + "]}", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string();
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const std::string agg = calculate_plot::engine::drawAgg(*figure, options).error;
        EXPECT_EQ(calculate_plot::engine::drawSvg(*figure, options).error, agg);
        return agg;
    };
    const std::string plot = R"(, {"on": "axs[0, 0]", "call": "plot", "args": [[1, 2], [1, 2]]})";
    const std::string line = R"(, {"on": "axs[0, 0]", "call": "axhline", "args": [3.0]})";
    EXPECT_EQ(error(""), "");
    EXPECT_EQ(error(plot), "");
    EXPECT_EQ(error(line), "");
    EXPECT_EQ(error(plot + line), "axs[0, 0].axhline: Singular matrix");
    EXPECT_EQ(error(plot + line + plot), "axs[0, 0].axhline: Singular matrix");
    // and the next figure starts clean
    EXPECT_EQ(error(plot), "");
}

// One round marker with a size that is negative, nan or infinite: matplotlib takes the extents of
// the marker's curves, finds none and breaks ("need at least one array to concatenate").
TEST(CollectionCalls, OneCurvedMarkerOfNoFiniteSizeIsRefused) {
    for (const char* size : {"-1", R"({"$float": "nan"})", R"({"$float": "inf"})", R"({"$float": "-inf"})"}) {
        const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2, 3], [1, 3, 2]], "kwargs": {"s": )" +
                                  size + "}}");
        EXPECT_EQ(drawing.error, "need at least one array to concatenate") << size;
    }
    for (const char* size : {"0", "[-1, 4, 9]", "4"})
        EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2, 3], [1, 3, 2]], "kwargs": {"s": )" + size + "}}").error, "")
            << size;
}

class KeywordRefusals : public testing::TestWithParam<Refusal> {};

// Keyword values that matplotlib refuses (or breaks on when it draws) are refused by the call, never
// drawn as something else: colours that are none, alphas outside 0-1, widths and orders that are
// not finite, line styles that are none.
TEST_P(KeywordRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, KeywordRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"color": "t"}})", "axs[0].plot: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"color": [0.1, 0.2, {"$float": "nan"}]}})",
                "axs[0].plot: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"color": "\u0660.\u0665"}})", "axs[0].plot: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"markerfacecolor": "nocolor"}})",
                "axs[0].plot: bad markerfacecolor"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"markeredgecolor": [2, 0, 0]}})",
                "axs[0].plot: bad markeredgecolor"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"alpha": 2}})",
                "axs[0].plot: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"alpha": -0.1}})",
                "axs[0].plot: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"alpha": {"$float": "nan"}}})",
                "axs[0].plot: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"linewidth": {"$float": "inf"}}})",
                "axs[0].plot: linewidth must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"zorder": {"$float": "nan"}}})",
                "axs[0].plot: zorder must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "axhline", "args": [1], "kwargs": {"color": "T"}})", "axs[0].axhline: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "axvline", "args": [1], "kwargs": {"alpha": 3}})",
                "axs[0].axvline: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [1, 2], "kwargs": {"alpha": 1.5}})",
                "axs[0].axhspan: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [1, 2], "kwargs": {"linestyle": "t"}})", "axs[0].axvspan: bad linestyle"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [1, 2], "kwargs": {"ls": "Dashed"}})", "axs[0].axvspan: bad linestyle"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [1, 2], "kwargs": {"linewidth": {"$float": "-inf"}}})",
                "axs[0].axvspan: linewidth must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [1, 2], "kwargs": {"zorder": {"$float": "inf"}}})",
                "axs[0].axhspan: zorder must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [1, 2], "kwargs": {"color": [0.1, 0.2, {"$float": "nan"}]}})",
                "axs[0].axhspan: color is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"edgecolor": "t"}})",
                "axs[0].scatter: bad edgecolors"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"c": ["red", "nocolor"]}})",
                "axs[0].scatter: 'c' must be a color or a sequence of colors"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"alpha": 2}})",
                "axs[0].scatter: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"linewidths": []}})",
                "axs[0].scatter: bad linewidths"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"linewidths": [1, {"$float": "nan"}]}})",
                "axs[0].scatter: bad linewidths"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"zorder": {"$float": "nan"}}})",
                "axs[0].scatter: zorder must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": {"facecolors": "t"}})",
                "axs[0].fill_between: bad facecolors"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": {"ec": "nocolor"}})",
                "axs[0].fill_between: bad ec"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": {"alpha": 2}})",
                "axs[0].fill_between: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": {"linewidth": {"$float": "nan"}}})",
                "axs[0].fill_between: linewidth must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "hlines", "args": [[1, 2], 0, 1], "kwargs": {"colors": "t"}})", "axs[0].hlines: bad colors"},
        Refusal{R"({"on": "axs[0]", "call": "vlines", "args": [[1, 2], 0, 1], "kwargs": {"alpha": 2}})",
                "axs[0].vlines: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "hlines", "args": [[1, 2], 0, 1], "kwargs": {"linewidth": []}})", "axs[0].hlines: bad linewidths"},
        Refusal{R"({"on": "axs[0]", "call": "vlines", "args": [[1, 2], 0, 1], "kwargs": {"zorder": {"$float": "nan"}}})",
                "axs[0].vlines: zorder must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "color": "t"}})",
                "axs[0].errorbar: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "ecolor": "t"}})",
                "axs[0].errorbar: bad ecolor"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "alpha": 2}})",
                "axs[0].errorbar: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "fmt": "none", "alpha": 2}})",
                "axs[0].errorbar: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "fmt": "none", "linewidth": "wide"}})",
                "axs[0].errorbar: linewidth must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "fmt": null}})",
                "axs[0].errorbar: fmt must be a string"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "capsize": {"$float": "inf"}}})",
                "axs[0].errorbar: capsize must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[1, 2], [1, 2]], "kwargs": {"yerr": 0.1, "zorder": {"$float": "nan"}}})",
                "axs[0].errorbar: zorder must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"linestyle": "t"}})", "axs[0].grid: bad linestyle"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"color": "nocolor"}})", "axs[0].grid: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"c": "t"}})", "axs[0].grid: bad c"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"alpha": 2}})",
                "axs[0].grid: alpha must be a number from 0 to 1"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"linewidth": {"$float": "inf"}}})",
                "axs[0].grid: linewidth must be a finite number"},
        Refusal{R"({"on": "axs[0]", "call": "legend", "kwargs": {"edgecolor": [0.1, 0.2, {"$float": "nan"}]}})",
                "axs[0].legend: bad edgecolor"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"color": "red", "c": "blue"}})",
                "axs[0].grid: c is given twice (it is an alias of color)"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"ls": ":", "linestyle": "--"}})",
                "axs[0].grid: linestyle is given twice (it is an alias of linestyle)"},
        Refusal{R"({"on": "axs[0]", "call": "grid", "args": [true], "kwargs": {"lw": 1, "linewidth": 2}})",
                "axs[0].grid: linewidth is given twice (it is an alias of linewidth)"},
        Refusal{R"({"on": "axs[0]", "call": "axhline", "args": [1], "kwargs": {"c": null}})", "axs[0].axhline: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [1, 2], "kwargs": {"color": "red", "facecolor": "nocolor"}})",
                "axs[0].axvspan: facecolor is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [1, 2], "kwargs": {"edgecolor": [1, 2], "color": "red"}})",
                "axs[0].axhspan: edgecolor is not a colour"},
        Refusal{R"({"on": "axs[0]", "call": "axvline", "args": [1], "kwargs": {"c": null}})", "axs[0].axvline: bad color"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": {"step": false}})",
                "axs[0].fill_between: step must be 'pre', 'post' or 'mid'"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlim", "kwargs": {"left": {"$float": "nan"}}})",
                "axs[0].set_xlim: limits cannot be NaN or Inf"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlim", "args": [0, {"$float": "inf"}]})", "axs[0].set_xlim: limits cannot be NaN or Inf"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylim", "args": [[{"$float": "-inf"}, 1]]})", "axs[0].set_ylim: limits cannot be NaN or Inf"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylim", "kwargs": {"top": {"$float": "nan"}}})",
                "axs[0].set_ylim: limits cannot be NaN or Inf"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlim", "kwargs": {"left": 18446744073709551616}})",
                "axs[0].set_xlim: an integer limit must fit in 64 bits"},
        Refusal{R"({"on": "axs[0]", "call": "set_ylim", "args": [-9223372036854775809, 1]})",
                "axs[0].set_ylim: an integer limit must fit in 64 bits"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2], "\u0663"]})",
                "'\u0663' is not a valid format string (unrecognized character '\u0663')"}));

// The integers numpy holds (64 bits, signed or not) are limits like any other number.
TEST(LimitCalls, TheLargestIntegersAreTaken) {
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_xlim", "args": [0, 18446744073709551615]})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_xlim", "args": [-9223372036854775808, 1]})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_ylim", "args": [0, 1.8446744073709552e+30]})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_ylim", "args": [0, 28446744073709551615]})").error,
              "axs[0].set_ylim: an integer limit must fit in 64 bits");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_ylim", "args": [-19223372036854775808, 0]})").error,
              "axs[0].set_ylim: an integer limit must fit in 64 bits");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_ylim", "args": [0, 100000000000000000000]})").error,
              "axs[0].set_ylim: an integer limit must fit in 64 bits");
}

// A keyword that names an argument already given by position is refused, as Python refuses it.
TEST(ArgumentForms, AKeywordCannotRepeatAPositionalArgument) {
    struct Row {
        const char* on;
        const char* call;
        std::vector<const char*> names;
        std::vector<const char*> values;
    };
    const std::vector<Row> rows = {
        {"axs[0]", "scatter", {"x", "y", "s", "c"}, {"[1, 2]", "[1, 2]", "30", "\"red\""}},
        {"axs[0]", "errorbar", {"x", "y", "yerr", "xerr", "fmt"}, {"[1, 2]", "[1, 2]", "0.2", "0.1", "\"o\""}},
        {"axs[0]", "fill_between", {"x", "y1", "y2"}, {"[1, 2]", "[1, 2]", "[0, 0]"}},
        {"axs[0]", "axhline", {"y", "xmin", "xmax"}, {"1", "0.1", "0.9"}},
        {"axs[0]", "axvline", {"x", "ymin", "ymax"}, {"1", "0.1", "0.9"}},
        {"axs[0]", "axhspan", {"ymin", "ymax", "xmin", "xmax"}, {"1", "2", "0.1", "0.9"}},
        {"axs[0]", "axvspan", {"xmin", "xmax", "ymin", "ymax"}, {"1", "2", "0.1", "0.9"}},
        {"axs[0]", "vlines", {"x", "ymin", "ymax", "colors", "linestyles", "label"}, {"[1, 2]", "0", "1", "\"red\"", "\"dashed\"", "\"l\""}},
        {"axs[0]", "hlines", {"y", "xmin", "xmax", "colors", "linestyles", "label"}, {"[1, 2]", "0", "1", "\"red\"", "\"dashed\"", "\"l\""}},
        {"axs[0]", "grid", {"visible"}, {"true"}},
        {"axs[0]", "text", {"x", "y", "s", "fontdict"}, {"1", "2", "\"t\"", "null"}},
        {"fig", "text", {"x", "y", "s", "fontdict"}, {"0.5", "0.5", "\"t\"", "null"}},
        {"axs[0]", "set_title", {"label"}, {"\"t\""}},
        {"axs[0]", "set_xlabel", {"xlabel"}, {"\"t\""}},
        {"axs[0]", "set_ylabel", {"ylabel"}, {"\"t\""}},
        {"axs[0]", "set_xlim", {"left", "right"}, {"0", "5"}},
        {"axs[0]", "set_ylim", {"bottom", "top"}, {"0", "5"}},
        {"fig", "suptitle", {"t"}, {"\"t\""}},
        {"fig", "subplots_adjust", {"left", "bottom", "right", "top", "wspace", "hspace"}, {"0.1", "0.1", "0.9", "0.9", "0.2", "0.2"}},
    };
    for (const Row& row : rows)
        for (std::size_t i = 0; i < row.names.size(); ++i) {
            std::string args;
            for (std::size_t k = 0; k <= i; ++k) args += std::string(k ? ", " : "") + row.values[k];
            const std::string call = std::string(R"({"on": ")") + row.on + R"(", "call": ")" + row.call + R"(", "args": [)" + args +
                                     R"(], "kwargs": {")" + row.names[i] + R"(": )" + row.values[i] + "}}";
            EXPECT_EQ(draw(kSubplots + call).error,
                      std::string(row.on) + "." + row.call + ": got multiple values for argument '" + row.names[i] + "'")
                << call;
        }
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [1], "kwargs": {"nrows": 1}})").error,
              "plt.subplots: got multiple values for argument 'nrows'");
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [1, 2], "kwargs": {"ncols": 2}})").error,
              "plt.subplots: got multiple values for argument 'ncols'");
    // (a keyword of another call's argument is that call's business)
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "axhline", "args": [1], "kwargs": {"xmin": 0.2}})").error, "");
}

// More positional arguments than the call has parameters for are refused.
TEST(ArgumentForms, TooManyPositionalArgumentsAreRefused) {
    const auto error = [](const std::string& on, const std::string& call, const std::string& args) {
        return draw(kSubplots + R"({"on": ")" + on + R"(", "call": ")" + call + R"(", "args": [)" + args + "]}").error;
    };
    EXPECT_EQ(error("axs[0]", "axhline", "1, 0.1, 0.9, 1"), "axs[0].axhline: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "axvline", "1, 0.1, 0.9, 1"), "axs[0].axvline: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "axhspan", "1, 2, 0.1, 0.9, 1"), "axs[0].axhspan: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "axvspan", "1, 2, 0.1, 0.9, 1"), "axs[0].axvspan: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "vlines", R"([1, 2], 0, 1, "red", "dashed", "l", 1)"), "axs[0].vlines: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "hlines", R"([1, 2], 0, 1, "red", "dashed", "l", 1)"), "axs[0].hlines: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "set_xlim", "0, 5, 1"), "axs[0].set_xlim: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "set_ylim", "0, 5, 1"), "axs[0].set_ylim: too many positional arguments");
    EXPECT_EQ(error("fig", "suptitle", R"("t", 1)"), "fig.suptitle: too many positional arguments");
    EXPECT_EQ(error("axs[0]", "text", R"(1, 2, "t", null, 1)"), "axs[0].text: too many positional arguments");
    EXPECT_EQ(error("fig", "text", R"(0.5, 0.5, "t", null, 1)"), "fig.text: too many positional arguments");
    EXPECT_EQ(error("fig", "subplots_adjust", "0.1, 0.1, 0.9, 0.9, 0.2, 0.2, 1"), "fig.subplots_adjust: too many positional arguments");
    EXPECT_EQ(draw(R"({"call": "subplots", "args": [1, 2, 3]})").error, "plt.subplots: too many positional arguments");
    // as many as there are parameters
    EXPECT_EQ(error("axs[0]", "axhline", "1, 0.1, 0.9"), "");
    EXPECT_EQ(error("axs[0]", "axvspan", "1, 2, 0.1, 0.9"), "");
    EXPECT_EQ(error("axs[0]", "set_xlim", "0, 5"), "");
}

// vlines and hlines take their colours, line styles and label by position too.
TEST(ArgumentForms, VlinesTakesColoursStylesAndLabelByPosition) {
    const auto lines = [](const std::string& call, const std::string& args, const std::string& kwargs) {
        const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": ")" + call + R"(", "args": [[1, 2], 0, 1)" + args +
                                  R"(], "kwargs": )" + kwargs + R"(}, {"on": "axs[0]", "call": "legend"})");
        EXPECT_EQ(drawing.error, "") << call << args << kwargs;
        return drawing.trace;
    };
    for (const std::string call : {"vlines", "hlines"}) {
        const std::string plain = lines(call, "", R"({"label": "l"})");
        const std::string all = lines(call, "", R"({"colors": "red", "linestyles": "dashed", "label": "l"})");
        EXPECT_NE(lines(call, "", R"({"colors": "red", "label": "l"})"), plain);
        EXPECT_NE(all, lines(call, "", R"({"colors": "red", "label": "l"})"));
        EXPECT_EQ(lines(call, R"(, "red")", R"({"label": "l"})"), lines(call, "", R"({"colors": "red", "label": "l"})"));
        EXPECT_EQ(lines(call, R"(, "red", "dashed")", R"({"label": "l"})"), all);
        EXPECT_EQ(lines(call, R"(, "red", "dashed", "l")", "{}"), all);
        EXPECT_NE(lines(call, R"(, "red", "dashed", "another")", "{}"), all);
    }
}

// colors=None is vlines' default; color=None comes after it as a keyword and sets the default colour again.
TEST(ArgumentForms, VlinesColorNoneSetsTheDefaultColourAgain) {
    const auto lines = [](const std::string& call, const std::string& kwargs) {
        const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": ")" + call + R"(", "args": [[1, 2], 0, 1], "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << call << kwargs;
        return drawing.trace;
    };
    for (const std::string call : {"vlines", "hlines"}) {
        const std::string plain = lines(call, "{}"), blue = lines(call, R"({"colors": "blue"})");
        EXPECT_NE(blue, plain);
        EXPECT_EQ(lines(call, R"({"colors": "blue", "color": null})"), plain);
        EXPECT_EQ(lines(call, R"({"color": null, "colors": "blue"})"), plain);
        EXPECT_EQ(lines(call, R"({"colors": null, "color": "blue"})"), blue);
        EXPECT_EQ(lines(call, R"({"colors": null})"), plain);
    }
}

// Where matplotlib normalises the keywords of a call (cbook.normalize_kwargs), a property given by
// two of its names is refused.
TEST(ArgumentForms, APropertyGivenByTwoOfItsNamesIsRefused) {
    const auto error = [](const std::string& call, const std::string& args, const std::string& first, const std::string& second) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": ")" + call + R"(", "args": [)" + args + R"(], "kwargs": {")" + first +
                    R"(": 1, ")" + second + R"(": 1}})")
            .error;
    };
    const std::vector<std::pair<const char*, const char*>> line = {
        {"aa", "antialiased"}, {"c", "color"}, {"ds", "drawstyle"}, {"ls", "linestyle"}, {"lw", "linewidth"},
        {"mec", "markeredgecolor"}, {"mew", "markeredgewidth"}, {"mfc", "markerfacecolor"},
        {"mfcalt", "markerfacecoloralt"}, {"ms", "markersize"}};
    for (const std::string call : {"plot", "errorbar"})
        for (const auto& [alias, name] : line) {
            EXPECT_EQ(error(call, "[1, 2], [1, 2]", name, alias),
                      "axs[0]." + call + ": " + alias + " is given twice (it is an alias of " + name + ")");
            EXPECT_EQ(error(call, "[1, 2], [1, 2]", alias, name),
                      "axs[0]." + call + ": " + name + " is given twice (it is an alias of " + name + ")");
        }
    const std::vector<std::pair<const char*, const char*>> collection = {
        {"aa", "antialiased"}, {"antialiaseds", "antialiased"}, {"ec", "edgecolor"}, {"edgecolors", "edgecolor"},
        {"fc", "facecolor"}, {"facecolors", "facecolor"}, {"dashes", "linestyle"}, {"linestyles", "linestyle"},
        {"ls", "linestyle"}, {"linewidths", "linewidth"}, {"lw", "linewidth"}};
    for (const std::string call : {"scatter", "fill_between"})
        for (const auto& [alias, name] : collection) {
            EXPECT_EQ(error(call, "[1, 2], [1, 2]", name, alias),
                      "axs[0]." + call + ": " + alias + " is given twice (it is an alias of " + name + ")");
            EXPECT_EQ(error(call, "[1, 2], [1, 2]", alias, name),
                      "axs[0]." + call + ": " + name + " is given twice (it is an alias of " + name + ")");
        }
    EXPECT_EQ(error("scatter", "[1, 2], [1, 2]", "ec", "edgecolors"),
              "axs[0].scatter: edgecolors is given twice (it is an alias of edgecolor)");
}

// plot gives its keywords to Line2D: None for one of Line2D's own parameters is its default.
TEST(ArgumentForms, NoneForAParameterOfALineIsItsDefault) {
    const auto plot = [](const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": )" + kwargs + "}");
    };
    const auto plain = plot("{}");
    ASSERT_EQ(plain.error, "");
    for (const char* name : {"linewidth", "linestyle", "color", "gapcolor", "marker", "markersize", "markeredgewidth", "markeredgecolor",
                             "markerfacecolor", "markerfacecoloralt", "fillstyle", "antialiased", "dash_capstyle", "solid_capstyle",
                             "dash_joinstyle", "solid_joinstyle", "pickradius", "drawstyle", "markevery", "lw", "ls", "c", "ms", "mew", "mec",
                             "mfc", "mfcalt", "aa", "ds"}) {
        const auto none = plot(std::string(R"({")") + name + R"(": null})");
        EXPECT_EQ(none.error, "") << name;
        EXPECT_EQ(none.trace, plain.trace) << name;
    }
    // (the keywords that are no parameter of Line2D go to their setters, None or not)
    EXPECT_EQ(plot(R"({"zorder": null})").error, "axs[0].plot: zorder must be a finite number");
    EXPECT_EQ(plot(R"({"drawstyle": "steps"})").error, "");
    EXPECT_EQ(plot(R"({"drawstyle": "stairs"})").error,
              "axs[0].plot: drawstyle must be one of 'default', 'steps-mid', 'steps-pre', 'steps-post', 'steps'");
    EXPECT_EQ(plot(R"({"ds": 1})").error,
              "axs[0].plot: drawstyle must be one of 'default', 'steps-mid', 'steps-pre', 'steps-post', 'steps'");
    EXPECT_EQ(plot(R"({"bogus": null})").error, "axs[0].plot: unsupported keyword bogus");
}

// fill_between draws straight between the points: no interpolation of crossings, no steps.
TEST(ArgumentForms, FillBetweenTakesNoInterpolationAndStepsByName) {
    const auto fill = [](const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": )" + kwargs + "}");
    };
    const auto plain = fill("{}");
    ASSERT_EQ(plain.error, "");
    for (const char* kwargs : {R"({"interpolate": false})", R"({"interpolate": null})", R"({"step": null})"}) {
        EXPECT_EQ(fill(kwargs).error, "") << kwargs;
        EXPECT_EQ(fill(kwargs).trace, plain.trace) << kwargs;
    }
    EXPECT_EQ(fill(R"({"interpolate": true})").error, "axs[0].fill_between: interpolate is not supported yet");
    EXPECT_EQ(fill(R"({"interpolate": 0})").error, "axs[0].fill_between: interpolate is not supported yet");
    EXPECT_EQ(fill(R"({"step": "pre"})").error, "");
    EXPECT_EQ(fill(R"({"step": "steps"})").error, "axs[0].fill_between: step must be 'pre', 'post' or 'mid'");
    EXPECT_EQ(fill(R"({"step": "Pre"})").error, "axs[0].fill_between: step must be 'pre', 'post' or 'mid'");
}

// scatter's own edgecolors and linewidths join the keywords only when they are not None.
TEST(ArgumentForms, ScattersOwnParametersThatAreNoneAreNotKeywords) {
    const auto scatter = [](const std::string& kwargs) {
        const auto drawing = draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << kwargs;
        return drawing.trace;
    };
    EXPECT_EQ(scatter(R"({"edgecolors": null, "ec": "red"})"), scatter(R"({"ec": "red"})"));
    EXPECT_EQ(scatter(R"({"lw": 3, "linewidths": null})"), scatter(R"({"lw": 3})"));
    EXPECT_NE(scatter(R"({"lw": 3})"), scatter("{}"));
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "scatter", "args": [[1, 2], [1, 2]], "kwargs": {"ec": null, "edgecolor": "red"}})").error,
              "axs[0].scatter: edgecolor is given twice (it is an alias of edgecolor)");
}

// markerfirst=None is not the default: it is false, and the markers come last.
TEST(LegendCalls, MarkerFirstNoneIsFalse) {
    const std::string plot = R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"label": "a"}},)";
    const auto legend = [&](const std::string& kwargs) {
        const auto drawing = draw(kSubplots + plot + R"({"on": "axs[0]", "call": "legend", "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << kwargs;
        return drawing.trace;
    };
    EXPECT_EQ(legend(R"({"markerfirst": null})"), legend(R"({"markerfirst": false})"));
    EXPECT_NE(legend(R"({"markerfirst": null})"), legend("{}"));
    EXPECT_EQ(legend(R"({"markerfirst": true})"), legend("{}"));
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

// What plt.subplots and plt.figure refuse of the figure's keywords, or cannot draw with.
TEST(SubplotsCalls, RefuseFigureKeywordsThatMatplotlibRefuses) {
    const auto error = [](const std::string& kwargs) { return draw(R"({"call": "subplots", "kwargs": )" + kwargs + "}").error; };
    for (const char* dpi : {"0", "-1", R"({"$float": "nan"})", R"({"$float": "inf"})", R"({"$float": "-inf"})"})
        EXPECT_EQ(error(std::string(R"({"dpi": )") + dpi + "}"), "plt.subplots: dpi must be positive and finite") << dpi;
    EXPECT_EQ(error(R"({"dpi": 50.5})"), "");
    for (const char* figsize : {"[-7, 7]", "[4, -0.001]", R"([{"$float": "nan"}, 1])", R"([1, {"$float": "inf"}])"})
        EXPECT_EQ(error(std::string(R"({"figsize": )") + figsize + "}"), "plt.subplots: figsize must be finite and not negative") << figsize;
    EXPECT_EQ(error(R"({"figsize": [0, 3]})"), "");  // (no pixels to save, but a figure)
    EXPECT_EQ(error(R"({"figsize": [4, 0]})"), "");
    for (const char* count : {"1.0", "1.5", "true", "null", "\"1\""}) {
        EXPECT_EQ(error(std::string(R"({"nrows": )") + count + "}"), "plt.subplots: nrows must be an integer") << count;
        EXPECT_EQ(error(std::string(R"({"ncols": )") + count + "}"), "plt.subplots: ncols must be an integer") << count;
    }
    EXPECT_EQ(error(R"({"nrows": 2, "ncols": 2})"), "");
    for (const char* color : {"\"t\"", "\"2\"", "[0.1, 0.2]", "true", "0.5", "\"auto\""}) {
        EXPECT_EQ(error(std::string(R"({"facecolor": )") + color + "}"), "plt.subplots: bad facecolor") << color;
        EXPECT_EQ(error(std::string(R"({"edgecolor": )") + color + "}"), "plt.subplots: bad edgecolor") << color;
    }
    EXPECT_EQ(draw(R"({"call": "figure", "kwargs": {"facecolor": "t"}})").error, "plt.figure: bad facecolor");
    EXPECT_EQ(draw(R"({"call": "figure", "kwargs": {"dpi": 0}})").error, "plt.figure: dpi must be positive and finite");
}

// The figure's colours are any colour: a name, a tuple, 'none'; None is the rcParams'.
TEST(SubplotsCalls, TheFiguresColoursAreAnyColour) {
    const auto trace = [](const std::string& kwargs) {
        const auto drawing = draw(R"({"call": "subplots", "kwargs": )" + kwargs + "}");
        EXPECT_EQ(drawing.error, "") << kwargs;
        return drawing.trace;
    };
    const std::string plain = trace("{}");
    EXPECT_EQ(trace(R"({"facecolor": null, "edgecolor": null})"), plain);
    EXPECT_NE(trace(R"({"facecolor": "red"})"), plain);
    EXPECT_EQ(trace(R"({"facecolor": [1, 0, 0]})"), trace(R"({"facecolor": "red"})"));
    EXPECT_EQ(trace(R"({"facecolor": {"$tuple": [1.0, 0.0, 0.0, 0.2]}})"), trace(R"({"facecolor": "#ff000033"})"));
    EXPECT_NE(trace(R"({"facecolor": "#ff000033"})"), trace(R"({"facecolor": "red"})"));
    EXPECT_EQ(trace(R"({"edgecolor": [0, 0, 1]})"), trace(R"({"edgecolor": "blue"})"));
    EXPECT_NE(trace(R"({"facecolor": "none"})"), plain);
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

class AxisRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(AxisRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, AxisRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0].xaxis", "call": "set_major_formatter"})",
                "axs[0].xaxis.set_major_formatter: missing 1 required positional argument: 'formatter'"},
        Refusal{R"({"on": "axs[0].xaxis", "call": "set_major_formatter", "args": [{"$new": "NullFormatter"}, 1]})",
                "axs[0].xaxis.set_major_formatter: too many positional arguments"},
        Refusal{R"({"on": "axs[0].xaxis", "call": "set_major_formatter", "args": [{"$new": "NullFormatter"}],
                    "kwargs": {"formatter": {"$new": "NullFormatter"}}})",
                "axs[0].xaxis.set_major_formatter: got multiple values for argument 'formatter'"},
        Refusal{R"({"on": "axs[0].yaxis", "call": "set_minor_formatter", "kwargs": {"fmt": {"$new": "NullFormatter"}}})",
                "axs[0].yaxis.set_minor_formatter: got an unexpected keyword argument 'fmt'"},
        Refusal{R"({"on": "axs[0].yaxis", "call": "set_minor_formatter", "args": ["%d"]})",
                "axs[0].yaxis.set_minor_formatter: a formatter other than NullFormatter() is not supported yet"},
        Refusal{R"({"on": "axs[0].yaxis", "call": "set_minor_formatter", "args": [{"$new": "ScalarFormatter"}]})",
                "axs[0].yaxis.set_minor_formatter: a formatter other than NullFormatter() is not supported yet"},
        Refusal{R"({"on": "axs[0].xaxis", "call": "set_major_locator", "args": [{"$new": "NullLocator"}]})",
                "unsupported call: axs[0].xaxis.set_major_locator"},
        Refusal{R"({"on": "axs[0].zaxis", "call": "set_major_formatter", "args": [{"$new": "NullFormatter"}]})",
                "unsupported call: axs[0].zaxis.set_major_formatter"},
        Refusal{R"({"on": "fig.xaxis", "call": "set_major_formatter", "args": [{"$new": "NullFormatter"}]})",
                "unsupported call: fig.xaxis.set_major_formatter"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [{"$new": "NullFormatter"}]})",
                "axs[0].plot: y must be a list of numbers"}));

// A line style (offset, on-off sequence) is a tuple; Matplotlib refuses the rest, and dashes that are
// negative or all zero when the line is drawn.
TEST(LineCalls, DashTuplesAreReadAsMatplotlibReadsThem) {
    const auto plot = [](const std::string& style) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]], "kwargs": {"linestyle": )" + style + "}}").error;
    };
    EXPECT_EQ(plot(R"({"$tuple": [0, {"$tuple": [4, 2]}]})"), "");
    EXPECT_EQ(plot(R"({"$tuple": [1, null]})"), "");
    EXPECT_EQ(plot(R"([0, [4, 2]])"), "axs[0].plot: bad linestyle");
    EXPECT_EQ(plot(R"({"$tuple": [null, {"$tuple": [4, 2]}]})"), "axs[0].plot: bad linestyle");
    EXPECT_EQ(plot(R"({"$tuple": [0, {"$tuple": [4, 2]}, 1]})"), "axs[0].plot: bad linestyle");
    EXPECT_EQ(plot(R"({"$tuple": [0, "4 2"]})"), "axs[0].plot: bad linestyle");
    EXPECT_EQ(plot(R"({"$tuple": [0, {"$tuple": [4, -2]}]})"), "All values in the dash list must be non-negative");
    EXPECT_EQ(plot(R"({"$tuple": [0, {"$tuple": [0, 0]}]})"), "At least one value in the dash list must be positive");
}

// Markers 0 to 11 (TICKLEFT ... CARETDOWNBASE) are numbers, as markers.py's dict finds them (a bool or a
// float of the same value too); their names are no marker a figure file can give.
TEST(MarkerCalls, IntegerMarkersAreMatplotlibs) {
    const auto error = [](const std::string& call, const std::string& marker) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": ")" + call + R"(", "args": [[0, 1], [0, 1]], "kwargs": {"marker": )" +
                    marker + "}}")
            .error;
    };
    for (const char* call : {"plot", "scatter"}) {
        for (const char* ok : {"0", "4", "11", "true", "false", "4.0"}) EXPECT_EQ(error(call, ok), "") << call << " " << ok;
        const std::string where = std::string("axs[0].") + call + ": ";
        EXPECT_EQ(error(call, "12"), where + "Unrecognized marker style 12");
        EXPECT_EQ(error(call, "-1"), where + "Unrecognized marker style -1");
        EXPECT_EQ(error(call, "4.5"), where + "Unrecognized marker style 4.5");
        EXPECT_EQ(error(call, R"("tickleft")"), where + "Unrecognized marker style 'tickleft'");
        EXPECT_EQ(error(call, R"("caretdownbase")"), where + "Unrecognized marker style 'caretdownbase'");
    }
}

// add_subplot and plt.subplot read their arguments as SubplotSpec._from_subplot_args does.
TEST(SubplotCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args) {
        return draw(R"({"call": "figure", "as": "fig"}, {"on": "fig", "call": "add_subplot", "args": )" + args + "}").error;
    };
    EXPECT_EQ(error("[]"), "");
    EXPECT_EQ(error("[111]"), "");
    EXPECT_EQ(error(R"([2, 2, {"$tuple": [-1, 4]}])"), "");
    EXPECT_EQ(error("[1111]"), "fig.add_subplot: Single argument to subplot must be a three-digit integer, not 1111");
    EXPECT_EQ(error("[true]"), "fig.add_subplot: Single argument to subplot must be a three-digit integer, not True");
    EXPECT_EQ(error("[110]"), "fig.add_subplot: num must be an integer with 1 <= num <= 1, not 0");
    EXPECT_EQ(error("[2, 2, 5]"), "fig.add_subplot: num must be an integer with 1 <= num <= 4, not 5");
    EXPECT_EQ(error(R"([2, 2, {"$tuple": [0, 1]}])"), "fig.add_subplot: GridSpec slice would result in no space allocated for subplot");
    EXPECT_EQ(error(R"([2, 2, {"$tuple": [3, 1]}])"), "fig.add_subplot: GridSpec slice would result in no space allocated for subplot");
    EXPECT_EQ(error("[0, 1, 1]"), "fig.add_subplot: Number of rows must be a positive integer, not 0");
    EXPECT_EQ(error("[1.5, 1, 1]"), "fig.add_subplot: Number of rows must be a positive integer, not 1.5");
    EXPECT_EQ(error("[1, 2]"), "fig.add_subplot: subplot() takes 1 or 3 positional arguments but 2 were given");
    EXPECT_EQ(error(R"([2, 2, {"$tuple": [1, 2.5]}])"), "fig.add_subplot: Subplot specifier tuple must contain integers, not (1, 2.5)");
    EXPECT_EQ(draw(R"({"call": "plot", "args": [[0, 1]]}, {"call": "xlim"})").error,
              "plt.xlim() without arguments (the limits as they are) is not supported yet");
}

// ax.set(**kwargs): each keyword through its setter, in order; positional arguments are refused.
TEST(AxesCalls, SetGoesThroughTheSetters) {
    const auto error = [](const std::string& call) { return draw(kSubplots + call).error; };
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "kwargs": {"xlim": [0, 1], "title": "t"}})"), "");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "args": [1]})"), "axs[0].set: set() takes 1 positional argument but 2 were given");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "kwargs": {"aspect": "equal"}})"), "axs[0].set: set(aspect=...) is not supported yet");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "kwargs": {"xlim": [0, {"$float": "nan"}]}})").find("axs[0].set_xlim"), 0u);
}

// fig.add_axes(rect) and plt.axes(rect): four finite numbers, not negative in size.
TEST(SubplotCalls, AxesAtARect) {
    const auto error = [](const std::string& call) { return draw(R"({"call": "figure", "as": "fig"}, )" + call).error; };
    EXPECT_EQ(error(R"({"on": "fig", "call": "add_axes", "args": [[0.1, 0.1, 0.5, 0.5]]})"), "");
    EXPECT_EQ(error(R"({"on": "fig", "call": "add_axes", "args": [[0.1, 0.1, 0.5]]})"), "fig.add_axes: rect must be (left, bottom, width, height)");
    EXPECT_EQ(error(R"({"on": "fig", "call": "add_axes", "args": [[0.1, 0.1, -0.5, 0.5]]})"), "fig.add_axes: Width and height specified must be non-negative");
    EXPECT_EQ(error(R"({"on": "fig", "call": "add_axes", "args": [[0.1, {"$float": "inf"}, 0.5, 0.5]]})"),
              "fig.add_axes: all entries in rect must be finite not [0.1, inf, 0.5, 0.5]");
    EXPECT_EQ(error(R"({"call": "axes", "args": [[0.1, 0.1, 0.5, 0.5]], "kwargs": {"facecolor": "r"}})"),
              "plt.axes: keyword arguments are not supported yet");
    EXPECT_EQ(error(R"({"call": "plot", "args": [[1, 2]]}, {"call": "loglog", "args": [[1, 2]]})"), "unsupported call: plt.loglog");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "loglog", "args": [[1, 2]], "kwargs": {"base": 2}})").error,
              "axs[0].loglog: the keywords of the log scale are not supported yet");
}

// bar and barh: their arguments broadcast together; labels one per bar or one for all.
TEST(BarCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "bar", "args": [[1, 2, 3], [4, 5, 6]], "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(error("{}"), "");
    EXPECT_EQ(error(R"({"width": [0.5, 0.5]})"), "axs[0].bar: the arguments cannot be broadcast to one shape");
    EXPECT_EQ(error(R"({"label": ["a", "b"]})"), "axs[0].bar: number of labels (2) does not match number of bars (3).");
    EXPECT_EQ(error(R"({"align": "left"})"), "axs[0].bar: align must be 'center' or 'edge'");
    EXPECT_EQ(error(R"({"tick_label": ["a", "b", "c"]})"), "axs[0].bar: tick_label is not supported yet");
    EXPECT_EQ(error(R"({"lw": 1, "linewidth": 2})").find("axs[0].bar:"), 0u);
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "bar", "args": [[1, 2]]})").error, "axs[0].bar: height is required");
}
