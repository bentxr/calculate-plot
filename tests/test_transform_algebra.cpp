// How transforms compare and subtract equals matplotlib 3.11.2 (tests/oracle/transform_algebra.json):
// equality is partly by value, so the transforms of an Axes are equal or not depending on its view
// limits, and A - B takes the shortcuts that equality allows. Every ordered pair of an Axes'
// transforms, in seven states of the Axes.
#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::engine;
namespace transforms = calculate_plot::transforms;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

void prepare(Axes& ax, const std::string& state, const calculate_plot::rc::Params& rc) {
    const auto plot = [&] {
        ax.addLine(std::make_unique<Line2D>(rc, std::vector<double>{1, 2, 3}, std::vector<double>{10, 20, 15}));
        ax.requestAutoscaleView(true, true);
    };
    if (state == "xlim" || state == "both") ax.xaxis->setLim(2, 5, false);
    if (state == "ylim" || state == "both") ax.yaxis->setLim(-1, 3, false);
    if (state == "data" || state == "log") plot();
    if (state == "log") ax.setScale(false, "log");
    if (state == "unit again") {
        ax.xaxis->setLim(2, 5, false);
        ax.xaxis->setLim(0, 1, false);
    }
    ax.unstaleViewLim();  // reading Axes.viewLim
}

TEST(TransformAlgebra, ComparesAndSubtractsAsMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/transform_algebra.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    const auto rc = calculate_plot::rc::Params::defaults();
    const Json& r = *oracle->find("rectangle");
    ASSERT_EQ(oracle->find("states")->size(), 7u);
    int equalPairs = 0;
    for (const Json& state : oracle->find("states")->items()) {
        const std::string name = state.find("state")->asString();
        Figure figure(rc, {});
        Axes& ax = *figure.subplots(1, 1)[0];
        prepare(ax, name, rc);
        std::map<std::string, transforms::TransformPtr> t{
            {"transData", ax.transData},     {"transAxes", ax.transAxes}, {"transScale", ax.transScale},
            {"transLimits", ax.transLimits}, {"xaxis", ax.xaxisTransform}, {"yaxis", ax.yaxisTransform}};
        std::vector<std::unique_ptr<Rectangle>> rectangles;
        for (const Json& over : oracle->find("over")->items()) {
            rectangles.push_back(std::make_unique<Rectangle>(rc, r[0].asNumber(), r[1].asNumber(), r[2].asNumber(), r[3].asNumber()));
            rectangles.back()->transform = t.at(over.asString());
            t["rectangle+" + over.asString()] = rectangles.back()->fullTransform();
        }
        auto scale = std::make_shared<transforms::Affine2D>(), shift = std::make_shared<transforms::Affine2D>();
        auto both = std::make_shared<transforms::Affine2D>();
        scale->scale(2, 3);
        shift->translate(1, 1);
        both->scale(2, 3).translate(1, 1);
        t["scale+shift"] = transforms::compose(scale, shift);
        t["scaled, shifted"] = both;
        t["identity"] = std::make_shared<transforms::IdentityTransform>();
        // Affine2D.from_values(a, b, c, d, e, f) is [[a, c, e], [b, d, f]]
        t["shifted in y"] = std::make_shared<transforms::Affine2D>(transforms::Matrix{2, 0, 1, 0, 3, 2});
        t["sheared in x"] = std::make_shared<transforms::Affine2D>(transforms::Matrix{2, 0.5, 1, 0, 3, 1});
        t["sheared in y"] = std::make_shared<transforms::Affine2D>(transforms::Matrix{2, 0, 1, 0.5, 3, 1});
        rectangles.push_back(std::make_unique<Rectangle>(rc, 0.0, 0.0, 1.0, 1.0));
        rectangles.back()->transform = t.at("identity");
        t["unit rectangle"] = rectangles.back()->fullTransform();
        ASSERT_EQ(state.find("pairs")->size(), 289u);
        for (const Json& pair : state.find("pairs")->items()) {
            const std::string a = pair.find("a")->asString(), b = pair.find("b")->asString();
            SCOPED_TRACE(name + ": " + a + " and " + b);
            const auto& A = t.at(a);
            const auto& B = t.at(b);
            EXPECT_EQ(transforms::equal(*A, *B), pair.find("equal")->asBool());
            EXPECT_EQ(A->containsBranch(*B), pair.find("contains")->asBool());
            const auto separately = transforms::containsBranchSeparately(A, B);
            EXPECT_EQ(separately[0], (*pair.find("separately"))[0].asBool());
            EXPECT_EQ(separately[1], (*pair.find("separately"))[1].asBool());
            EXPECT_EQ(A->depth(), int(pair.find("depth")->asNumber()));
            equalPairs += a != b && pair.find("equal")->asBool();
            if (const Json* minus = pair.find("minus")) {
                const auto difference = transforms::subtract(A, B);
                for (std::size_t i = 0; i < 3; ++i) {
                    const Json& p = (*oracle->find("points"))[i];
                    const auto got = difference->transform(p[0].asNumber(), p[1].asNumber());
                    EXPECT_EQ(got[0], hex((*minus)[i][0])) << "x of point " << i;
                    EXPECT_EQ(got[1], hex((*minus)[i][1])) << "y of point " << i;
                }
            }
        }
    }
    EXPECT_EQ(equalPairs, 81) << "different transforms that are equal by value";
}

}  // namespace
