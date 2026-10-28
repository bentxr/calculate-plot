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
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"bbox": [1]}})",
                "axs[0].text: bbox must be a dict ({\"$dict\": {...}}) or None"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [1, 2, "t"], "kwargs": {"bbox": {"$dict": {"boxstyle": "cloud"}}}})",
                "axs[0].text: Unknown style: 'cloud,pad=0.30'"},
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
    EXPECT_EQ(error("cycler(fc='rg')", "fill"), "");  // (every property a cycle may have is a polygon's, or a marker's)
    EXPECT_EQ(error("cycler(marker=['o'])", "fill"), "");  // (the markers are left out for a polygon)
    EXPECT_EQ(error("cycler(color='rg', lw=[1, 2])", "fill"), "");
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

// A log axis whose view reaches zero or below, with no positive data to stand in (minpos), has no
// ticks: LogLocator.tick_values raises "Data cannot be log-scaled because all values are <= 0." and
// the drawing ends there.
TEST(RcCalls, ALogAxisWithNothingPositiveStopsTheDrawing) {
    const auto error = [](const std::string& calls) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]})" + calls + "]}", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string("?");
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const std::string agg = calculate_plot::engine::drawAgg(*figure, options).error;
        EXPECT_EQ(calculate_plot::engine::drawSvg(*figure, options).error, agg);
        return agg;
    };
    const std::string raised = "Data cannot be log-scaled because all values are <= 0.";
    const std::string xlim = R"(, {"on": "ax", "call": "set_xlim", "args": [-4.2, 5.8]})";
    const std::string ylim = R"(, {"on": "ax", "call": "set_ylim", "args": [-2.9, -2.8]})";
    const std::string plot = R"(, {"on": "ax", "call": "plot", "args": [[1, 2], [1, 2]]})";
    EXPECT_EQ(error(xlim + R"(, {"on": "ax", "call": "set_xscale", "args": ["log"]})"), raised);
    EXPECT_EQ(error(ylim + R"(, {"on": "ax", "call": "semilogy"})"), raised);
    EXPECT_EQ(error(plot + xlim + R"(, {"on": "ax", "call": "set_xscale", "args": ["log"]})"), "");
    // limits the other way round: tick_values swaps them after that test, and math.ceil of the logarithm
    // of a negative value (NaN) or of 0 (-inf) raises
    EXPECT_EQ(error(R"(, {"on": "ax", "call": "set_xlim", "args": [0.333, -1.667]}, {"on": "ax", "call": "loglog"})"),
              "cannot convert float NaN to integer");
    EXPECT_EQ(error(R"(, {"on": "ax", "call": "set_xlim", "args": [0.5, 0]}, {"on": "ax", "call": "semilogx"})"),
              "cannot convert float infinity to integer");
    // and the next figure starts clean
    EXPECT_EQ(error(plot), "");
}

// A logit axis whose smallest positive datum is 1 or more: LogitLocator.nonsingular puts the upper limit at
// 1 - minpos (<= 0), and int() of its logarithm raises (it hung the port: a loop over 2**63 decades).
TEST(RcCalls, ALogitAxisWithMinposFromOneUpStopsTheDrawing) {
    const auto error = [](const std::string& calls) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]})" + calls + "]}", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string("?");
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const std::string agg = calculate_plot::engine::drawAgg(*figure, options).error;
        EXPECT_EQ(calculate_plot::engine::drawSvg(*figure, options).error, agg);
        return agg;
    };
    const auto logit = [](const char* xs, const char* lim) {
        return std::string(R"(, {"on": "ax", "call": "plot", "args": [)") + xs + R"(, [1, 2]]}, {"on": "ax", "call": "set_xlim", "args": )" +
               lim + R"(}, {"on": "ax", "call": "set_xscale", "args": ["logit"]})";
    };
    EXPECT_EQ(error(logit("[1.5, 3]", "[0.2, 2]")), "cannot convert float NaN to integer");
    EXPECT_EQ(error(logit("[1.0, 3]", "[0.2, 2]")), "cannot convert float infinity to integer");
    EXPECT_EQ(error(logit("[2, 3]", "[0.2, 0.9]")), "");
}

// A pie on a logit axis: its limits (-1.25, 1.25) have no logit, so the equal aspect leaves the Axes' box
// NaN. A locator that asks the axis how many labels fit then raises (int() of NaN); with the ticks fixed by
// the pie, tight_layout takes the wedges' extents, finds no finite curve and raises too.
TEST(RcCalls, APieOnALogitAxisStopsTheDrawing) {
    const auto error = [](const std::string& calls) {
        std::string parseError;
        const auto figure = calculate_plot::model::parseFigure(
            R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]})" + calls + "]}", &parseError);
        EXPECT_TRUE(figure.has_value()) << parseError;
        if (!figure) return std::string("?");
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const std::string agg = calculate_plot::engine::drawAgg(*figure, options).error;
        EXPECT_EQ(calculate_plot::engine::drawSvg(*figure, options).error, agg);
        return agg;
    };
    // (a wedge wholly left of x = 0 has no finite point at all)
    const std::string pie = R"(, {"on": "ax", "call": "pie", "args": [[9, 1, 0.5, 0.906, 8.433]]})";
    const std::string logit = R"(, {"on": "ax", "call": "set_xscale", "args": ["logit"]})";
    const std::string tight = R"(, {"on": "fig", "call": "tight_layout"})";
    EXPECT_EQ(error(pie + logit), "cannot convert float NaN to integer");
    EXPECT_EQ(error(logit + pie + tight), "fig.tight_layout: need at least one array to concatenate");
    EXPECT_EQ(error(logit + pie), "");
    EXPECT_EQ(error(logit + R"(, {"on": "ax", "call": "pie", "args": [[9, 1, 0.5]]})" + tight), "");
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
TEST(ArgumentForms, FillBetweenTakesInterpolationAndStepsByName) {
    const auto fill = [](const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [1, 2]], "kwargs": )" + kwargs + "}");
    };
    const auto plain = fill("{}");
    ASSERT_EQ(plain.error, "");
    for (const char* kwargs : {R"({"interpolate": false})", R"({"interpolate": null})", R"({"interpolate": 0})", R"({"step": null})"}) {
        EXPECT_EQ(fill(kwargs).error, "") << kwargs;
        EXPECT_EQ(fill(kwargs).trace, plain.trace) << kwargs;
    }
    // (the region then starts and ends on f1: the curves do not cross before the first point)
    EXPECT_EQ(fill(R"({"interpolate": true})").error, "");
    EXPECT_NE(fill(R"({"interpolate": true})").trace, plain.trace);
    EXPECT_EQ(fill(R"({"interpolate": 2})").trace, fill(R"({"interpolate": true})").trace);
    EXPECT_EQ(fill(R"({"interpolate": "yes"})").error, "axs[0].fill_between: interpolate must be true or false");
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
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "kwargs": {"aspect": "equal"}})"), "");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set", "kwargs": {"facecolor": "r"}})"), "axs[0].set: set(facecolor=...) is not supported yet");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_aspect", "args": [-1]})"), "axs[0].set_aspect: aspect must be finite and positive ");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_aspect", "args": [1, "datalim"]})"),
              "axs[0].set_aspect: adjustable 'datalim' is not supported yet");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_aspect", "args": [1], "kwargs": {"anchor": "X"}})"),
              "axs[0].set_aspect: argument must be among C, SW, S, SE, E, NE, N, NW, W");
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
    EXPECT_EQ(error(R"({"call": "plot", "args": [[1, 2]]}, {"call": "waitforbuttonpress"})"), "unsupported call: plt.waitforbuttonpress");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "loglog", "args": [[1, 2]], "kwargs": {"base": 2}})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "loglog", "args": [[1, 2]], "kwargs": {"basex": 2}})").error,
              "axs[0].loglog: LogScale.__init__() got an unexpected keyword argument 'basex'");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_xscale", "args": ["log"], "kwargs": {"base": 1}})").error,
              "axs[0].set_xscale: The log base cannot be <= 0 or == 1");
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

