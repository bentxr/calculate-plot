// What the engine answers to calls it cannot run: an error that names the call and says why, never
// a figure drawn differently from matplotlib's.
#include "engine/engine.hpp"
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
        Refusal{R"({"on": "axs[0]", "call": "plot", "args": [[0, 1], [0, 1000000]]},
                   {"on": "axs[0]", "call": "set_title", "args": ["t"]},
                   {"on": "fig", "call": "tight_layout"})",
                "fig.tight_layout: titles beside a y offset text are not supported yet"}));

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
              "rc savefig.dpi: the value given for savefig.dpi is not of the right kind");
    EXPECT_EQ(error(R"({"savefig.dpi": "screen"})"), "rc savefig.dpi must be 'figure' or a number");
    EXPECT_EQ(error(R"({"savefig.bbox": "loose"})"), "rc savefig.bbox must be 'tight', 'standard' or None");
    EXPECT_EQ(error(R"({"savefig.bbox": 1})"), "rc savefig.bbox must be 'tight', 'standard' or None");
    EXPECT_EQ(error(R"({"savefig.facecolor": "not a colour"})"), "rc savefig.facecolor is not a colour");
    EXPECT_EQ(error(R"({"savefig.edgecolor": "not a colour"})"), "rc savefig.edgecolor is not a colour");
    EXPECT_EQ(error(R"({"savefig.bbox": null, "savefig.dpi": 50.5, "savefig.facecolor": "auto", "savefig.edgecolor": "C1"})"), "");
}

}  // namespace
