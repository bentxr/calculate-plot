// Saving changes the figure only for the save (its dpi, its colours, its box when the bounding box
// is tight): afterwards the figure is as it was, as after matplotlib's Figure.savefig.
#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

using namespace calculate_plot::engine;

namespace {

struct State {
    double dpi;
    std::array<double, 4> bbox, bboxInches, boxout, patchBounds, axesBbox;
    Patch::Colors patch, axesPatch;
};

State stateOf(const Figure& figure) {
    const Rectangle& p = *figure.patch;
    return {figure.dpi,
            figure.bbox->points(),
            figure.bboxInches->points(),
            figure.transFigure->boxout()->points(),
            {p.x0, p.y0, p.width, p.height},
            figure.axes[0]->bbox->points(),
            p.colorState(),
            figure.axes[0]->patch->colorState()};
}

void expectSame(const State& a, const State& b) {
    EXPECT_EQ(a.dpi, b.dpi);
    EXPECT_EQ(a.bbox, b.bbox);
    EXPECT_EQ(a.bboxInches, b.bboxInches);
    EXPECT_EQ(a.boxout, b.boxout);
    EXPECT_EQ(a.patchBounds, b.patchBounds);
    EXPECT_EQ(a.axesBbox, b.axesBbox);
    for (const auto& [x, y] : {std::pair{a.patch, b.patch}, std::pair{a.axesPatch, b.axesPatch}}) {
        EXPECT_EQ(x.originalFace, y.originalFace);
        EXPECT_EQ(x.originalEdge, y.originalEdge);
        EXPECT_EQ(x.face, y.face);
        EXPECT_EQ(x.edge, y.edge);
    }
}

calculate_plot::rc::Params saveRc() {
    auto rc = calculate_plot::rc::Params::defaults();
    const auto set = [&](const char* key, const char* json) {
        std::string error;
        EXPECT_TRUE(rc.set(key, *calculate_plot::support::parseJson(json, nullptr), &error)) << error;
    };
    set("savefig.dpi", "200");
    set("savefig.bbox", "\"tight\"");
    set("savefig.facecolor", "\"#ddeeff\"");
    set("savefig.edgecolor", "\"red\"");
    set("savefig.transparent", "true");
    return rc;
}

const std::string kFonts = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

TEST(Savefig, TheFigureIsAsItWasAfterAPng) {
    const auto rc = saveRc();
    Figure figure(rc, {});
    figure.subplots(1, 1);
    const State before = stateOf(figure);
    std::string error;
    const std::string trace = drawAggTrace(figure, "C.UTF-8", kFonts, &error);
    EXPECT_EQ(error, "");
    EXPECT_NE(trace.find("\"dpi\":200"), std::string::npos) << "the save itself was at 200 dpi";
    expectSame(stateOf(figure), before);
    // and so a second save writes the same
    EXPECT_EQ(drawAggTrace(figure, "C.UTF-8", kFonts, &error), trace);
}

TEST(Savefig, TheFigureIsAsItWasAfterAnSvg) {
    const auto rc = saveRc();
    Figure figure(rc, {});
    figure.subplots(1, 1);
    const State before = stateOf(figure);
    std::string error;
    const std::string trace = drawSvgTrace(figure, "C.UTF-8", kFonts, {{"Creator", "calculate-plot"}, {"Date", std::nullopt}}, "salt", &error);
    EXPECT_EQ(error, "");
    expectSame(stateOf(figure), before);
    EXPECT_EQ(drawSvgTrace(figure, "C.UTF-8", kFonts, {{"Creator", "calculate-plot"}, {"Date", std::nullopt}}, "salt", &error), trace);
}

}  // namespace