// Axes.fill: (x, y[, colour]) groups as plot reads them; the keywords go to the polygons, whose
// properties are what Polygon.set takes.
TEST(FillCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "fill", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    const std::string one = "[[0, 1, 2], [0, 1, 0]]";
    EXPECT_EQ(error(one, "{}"), "");
    EXPECT_EQ(error("[]", "{}"), "");  // (no polygon)
    EXPECT_EQ(error("[[0, 1, 2], [0, 1, 0], [3, 4], [0, 1]]", R"({"label": "a"})"), "");
    EXPECT_EQ(error(one, R"({"x": [1, 2]})"), "axs[0].fill: got an unexpected keyword argument 'x'");
    EXPECT_EQ(error(one, R"({"y": [1, 2]})"), "axs[0].fill: got an unexpected keyword argument 'y'");
    EXPECT_EQ(error("[[0, 1, 2], [0, 1, 0], [3, 4], [0, 1]]", R"({"label": ["a", "b"]})"),
              "axs[0].fill: plot() with multiple groups of data (i.e., pairs of x and y) does not support multiple labels");
    EXPECT_EQ(error("[[0, 1], [0, 1], [0, 1]]", "{}"), "");  // (x, y, then y alone: two polygons)
    EXPECT_EQ(error("[[0, 1, 2], [0, 1]]", "{}"), "axs[0].fill: x and y must have same first dimension");
    EXPECT_EQ(error(one, R"({"data": [1]})"), "axs[0].fill: data is not supported yet");
    EXPECT_EQ(error(one, R"({"markevery": 2})"), "axs[0].fill: unsupported keyword markevery");
    EXPECT_EQ(error(one, R"({"closed": "yes"})"), "axs[0].fill: closed must be True, False or None");
    EXPECT_EQ(error(one, R"({"c": "r", "color": "g"})").find("axs[0].fill:"), 0u);
    EXPECT_EQ(error(one, R"({"fc": "r", "facecolor": "g"})").find("axs[0].fill:"), 0u);
    EXPECT_EQ(error(one, R"({"facecolor": "nonsense"})").find("axs[0].fill: facecolor is not a colour"), 0u);
    EXPECT_EQ(draw(R"({"call": "fill", "args": [[0, 1, 2], [0, 1, 0]]})").error, "");  // (plt.fill: on the current Axes)
}

// plt.Polygon(xy, closed=True, **kwargs) makes a patch the figure file names; ax.add_patch adds it.
TEST(FillCalls, APolygonIsMadeThenAdded) {
    const std::string tri = R"({"call": "Polygon", "args": [[[0, 0], [1, 1], [1, 0]]], "as": "tri"},)";
    const std::string add = R"({"on": "axs[0]", "call": "add_patch", "args": [{"$ref": "tri"}]})";
    EXPECT_EQ(draw(kSubplots + tri + add).error, "");
    EXPECT_EQ(draw(kSubplots + R"({"call": "Polygon", "args": [[[0, 0], [1, 1]]]})").error, "");  // (made, not added)
    EXPECT_EQ(draw(kSubplots + tri + add + "," + add).error, "axs[0].add_patch: tri is in an Axes already (not supported yet)");
    EXPECT_EQ(draw(kSubplots + R"({"call": "Polygon", "args": [[0, 1, 2]], "as": "p"})").error,
              "plt.Polygon: xy must be a list of (x, y) points");
    EXPECT_EQ(draw(kSubplots + R"({"call": "Polygon", "args": [], "as": "p"})").error, "plt.Polygon: xy is required");
    EXPECT_EQ(draw(kSubplots + R"({"call": "Polygon", "args": [[[0, 0]]], "kwargs": {"markevery": 2}, "as": "p"})").error,
              "plt.Polygon: unsupported keyword markevery");
    EXPECT_EQ(draw(kSubplots + R"({"call": "Polygon", "args": [[[0, 0]]], "as": ["p", "q"]})").error,
              "plt.Polygon returns one object: name it once");
    EXPECT_EQ(draw(kSubplots + R"({"call": "subplots", "as": ["f2", "a2"]},)" + tri +
                   R"({"on": "a2", "call": "add_patch", "args": [{"$ref": "tri"}]})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "add_patch", "args": [1]})").error,
              "axs[0].add_patch: the patch must be one the figure file made (a {\"$ref\": name})");
}

