// The transform chain of an Axes, ported: 400 recorded cases from matplotlib give the same
// matrices, boxes and transformed points, bit for bit.
#include "support/json.hpp"
#include "transforms/transforms.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace t = calculate_plot::transforms;
using calculate_plot::support::Json;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

std::vector<double> hexes(const Json& j) {
    std::vector<double> out;
    for (const auto& x : j.items()) out.push_back(hex(x));
    return out;
}

std::vector<double> flat(const t::Matrix& m) { return {m.sx, m.shx, m.tx, m.shy, m.sy, m.ty}; }

}  // namespace

TEST(Transforms, TheAxesChainMatchesMatplotlib) {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/transforms.json");
    std::ostringstream text;
    text << in.rdbuf();
    const auto cases = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(cases.has_value());
    int n = 0, inverseMismatches = 0;
    for (const auto& c : cases->items()) {
        const auto figsize = hexes(*c.find("figsize"));
        const double dpi = hex(*c.find("dpi"));
        const auto pos = hexes(*c.find("position"));
        const auto view = hexes(*c.find("view"));
        auto dpiScale = std::make_shared<t::Affine2D>();
        dpiScale->scale(dpi);
        auto figBbox = std::make_shared<t::TransformedBbox>(t::Bbox::fromBounds(0, 0, figsize[0], figsize[1]), dpiScale);
        auto transFigure = std::make_shared<t::BboxTransformTo>(figBbox);
        auto axBbox = std::make_shared<t::TransformedBbox>(t::Bbox::fromBounds(pos[0], pos[1], pos[2], pos[3]), transFigure);
        auto transAxes = std::make_shared<t::BboxTransformTo>(axBbox);
        auto transScale = std::make_shared<t::IdentityTransform>();
        auto viewLim = t::Bbox::fromExtents(view[0], view[1], view[2], view[3]);
        auto transLimits = std::make_shared<t::BboxTransformFrom>(std::make_shared<t::TransformedBbox>(viewLim, transScale));
        auto transData = t::compose(transScale, t::compose(transLimits, transAxes));
        auto xaxis = t::blend(transData, transAxes);
        auto yaxis = t::blend(transAxes, transData);
        auto tickLabel = t::compose(xaxis, std::make_shared<t::ScaledTranslation>(0, -3.5 / 72, dpiScale));
        const auto fb = figBbox->points(), ab = axBbox->points();
        EXPECT_EQ(std::vector<double>(fb.begin(), fb.end()), hexes(*c.find("fig_bbox"))) << n;
        EXPECT_EQ(std::vector<double>(ab.begin(), ab.end()), hexes(*c.find("ax_bbox"))) << n;
        EXPECT_EQ(flat(transFigure->matrix()), hexes(*c.find("transFigure"))) << n;
        EXPECT_EQ(flat(transAxes->matrix()), hexes(*c.find("transAxes"))) << n;
        EXPECT_EQ(flat(transLimits->matrix()), hexes(*c.find("transLimits"))) << n;
        EXPECT_EQ(flat(transData->matrix()), hexes(*c.find("transData"))) << n;
        EXPECT_EQ(flat(xaxis->matrix()), hexes(*c.find("xaxis"))) << n;
        EXPECT_EQ(flat(yaxis->matrix()), hexes(*c.find("yaxis"))) << n;
        EXPECT_EQ(flat(tickLabel->matrix()), hexes(*c.find("tickLabel"))) << n;
        const auto point = hexes(*c.find("point"));
        const auto pd = transData->transform(point[0], point[1]);
        EXPECT_EQ(std::vector<double>(pd.begin(), pd.end()), hexes(*c.find("pointData"))) << n;
        const auto pi = transData->inverted()->transform(point[0], point[1]);
        if (std::vector<double>(pi.begin(), pi.end()) != hexes(*c.find("pointInverse"))) ++inverseMismatches;
        ++n;
    }
    EXPECT_EQ(n, 400);
    EXPECT_EQ(inverseMismatches, 0) << "numpy.linalg.inv (LAPACK) and the port's LU differ in the last bits";
}

TEST(Transforms, CompositionDropsIdentities) {
    auto a = std::make_shared<t::Affine2D>();
    a->scale(2.0);
    auto id = std::make_shared<t::IdentityTransform>();
    EXPECT_EQ(t::compose(id, a), a);
    EXPECT_EQ(t::compose(a, id), a);
}
