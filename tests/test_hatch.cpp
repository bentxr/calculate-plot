// Hatch paths against matplotlib 3.11.2's hatch.get_path (tests/oracle/hatch.json, written by
// tools/oracle/make_hatch_oracle.py).
#include "engine/hatch.hpp"

#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;

TEST(Hatch, PathsAreMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/hatch.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json oracle = *calculate_plot::support::parseJson(text.str(), nullptr);
    const auto rc = calculate_plot::rc::Params::defaults();
    int checked = 0, refused = 0;
    for (const Json& c : oracle.find("cases")->items()) {
        const std::string pattern = c.find("pattern")->asString();
        const int density = int(c.find("density")->asNumber());
        const std::string name = "'" + pattern + "' at " + std::to_string(density);
        const auto path = calculate_plot::engine::hatchPath(pattern, rc, density);
        if (c.find("error")) {
            EXPECT_FALSE(path.has_value()) << name;
            ++refused;
            continue;
        }
        ASSERT_TRUE(path.has_value()) << name;
        const Json& vertices = *c.find("vertices");
        ASSERT_EQ(path->size() * 2, vertices.size()) << name;
        for (std::size_t i = 0; i < vertices.size(); ++i)
            ASSERT_EQ((*path->vertices)[i], std::strtod(vertices[i].asString().c_str(), nullptr)) << name << " vertex value " << i;
        const Json& codes = *c.find("codes");
        if (codes.isNull()) {
            EXPECT_FALSE(path->codes) << name;
        } else {
            ASSERT_TRUE(path->codes) << name;
            ASSERT_EQ(path->codes->size(), codes.size()) << name;
            for (std::size_t i = 0; i < codes.size(); ++i) EXPECT_EQ(int((*path->codes)[i]), int(codes[i].asNumber())) << name << " code " << i;
        }
        EXPECT_EQ(path->shouldSimplify, c.find("simplify")->asBool()) << name;
        ++checked;
    }
    EXPECT_EQ(checked, 157);
    EXPECT_EQ(refused, 28);
}