// Axes.annotate(text, xy, xytext=None, xycoords='data', textcoords=None, arrowprops=None,
// annotation_clip=None, **Text properties); arrows are not drawn yet.
TEST(AnnotateCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "annotate", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(error(R"(["a", [1, 2]])", "{}"), "");
    EXPECT_EQ(error(R"(["a"])", R"({"xy": [1, 2], "xytext": [3, 4], "textcoords": "offset points"})"), "");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"xycoords": {"$tuple": ["data", "axes fraction"]}})"), "");
    EXPECT_EQ(error(R"(["a"])", "{}"), "axs[0].annotate: xy is required");
    EXPECT_EQ(error(R"(["a", [1, 2, 3]])", "{}"), "axs[0].annotate: xy must be a pair of numbers");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"arrowprops": {"$tuple": []}})"), "axs[0].annotate: arrowprops must be a dict ({\"$dict\": {...}}) or None");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"xycoords": "offset points"})"), "axs[0].annotate: xycoords cannot be an offset coordinate");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"textcoords": "axes"})"), "axs[0].annotate: 'axes' is not a valid coordinate");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"textcoords": "figure inches"})"), "axs[0].annotate: 'inches' is not a recognized unit");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"xycoords": "polar"})"), "axs[0].annotate: polar coordinates are not supported yet");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"annotation_clip": "yes"})"), "axs[0].annotate: annotation_clip must be True, False or None");
    EXPECT_EQ(error(R"(["a", [1, 2]])", R"({"fontsize": "huge"})").find("axs[0].annotate:"), 0u);
    EXPECT_EQ(draw(kSubplots + R"({"call": "annotate", "args": ["a", [1, 2]]})").error, "");  // (plt.annotate)
}

// Axes.bar_label(container, labels=None, *, fmt='%g', label_type='edge', padding=0, **kwargs): the
// container is a bar call's result, named in the figure file.
TEST(BarCalls, LabelsAreReadAsMatplotlibReadsThem) {
    const std::string bars = R"({"on": "axs[0]", "call": "bar", "args": [[1, 2, 3], [4, 5, 6]], "as": "bars"},)";
    const auto error = [&](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + bars + R"({"on": "axs[0]", "call": "bar_label", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    const std::string ref = R"([{"$ref": "bars"}])";
    EXPECT_EQ(error(ref, "{}"), "");
    EXPECT_EQ(error(R"([{"$ref": "bars"}, ["a", "b", "c"]])", R"({"padding": [1, 2, 3], "fmt": "{:.1f}"})"), "");
    EXPECT_EQ(error(ref, R"({"ha": "left"})"), "axs[0].bar_label: Passing 'ha' to bar_label() is not supported.");
    EXPECT_EQ(error(ref, R"({"verticalalignment": "top"})"), "axs[0].bar_label: Passing 'verticalalignment' to bar_label() is not supported.");
    EXPECT_EQ(error(ref, R"({"label_type": "middle"})"),
              "axs[0].bar_label: 'middle' is not a valid value for label_type; supported values are 'edge', 'center'");
    EXPECT_EQ(error(ref, R"({"padding": [1, 2]})"), "axs[0].bar_label: padding must be of length 3 when passed as a sequence");
    EXPECT_EQ(error(R"([{"$ref": "bars"}, ["a", "b", "c", "d"]])", "{}"),
              "axs[0].bar_label: 'NoneType' object has no attribute 'get_bbox'");
    EXPECT_EQ(error(ref, R"({"fmt": "{1}"})"), "axs[0].bar_label: IndexError: Replacement index 1 out of range for positional args tuple");
    EXPECT_EQ(error(ref, R"({"fmt": "%*d"})"), "axs[0].bar_label: this fmt is not supported yet ('*' widths)");
    EXPECT_EQ(error("[1]", "{}"), "axs[0].bar_label: the container must be a bar call's result (a {\"$ref\": name})");
    EXPECT_EQ(error(ref, R"({"fontsize": "huge"})").find("axs[0].bar_label:"), 0u);
}

// Axes.step (plot with a step draw style) and Axes.stairs (a StepPatch)
TEST(StepCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& call, const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": ")" + call + R"(", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(error("step", "[[1, 2], [3, 4]]", R"({"where": "post", "drawstyle": "default"})"), "");
    EXPECT_EQ(error("step", "[[1, 2], [3, 4]]", R"({"where": "after"})"),
              "axs[0].step: 'after' is not a valid value for where; supported values are 'pre', 'post', 'mid'");
    EXPECT_EQ(error("step", "[[1, 2]]", "{}"), "axs[0].step: x and y are required");
    EXPECT_EQ(error("stairs", "[[1, 2]]", R"({"baseline": [0, 1], "fill": true})"), "");
    EXPECT_EQ(error("stairs", "[[1, 2]]", R"({"edges": [0, 1]})"),
              "axs[0].stairs: Size mismatch between \"values\" and \"edges\". Expected `len(values) + 1 == len(edges)`, but "
              "`len(values) = 2` and `len(edges) = 2`.");
    EXPECT_EQ(error("stairs", "[[1, 2]]", R"({"edges": [0, {"$float": "nan"}, 2]})"), "axs[0].stairs: Nan values in \"edges\" are disallowed");
    EXPECT_EQ(error("stairs", "[[1, 2]]", R"({"baseline": [0, 1, 2]})").find("axs[0].stairs: operands could not be broadcast"), 0u);
    EXPECT_EQ(error("stairs", "[[1, 2]]", R"({"capstyle": "round"})"), "axs[0].stairs: unsupported keyword capstyle");
}

// Axes.stem(*args, linefmt, markerfmt, basefmt, bottom, label, orientation): no other keywords
TEST(StemCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "stem", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(error("[[1, 2]]", "{}"), "");
    EXPECT_EQ(error(R"([[1, 2], [3, 4], "r-"])", R"({"basefmt": "k:", "bottom": 1, "orientation": "horizontal"})"), "");
    EXPECT_EQ(error("[]", "{}"), "axs[0].stem: stem() takes from 1 to 3 positional arguments but 0 were given");
    EXPECT_EQ(error(R"([[1], [2], "r", "b"])", "{}"), "axs[0].stem: stem() takes from 1 to 3 positional arguments but 4 were given");
    EXPECT_EQ(error("[[1, 2]]", R"({"color": "r"})"), "axs[0].stem: got an unexpected keyword argument 'color'");
    EXPECT_EQ(error("[[1, 2]]", R"({"orientation": "up"})"),
              "axs[0].stem: 'up' is not a valid value for orientation; supported values are 'horizontal', 'vertical'");
    EXPECT_EQ(error("[[]]", "{}"), "axs[0].stem: zero-size array to reduction operation minimum which has no identity");
}

// Axes.boxplot: its own parameters only; property dictionaries and bootstrap are not supported yet
TEST(BoxplotCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "boxplot", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    const std::string two = "[[[1, 2, 3, 9], [2, 3, 4]]]";
    EXPECT_EQ(error(two, "{}"), "");
    EXPECT_EQ(error(two, R"({"color": "r"})"), "axs[0].boxplot: got an unexpected keyword argument 'color'");
    EXPECT_EQ(error(two, R"({"positions": [1]})"),
              "axs[0].boxplot: List of boxplot statistics and `positions` values must have same the length");
    EXPECT_EQ(error(two, R"({"tick_labels": ["a"]})"), "axs[0].boxplot: The number of labels (1) must match the number of columns (2).");
    EXPECT_EQ(error(two, R"({"whis": "far"})"), "axs[0].boxplot: whis must be a float or list of percentiles");
    EXPECT_EQ(error(two, R"({"bootstrap": 100})"), "axs[0].boxplot: bootstrap (a random estimate) is not supported");
    EXPECT_EQ(error(two, R"({"orientation": "up"})"),
              "axs[0].boxplot: 'up' is not a valid value for orientation; supported values are 'horizontal', 'vertical'");
    EXPECT_EQ(error(two, R"({"usermedians": [1]})"), "axs[0].boxplot: 'usermedians' and 'x' have different lengths");
    EXPECT_EQ(error(two, R"({"conf_intervals": [[1], null]})"), "axs[0].boxplot: each confidence interval must have two values");
    EXPECT_EQ(error(two, R"({"label": ["a"]})"), "axs[0].boxplot: List of boxplot statistics and `label` values must have same the length");
}

// Axes.violinplot: its own parameters only
TEST(ViolinCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "violinplot", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    const std::string two = "[[[1, 2, 3, 9], [2, 3, 4]]]";
    EXPECT_EQ(error(two, "{}"), "");
    EXPECT_EQ(error(two, R"({"color": "r"})"), "axs[0].violinplot: got an unexpected keyword argument 'color'");
    EXPECT_EQ(error(two, R"({"side": "left"})"),
              "axs[0].violinplot: 'left' is not a valid value for side; supported values are 'both', 'low', 'high'");
    EXPECT_EQ(error(two, R"({"bw_method": "wide"})"), "axs[0].violinplot: `bw_method` should be 'scott', 'silverman', a scalar or a callable");
    EXPECT_EQ(error(two, R"({"quantiles": [0.5]})"),
              "axs[0].violinplot: List of violinplot statistics and quantiles values must have the same length");
    EXPECT_EQ(error(two, R"({"positions": [1]})"),
              "axs[0].violinplot: List of violinplot statistics and `positions` values must have the same length");
    EXPECT_EQ(error(two, R"({"points": -1})"), "axs[0].violinplot: Number of samples, -1, must be non-negative.");
}

// Axes.set_xticks / set_yticks (Axis.set_ticks) and set_frame_on
TEST(TickCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& call) { return draw(kSubplots + call).error; };
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_xticks", "args": [[1, 2], ["a", "b"]]})"), "");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_xticks", "args": [[1, 2], ["a"]]})"),
              "axs[0].set_xticks: The number of FixedLocator locations (2), usually from a call to set_ticks, does not match the "
              "number of labels (1).");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_yticks", "args": [[1, 2]], "kwargs": {"fontsize": 8}})"),
              "axs[0].set_yticks: Incorrect use of keyword argument 'fontsize'. Keyword arguments other than 'minor' modify the text "
              "labels and can only be used if 'labels' are passed as well.");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_yticks", "args": [[1, 2]], "kwargs": {"minor": true}})"),
              "axs[0].set_yticks: minor ticks are not supported yet");
    EXPECT_EQ(error(R"({"on": "axs[0]", "call": "set_frame_on", "args": [0]})"), "");
}

