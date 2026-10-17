// Subplot positions equal matplotlib 3.11.2's: GridSpec.get_grid_positions and
// SubplotSpec.get_position for 400 random grids (tests/oracle/gridspec.json).
#include "engine/gridspec.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::engine;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

// Which numbers are Python ints (written without a point or exponent).
std::vector<bool> integers(const Json& j) {
    std::vector<bool> out;
    for (const Json& v : j.items()) out.push_back(v.numberText().find_first_of(".eE") == std::string::npos);
    return out;
}

std::vector<double> numbers(const Json& j) {
    std::vector<double> out;
    for (const Json& v : j.items()) out.push_back(v.asNumber());
    return out;
}

TEST(GridSpec, PositionsEqualMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/gridspec.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    const auto rc = calculate_plot::rc::Params::defaults();
    int index = 0;
    for (const Json& c : cases.items()) {
        SCOPED_TRACE("grid " + std::to_string(index++));
        SubplotParams params = SubplotParams::defaults(rc);
        const Json& p = *c.find("subplotpars");
        if (p.find("left")) {
            std::string error;
            ASSERT_TRUE(params.update(p.find("left")->asNumber(), p.find("bottom")->asNumber(),
                                      p.find("right")->asNumber(), p.find("top")->asNumber(), std::nullopt,
                                      std::nullopt, &error))
                << error;
        }
        GridSpec gs(int(c.find("nrows")->asInteger()), int(c.find("ncols")->asInteger()));
        const Json& kw = *c.find("gridspec");
        if (const Json* w = kw.find("wspace")) gs.wspace = w->asNumber();
        if (const Json* h = kw.find("hspace")) gs.hspace = h->asNumber();
        if (const Json* r = kw.find("width_ratios")) gs.widthRatios = numbers(*r), gs.widthRatiosInt = integers(*r);
        if (const Json* r = kw.find("height_ratios")) gs.heightRatios = numbers(*r), gs.heightRatiosInt = integers(*r);
        for (const Json& s : c.find("spans")->items()) {
            const SubplotSpec spec = gs.span(int((*s.find("rows"))[0].asInteger()), int((*s.find("rows"))[1].asInteger()),
                                             int((*s.find("cols"))[0].asInteger()), int((*s.find("cols"))[1].asInteger()));
            const auto e = spec.position(params)->points();
            for (int i = 0; i < 4; ++i) EXPECT_EQ(e[i], hex((*s.find("extents"))[i])) << i;
        }
    }
    EXPECT_EQ(index, 400);
}

TEST(GridSpec, RefusesCrossedMargins) {
    SubplotParams params = SubplotParams::defaults(calculate_plot::rc::Params::defaults());
    std::string error;
    EXPECT_FALSE(params.update(0.9, std::nullopt, 0.1, std::nullopt, std::nullopt, std::nullopt, &error));
    EXPECT_EQ(error, "left cannot be >= right");
    EXPECT_FALSE(params.update(std::nullopt, 0.5, std::nullopt, 0.5, std::nullopt, std::nullopt, &error));
    EXPECT_EQ(error, "bottom cannot be >= top");
    EXPECT_EQ(params.left, 0.125);
}

}  // namespace
