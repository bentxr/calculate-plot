// Delaunay triangulations through Qhull equal matplotlib 3.11.2's (tests/oracle/triangulation.json): the triangles
// and their neighbours of random point sets, grids of cocircular points, duplicates, points far from the origin, and
// matplotlib's errors for too few points, too few unique points and collinear points.
#include "support/json.hpp"
#include "tri/delaunay.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;

TEST(Triangulation, DelaunayIsMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/triangulation.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    ASSERT_EQ(oracle->find("cases")->size(), 18u);
    int errors = 0;
    for (const Json& c : oracle->find("cases")->items()) {
        const std::string name = c.find("name")->asString();
        std::vector<double> x, y;
        for (const Json& v : c.find("x")->items()) x.push_back(std::strtod(v.asString().c_str(), nullptr));
        for (const Json& v : c.find("y")->items()) y.push_back(std::strtod(v.asString().c_str(), nullptr));
        calculate_plot::tri::Delaunay d;
        std::string error;
        const bool ok = calculate_plot::tri::delaunay(x, y, &d, &error);
        if (const Json* e = c.find("error")) {
            EXPECT_FALSE(ok) << name;
            EXPECT_EQ(error, e->asString()) << name;
            ++errors;
            continue;
        }
        ASSERT_TRUE(ok) << name << ": " << error;
        const auto rows = [](const Json& j) {
            std::vector<std::array<int, 3>> out;
            for (const Json& r : j.items()) out.push_back({int(r[0].asNumber()), int(r[1].asNumber()), int(r[2].asNumber())});
            return out;
        };
        EXPECT_EQ(d.triangles, rows(*c.find("triangles"))) << name;
        EXPECT_EQ(d.neighbors, rows(*c.find("neighbors"))) << name;
    }
    EXPECT_EQ(errors, 3);
}