// Axes.pie: its own parameters only; the property dictionaries are not supported yet
TEST(PieCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "pie", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    EXPECT_EQ(error("[[1, 2, 3]]", "{}"), "");
    EXPECT_EQ(error("[[1, -2]]", "{}"), "axs[0].pie: Wedge sizes 'x' must be non negative values");
    EXPECT_EQ(error("[[0, 0]]", "{}"), "axs[0].pie: All wedge sizes are zero");
    EXPECT_EQ(error("[[1, 2]]", R"({"normalize": false})"), "axs[0].pie: Cannot plot an unnormalized pie with sum(x) > 1");
    EXPECT_EQ(error("[[1, 2]]", R"({"labels": ["a"]})"), "axs[0].pie: 'labels' must be of length 'x', not 1");
    EXPECT_EQ(error("[[1, 2]]", R"({"radius": 0})"), "axs[0].pie: 'radius' must be a positive number, not 0.0");
    EXPECT_EQ(error("[[1, 2]]", R"({"color": "r"})"), "axs[0].pie: got an unexpected keyword argument 'color'");
}

TEST(HistCalls, ArgumentsAreReadAsMatplotlibReadsThem) {
    const auto error = [](const std::string& args, const std::string& kwargs) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "hist", "args": )" + args + R"(, "kwargs": )" + kwargs + "}").error;
    };
    const std::string one = "[[1, 2, 2, 3]]", two = "[[[1, 2], [3, 4, 5]]]";
    EXPECT_EQ(error(one, "{}"), "");
    EXPECT_EQ(error("[2.5]", "{}"), "");  // (a number is one value)
    EXPECT_EQ(error(two, R"({"bins": [0, 2, 6], "stacked": true, "density": true, "cumulative": -1})"), "");
    EXPECT_EQ(error(one, R"({"histtype": "step"})"), "");
    EXPECT_EQ(error(two, R"({"histtype": "stepfilled", "stacked": true, "bins": 2, "bottom": [1, 2]})"), "");
    EXPECT_EQ(error(one, R"({"histtype": "step", "bins": 3, "bottom": [1, 2]})"),
              "axs[0].hist: bottom cannot be broadcast to the bins");
    EXPECT_EQ(error(one, R"({"histtype": "steps"})"),
              "axs[0].hist: 'steps' is not a valid value for histtype; supported values are 'bar', 'barstacked', 'step', 'stepfilled'");
    EXPECT_EQ(error(one, R"({"histtype": null})").find("axs[0].hist: that is not a valid value for histtype"), 0u);
    EXPECT_EQ(error(one, R"({"bins": "auto"})"), "");
    EXPECT_EQ(error(two, R"({"bins": "stone"})"), "");
    EXPECT_EQ(error(one, R"({"bins": "bogus"})"), "axs[0].hist: 'bogus' is not a valid estimator for `bins`");
    EXPECT_EQ(error(one, R"({"bins": "fd", "weights": [1, 1, 1, 1]})"),
              "axs[0].hist: Automated estimation of the number of bins is not supported for weighted data");
    EXPECT_EQ(error(one, R"({"bins": 2.5})"), "axs[0].hist: `bins` must be an integer, a string, or an array");
    EXPECT_EQ(error(one, R"({"bins": 0})"), "axs[0].hist: `bins` must be positive, when an integer");
    EXPECT_EQ(error(one, R"({"bins": [3, 1]})"), "axs[0].hist: `bins` must increase monotonically, when an array");
    EXPECT_EQ(error(one, R"({"range": [3, 1]})"), "axs[0].hist: max must be larger than min in range parameter.");
    EXPECT_EQ(error("[[1, {\"$float\": \"inf\"}]]", "{}"), "axs[0].hist: supplied range of [1.0, inf] is not finite");
    EXPECT_EQ(error(one, R"({"weights": [1, 2]})"), "axs[0].hist: weights should have the same shape as x");
    EXPECT_EQ(error(two, R"({"color": ["r", "g", "b"]})"),
              "axs[0].hist: The 'color' keyword argument must have one color per dataset, but 2 datasets and 3 colors were provided");
    EXPECT_EQ(error(two, R"({"hatch": []})"), "axs[0].hist: hatch must not be an empty sequence");
    EXPECT_EQ(error(one, R"({"linestyle": [0, [1, 1]]})"), "axs[0].hist: linestyle given as dashes is not supported yet");
    EXPECT_EQ(error(one, R"({"capstyle": "round"})"), "axs[0].hist: unsupported keyword capstyle");
    EXPECT_EQ(error(one, R"({"ec": "k", "edgecolor": "r"})").find("axs[0].hist:"), 0u);
}

