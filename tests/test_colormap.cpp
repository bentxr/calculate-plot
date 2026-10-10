// Colour mapping as matplotlib 3.11.2's ScalarMappable(Normalize(), 'viridis').to_rgba, for 300
// random data sets (tests/oracle/colormapping.json).
#include "colors/colormap.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::colors;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

TEST(Colormap, MapsAsMatplotlib) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/colormapping.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json cases = *calculate_plot::support::parseJson(text.str(), nullptr);
    const auto viridis = ListedColormap::named("viridis");
    ASSERT_TRUE(viridis.has_value());
    EXPECT_EQ(viridis->size(), 256u);
    int index = 0;
    for (const Json& c : cases.items()) {
        SCOPED_TRACE("case " + std::to_string(index++));
        std::vector<double> data;
        for (const Json& v : c.find("data")->items()) data.push_back(hex(v));
        std::optional<double> alpha;
        if (c.find("alpha")->isNumber()) alpha = c.find("alpha")->asNumber();
        Normalize norm;
        const auto rgba = mapColors(data, norm, *viridis, alpha);
        const Json& want = *c.find("rgba");
        ASSERT_EQ(rgba.size(), want.size());
        for (std::size_t i = 0; i < rgba.size(); ++i)
            for (int k = 0; k < 4; ++k) EXPECT_EQ(rgba[i][std::size_t(k)], hex(want[i][std::size_t(k)])) << i << " " << k;
    }
    EXPECT_EQ(index, 300);
    EXPECT_FALSE(ListedColormap::named("no-such-map").has_value());
}

}  // namespace