// markevery is kept as given; what matplotlib raises when the line is drawn stops the drawing with its message.
TEST(ArgumentForms, MarkeveryRaisesWhatMatplotlibRaisesWhenTheLineIsDrawn) {
    const auto plot = [](const std::string& every) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "plot", "args": [[1, 2, 3], "o"], "kwargs": {"markevery": )" + every + "}}").error;
    };
    EXPECT_EQ(plot("null"), "");
    EXPECT_EQ(plot(R"({"$tuple": [true, 2]})"), "");
    EXPECT_EQ(plot("false"), "slice step cannot be zero");
    EXPECT_EQ(plot("0"), "slice step cannot be zero");
    EXPECT_EQ(plot(R"({"$tuple": [1, 2, 3]})"), "`markevery` is a tuple but its len is not 2; markevery=(1, 2, 3)");
    EXPECT_EQ(plot(R"({"$tuple": [0.5, 2]})"),
              "`markevery` is a tuple with len 2 and second element is an int, but the first element is not an int; markevery=(0.5, 2)");
    EXPECT_EQ(plot(R"({"$tuple": ["a", 0.5]})"),
              "`markevery` is a tuple with len 2 and second element is a float, but the first element is not a float or an int; markevery=('a', 0.5)");
    EXPECT_EQ(plot(R"({"$tuple": [1, -0.1]})"), "'markevery' step must be positive, but got -0.1");
    EXPECT_EQ(plot(R"({"$tuple": [1, 0.0]})"), "'markevery' step must be positive, but got 0.0");
    EXPECT_EQ(plot("[0, 5]"), "markevery=[0, 5] is iterable but not a valid numpy fancy index");
    EXPECT_EQ(plot("[true, false]"), "markevery=[True, False] is iterable but not a valid numpy fancy index");
    EXPECT_EQ(plot("[0.5]"), "markevery=[0.5] is iterable but not a valid numpy fancy index");
    EXPECT_EQ(plot(R"({"$tuple": [1, "b"]})"), "markevery=(1, 'b') is a tuple with len 2, but its second element is not an int or a float");
    EXPECT_EQ(plot(R"("x")"), "markevery='x' is iterable but not a valid numpy fancy index");
}

// An annotation's arrow styles are checked when it is made, as matplotlib checks them; what the arrow
// cannot be drawn with is raised when it is drawn.
TEST(AnnotateCalls, ArrowStylesAreCheckedAsMatplotlibChecksThem) {
    const auto annotate = [](const std::string& props) {
        return draw(kSubplots + R"({"on": "axs[0]", "call": "annotate", "args": ["t", [0.5, 0.5], [0.1, 0.1]], "kwargs": {"arrowprops": {"$dict": )" +
                    props + "}}}")
            .error;
    };
    EXPECT_EQ(annotate(R"({"arrowstyle": "->"})"), "");
    EXPECT_EQ(annotate(R"({"arrowstyle": "foo"})"), "axs[0].annotate: Unknown style: 'foo'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "->,bogus"})"), "axs[0].annotate: Incorrect style argument: '->,bogus'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "->,head_length=x"})"), "axs[0].annotate: Incorrect style argument: '->,head_length=x'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "->,foo=1"})"),
              "axs[0].annotate: ArrowStyle._Curve.__init__() got an unexpected keyword argument 'foo'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "-,head_length=1"})"),
              "axs[0].annotate: ArrowStyle.Curve.__init__() got an unexpected keyword argument 'head_length'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "->", "connectionstyle": "arc3,rad=1,x=2"})"),
              "axs[0].annotate: ConnectionStyle.Arc3.__init__() got an unexpected keyword argument 'x'");
    EXPECT_EQ(annotate(R"({"arrowstyle": "->", "connectionstyle": "angle3,angleA=0,angleB=180"})"),
              "Given lines do not intersect. Please verify that the angles are not equal or differ by 180 degrees.");
    EXPECT_EQ(annotate(R"({"arrowstyle": "simple", "connectionstyle": "angle"})"), "'path' is not a valid quadratic Bezier curve");
    EXPECT_NE(annotate(R"({"relpos": {"$tuple": [0, 0]}})"), "");
    EXPECT_NE(annotate(R"({"arrowstyle": "->", "shrink": 0.1})"), "");
    EXPECT_NE(annotate(R"({"patchA": "box"})"), "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "annotate", "args": ["t", [0.5, 0.5]], "kwargs": {"arrowprops": [1]}})").error,
              "axs[0].annotate: arrowprops must be a dict ({\"$dict\": {...}}) or None");
}

// Dates ({"$datetime": ...}) are taken where matplotlib's units reach the data of the calls ported
// with them; anywhere else they are refused, as are lists that mix dates with numbers.
class DateRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(DateRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, DateRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "bar", "args": [[{"$datetime": "2020-01-01T00:00:00"}], [1]]})",
                "axs[0].bar: dates are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "errorbar", "args": [[{"$datetime": "2020-01-01T00:00:00"}], [1]]})",
                "axs[0].errorbar: dates are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "text", "args": [{"$datetime": "2020-01-01T00:00:00"}, 1, "a"]})",
                "axs[0].text: dates are not supported yet"},
        Refusal{R"({"on": "fig", "call": "text", "args": [{"$datetime": "2020-01-01T00:00:00"}, 1, "a"]})",
                "fig.text: dates are not supported yet"},
        Refusal{R"({"call": "bar", "args": [[{"$datetime": "2020-01-01T00:00:00"}], [1]]})",
                "plt.bar: dates are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "set", "kwargs": {"xlim": [{"$datetime": "2020-01-01T00:00:00"}, {"$datetime": "2020-01-02T00:00:00"}]}})",
                "axs[0].set: dates are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, 2], [1, 2]], "kwargs": {"linewidth": {"$datetime": "2020-01-01T00:00:00"}}})",
                "axs[0].plot: dates are not supported yet outside the data"},
        Refusal{R"({"on": "axs[0]", "call": "scatter", "args": [[1], [1], [{"$datetime": "2020-01-01T00:00:00"}]]})",
                "axs[0].scatter: dates are not supported yet outside the data"},
        Refusal{R"({"on": "axs[0]", "call": "axvline", "args": [1], "kwargs": {"ymin": {"$datetime": "2020-01-01T00:00:00"}}})",
                "axs[0].axvline: dates are not supported yet outside the data"},
        Refusal{R"({"on": "axs[0]", "call": "axhspan", "args": [0, 1, {"$datetime": "2020-01-01T00:00:00"}]})",
                "axs[0].axhspan: dates are not supported yet outside the data"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[{"$datetime": "2020-01-01T00:00:00"}, 2], [1, 2]]})",
                "axs[0].plot: a list that mixes dates with numbers is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[1, {"$datetime": "2020-01-01T00:00:00"}], [1, 2]]})",
                "axs[0].plot: a list that mixes dates with numbers is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "axvspan", "args": [1, {"$datetime": "2020-01-01T00:00:00"}]})",
                "axs[0].axvspan: a list that mixes dates with numbers is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "set_xlim", "args": [{"$datetime": "2020-01-01T00:00:00"}, 18000]})",
                "axs[0].set_xlim: a list that mixes dates with numbers is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "fill_between", "args": [[1, 2], [{"$datetime": "2020-01-01T00:00:00"}, {"$float": "nan"}]]})",
                "axs[0].fill_between: a list that mixes dates with numbers is not supported yet"},
        // num2date of a tick before year 1 (matplotlib raises it when the axis is drawn)
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[{"$datetime": "0001-06-01T00:00:00"}, {"$datetime": "0900-01-01T00:00:00"}], [1, 2]]})",
                "Date ordinal -735421.1 converts to a date outside years 0001 to 9999 (using epoch 1970-01-01T00:00:00), "
                "but Matplotlib dates must be between year 0001 and 9999."}));

// The DateConverter's settings that are not ported: the concise converter, another epoch, a time zone.
TEST(DateCalls, RcSettingsNotPortedAreRefused) {
    for (const auto& [rc, error] : std::vector<std::pair<std::string, std::string>>{
             {R"("date.converter": "concise")", "rcParams[\"date.converter\"] is not supported yet"},
             {R"("timezone": "Europe/Madrid")", "rcParams[\"timezone\"] is not supported yet"},
             {R"("date.epoch": "2000-01-01T00:00:00")", "a date is not supported yet"}}) {
        std::string why;
        const auto figure = calculate_plot::model::parseFigure(R"({"version": 1, "rc": {)" + rc + R"(}, "calls": []})", &why);
        if (!figure) {
            EXPECT_NE(why.find(error), std::string::npos) << why;
            continue;
        }
        calculate_plot::engine::DrawOptions options;
        options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
        const auto drawing = calculate_plot::engine::drawAgg(*figure, options);
        EXPECT_NE(drawing.error.find(error), std::string::npos) << drawing.error;
    }
    // their defaults are taken
    std::string why;
    const auto figure = calculate_plot::model::parseFigure(
        R"({"version": 1, "rc": {"date.converter": "auto", "timezone": "UTC", "date.epoch": "1970-01-01T00:00:00"}, "calls": [)" +
            kSubplots.substr(0, kSubplots.size() - 1) + "]}",
        &why);
    ASSERT_TRUE(figure.has_value()) << why;
    calculate_plot::engine::DrawOptions options;
    options.fontDirectory = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";
    EXPECT_EQ(calculate_plot::engine::drawAgg(*figure, options).error, "");
}

// set_xscale('function' / 'functionlog', functions=(forward, inverse)): functions as {"$lambda": ...}
INSTANTIATE_TEST_SUITE_P(
    FunctionScales, DateRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "set_xscale", "args": ["function"]})",
                "axs[0].set_xscale: the function scale needs functions=(forward, inverse)"},
        Refusal{R"({"on": "axs[0]", "call": "set_yscale", "args": ["functionlog"], "kwargs": {"base": 2}})",
                "axs[0].set_yscale: the functionlog scale needs functions=(forward, inverse)"},
        Refusal{R"({"on": "axs[0]", "call": "set_xscale", "args": ["function"], "kwargs": {"functions": [{"$lambda": "x"}]}})",
                "axs[0].set_xscale: functions must be (forward, inverse), two functions"},
        Refusal{R"({"on": "axs[0]", "call": "set_xscale", "args": ["function"], "kwargs": {"functions": [1, 2]}})",
                "axs[0].set_xscale: functions other than {\"$lambda\": ...} are not supported yet"},
        Refusal{R"j({"on": "axs[0]", "call": "set_xscale", "args": ["function"], "kwargs": {"functions": [{"$lambda": "x * (1 / 0)"}, {"$lambda": "x"}]}})j",
                "axs[0].set_xscale: ZeroDivisionError: division by zero"},
        Refusal{R"({"on": "axs[0]", "call": "set_xscale", "args": ["functionlog"], "kwargs": {"functions": [{"$lambda": "x"}, {"$lambda": "x"}], "base": 1}})",
                "axs[0].set_xscale: The log base cannot be <= 0 or == 1"},
        Refusal{R"({"on": "axs[0]", "call": "set_xscale", "args": ["function"], "kwargs": {"functions": [{"$lambda": "x"}, {"$lambda": "x"}], "base": 2}})",
                "axs[0].set_xscale: the function scale's option base is not supported yet"}));

// secondary_xaxis / secondary_yaxis: what matplotlib refuses, and what is not ported
INSTANTIATE_TEST_SUITE_P(
    SecondaryAxes, DateRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["left"]})",
                "axs[0].secondary_xaxis: secondary_xaxis location must be either a float or \"top\"/\"bottom\""},
        Refusal{R"({"on": "axs[0]", "call": "secondary_yaxis", "args": ["top"]})",
                "axs[0].secondary_yaxis: secondary_yaxis location must be either a float or \"left\"/\"right\""},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top", [{"$lambda": "x"}, {"$lambda": "x"}]]})",
                "axs[0].secondary_xaxis: functions argument of secondary Axes must be a two-tuple of callable functions with the "
                "first function being the transform and the second being the inverse"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top"], "kwargs": {"functions": {"$tuple": [{"$lambda": "x"}]}}})",
                "axs[0].secondary_xaxis: functions argument of secondary Axes must be a two-tuple of callable functions with the "
                "first function being the transform and the second being the inverse"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top"], "kwargs": {"transform": "ax.transData"}})",
                "axs[0].secondary_xaxis: a transform other than None is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top"], "kwargs": {"facecolor": "r"}})",
                "axs[0].secondary_xaxis: facecolor is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top"], "as": "s"}, {"on": "s", "call": "plot", "args": [[1, 2]]})",
                "s.plot: plot on a secondary axis is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_xaxis", "args": ["top"], "as": "s"}, {"on": "s", "call": "set_yticks", "args": [[1, 2]]})",
                "s.set_yticks: Cannot set yticks on a secondary x-axis"},
        Refusal{R"({"on": "axs[0]", "call": "secondary_yaxis", "args": ["left"], "as": "s"}, {"on": "s", "call": "set_alignment", "args": ["top"]})",
                "s.set_alignment: 'top' is not a valid value for align; supported values are 'left', 'right'"},
        // the limits through the function: NaN at the limits with margins (-0.05, 1.05) when the Axes is drawn
        Refusal{R"j({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1]]}, {"on": "axs[0]", "call": "secondary_yaxis", "args": ["right", {"$tuple": [{"$lambda": "np.sqrt(x)"}, {"$lambda": "x ** 2"}]}]})j",
                "Axis limits cannot be NaN or Inf"}));

// Layout engines: what Figure and set_layout_engine refuse, and what is not ported
class LayoutEngineRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(LayoutEngineRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, LayoutEngineRefusals,
    testing::Values(
        Refusal{R"({"call": "subplots", "kwargs": {"layout": "loose"}, "as": ["fig", "ax"]})",
                "plt.subplots: Invalid value for 'layout': 'loose'"},
        Refusal{R"({"call": "figure", "kwargs": {"layout": 3}})", "plt.figure: a layout engine object is not supported yet"},
        Refusal{R"({"call": "subplots", "kwargs": {"constrained_layout": "yes"}, "as": ["fig", "ax"]})",
                "plt.subplots: constrained_layout must be True, False or a dict"},
        Refusal{R"({"call": "subplots", "kwargs": {"constrained_layout": {"$dict": {"pad": 1}}}, "as": ["fig", "ax"]})",
                "plt.subplots: got an unexpected keyword argument 'pad'"},
        Refusal{R"({"call": "subplots", "kwargs": {"tight_layout": {"$dict": {"hspace": 1}}}, "as": ["fig", "ax"]})",
                "plt.subplots: got an unexpected keyword argument 'hspace'"},
        Refusal{R"({"call": "subplots", "kwargs": {"layout": "constrained"}, "as": ["fig", "ax"]}, {"on": "fig", "call": "set_layout_engine", "args": ["constrained"], "kwargs": {"h_pad": "big"}})",
                "fig.set_layout_engine: h_pad must be a finite number"},
        Refusal{R"({"call": "subplots", "args": [1, 2], "kwargs": {"width_ratios": [1, 2, 3]}, "as": ["fig", "axs"]})",
                "plt.subplots: Expected the given number of width ratios to match the number of columns of the grid"},
        Refusal{R"({"call": "subplots", "args": [2, 1], "kwargs": {"height_ratios": [1, -2]}, "as": ["fig", "axs"]})",
                "plt.subplots: height ratios must be positive numbers"}));

// A figure with the constrained layout ignores subplots_adjust; tight_layout() replaces the engine
TEST(LayoutEngineCalls, SubplotsAdjustIsIgnoredUnderTheConstrainedLayout) {
    EXPECT_EQ(draw(R"({"call": "subplots", "kwargs": {"layout": "constrained"}, "as": ["fig", "ax"]},
                     {"on": "fig", "call": "subplots_adjust", "kwargs": {"left": 0.9, "right": 0.1}})")
                  .error,
              "");
    EXPECT_EQ(draw(R"({"call": "subplots", "kwargs": {"layout": "tight"}, "as": ["fig", "ax"]},
                     {"on": "fig", "call": "subplots_adjust", "kwargs": {"left": 0.9, "right": 0.1}})")
                  .error,
              "fig.subplots_adjust: left cannot be >= right");
}

// Twinned Axes take adjustable 'datalim', where an aspect changes the data limits: not ported
TEST(TwinCalls, AspectsOfTwinsAreRefused) {
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "set_aspect", "args": [2]}, {"on": "axs[0]", "call": "twinx"})").error,
              "axs[0].twinx: twins of an Axes with an aspect (adjustable 'datalim') are not supported yet");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "pie", "args": [[1, 2]]}, {"on": "axs[0]", "call": "twiny"})").error,
              "axs[0].twiny: twins of an Axes with an aspect (adjustable 'datalim') are not supported yet");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "twinx"}, {"on": "axs[0]", "call": "pie", "args": [[1, 2]]})").error,
              "axs[0].pie: a pie on twinned Axes (adjustable 'datalim') is not supported yet");
}

// contour raises what matplotlib raises, and refuses what is not ported
class ContourRefusals : public testing::TestWithParam<Refusal> {};

TEST_P(ContourRefusals, NameTheCallAndTheReason) {
    const auto drawing = draw(kSubplots + GetParam().call);
    EXPECT_EQ(drawing.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    Engine, ContourRefusals,
    testing::Values(
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]], [2, 1]]})",
                "axs[0].contour: Contour levels must be increasing"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"levels": [1, 1]}})",
                "axs[0].contour: Contour levels must be increasing"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"colors": "k", "cmap": "viridis"}})",
                "axs[0].contour: Either colors or cmap must be None"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"origin": "left"}})",
                "axs[0].contour: 'left' is not a valid value for origin. Supported values are None, 'lower', 'upper', 'image'"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"extent": [0, 1]}})",
                "axs[0].contour: If given, 'extent' must be None or (x0, x1, y0, y1)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"color": "k"}})",
                "axs[0].contour: ContourSet.__init__() got an unexpected keyword argument 'color'"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"algorithm": "foo"}})",
                "axs[0].contour: 'foo' is not a valid value for contour.algorithm; supported values are ['mpl2005', 'mpl2014', 'serial', 'threaded']"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"algorithm": "serial"}})",
                "axs[0].contour: the serial contour algorithm is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[1, 2, 3]]})", "axs[0].contour: Input z must be 2D, not 1D"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[1, 2, 3]]]})",
                "axs[0].contour: Input z must be at least a (2, 2) shaped array, but has shape (1, 3)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[1, 2], [[1, 2], [1, 2]], [[0, 1], [2, 3]]]})",
                "axs[0].contour: Number of dimensions of x (1) and y (2) do not match"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[1, 2, 3], [1, 2], [[0, 1], [2, 3]]]})",
                "axs[0].contour: Length of x (3) must match number of columns in z (2)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[1, 2], [1, 2, 3], [[0, 1], [2, 3]]]})",
                "axs[0].contour: Length of y (3) must match number of rows in z (2)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[1, 2, 3], [1, 2, 3]], [[1, 2], [1, 2]], [[0, 1], [2, 3]]]})",
                "axs[0].contour: Shapes of x (2, 3) and z (2, 2) do not match"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[1, 2], [1, 2]], [[1, 2], [1, 2], [1, 2]], [[0, 1], [2, 3]]]})",
                "axs[0].contour: Shapes of y (3, 2) and z (2, 2) do not match"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]], 3, 4, 5, 6]})",
                "axs[0].contour: contour() takes from 1 to 4 positional arguments but 5 were given"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"nchunk": -1}})",
                "axs[0].contour: chunk_size cannot be negative"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"linewidths": []}})",
                "axs[0].contour: division by zero"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"linestyles": []}})",
                "axs[0].contour: division by zero"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"linestyles": "wavy"}})",
                "axs[0].contour: linestyles other than 'solid', 'dashed', 'dashdot' and 'dotted' are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"locator": 3}})",
                "axs[0].contour: locator is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"levels": []}})",
                "axs[0].contour: contours without levels are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"levels": -1}})",
                "axs[0].contour: levels as an integer must be from 0 to 1000 (others are not supported yet)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[{"$float": "nan"}, 1], [2, {"$float": "nan"}]], [[0, 1], [2, 3]], [[0, 1], [2, 3]]]})",
                "axs[0].contour: contours of x and y that are not finite are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[{"$float": "nan"}, {"$float": "nan"}], [{"$float": "nan"}, {"$float": "nan"}]]]})",
                "axs[0].contour: contours of a z without a valid value are not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "as": ["a", "b"]})",
                "axs[0].contour: a contour set is one object (\"as\" names one)"},
        Refusal{R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"rasterized": true}})",
                "axs[0].contour: rasterized is not supported yet"},
        Refusal{R"({"on": "axs[0]", "call": "contourf", "args": [[[0, 1], [2, 3]], [1]]})",
                "axs[0].contourf: Filled contours require at least 2 levels."},
        Refusal{R"({"on": "axs[0]", "call": "contourf", "args": [[[0, 1], [2, 3]]], "kwargs": {"hatches": null}})",
                "axs[0].contourf: 'NoneType' object is not iterable"},
        Refusal{R"({"on": "axs[0]", "call": "contourf", "args": [[[0, 1], [2, 3]]], "kwargs": {"hatches": ["/", 3]}})",
                "axs[0].contourf: 'int' object is not iterable"},
        Refusal{R"({"on": "axs[0]", "call": "contourf", "args": [[[0, 1], [2, 3]], 1, 2, 3, 4]})",
                "axs[0].contourf: contourf() takes from 1 to 4 positional arguments but 5 were given"}));

// contour lines never look at hatches, whatever they are
TEST(ContourCalls, LinesIgnoreHatches) {
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"hatches": null}})").error, "");
    EXPECT_EQ(draw(kSubplots + R"({"on": "axs[0]", "call": "contour", "args": [[[0, 1], [2, 3]]], "kwargs": {"hatches": 5}})").error, "");
}
