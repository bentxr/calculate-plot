// Delaunay triangulations through Qhull equal matplotlib 3.11.2's (tests/oracle/triangulation.json): the triangles
// and their neighbours of random point sets, grids of cocircular points, duplicates, points far from the origin, and
// matplotlib's errors for too few points, too few unique points and collinear points.
#include "support/json.hpp"
#include "tri/delaunay.hpp"
#include "tri/triangulation.hpp"

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

// Given triangles (some clockwise) and masks: the edges, the neighbours and the triangles turned anticlockwise
TEST(Triangulation, EdgesAndNeighboursAreMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/triangulation.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(oracle.has_value());
    ASSERT_EQ(oracle->find("given")->size(), 3u);
    for (const Json& g : oracle->find("given")->items()) {
        const std::string name = g.find("name")->asString();
        std::vector<double> x, y;
        for (const Json& v : g.find("x")->items()) x.push_back(std::strtod(v.asString().c_str(), nullptr));
        for (const Json& v : g.find("y")->items()) y.push_back(std::strtod(v.asString().c_str(), nullptr));
        const auto rows3 = [](const Json& j) {
            calculate_plot::tri::Triangulation::Triangles out;
            for (const Json& r : j.items()) out.push_back({int(r[0].asNumber()), int(r[1].asNumber()), int(r[2].asNumber())});
            return out;
        };
        std::optional<calculate_plot::tri::Triangulation::Triangles> triangles;
        if (!g.find("triangles")->isNull()) triangles = rows3(*g.find("triangles"));
        calculate_plot::tri::Triangulation t;
        std::string error;
        ASSERT_TRUE(calculate_plot::tri::Triangulation::make(x, y, triangles, std::nullopt, &t, &error)) << error;
        if (!g.find("mask")->isNull()) {
            std::vector<bool> mask;
            for (const Json& m : g.find("mask")->items()) mask.push_back(m.asBool());
            ASSERT_TRUE(t.setMask(mask, &error)) << error;
        }
        std::vector<std::array<int, 2>> edges;
        for (const Json& r : g.find("edges")->items()) edges.push_back({int(r[0].asNumber()), int(r[1].asNumber())});
        EXPECT_EQ(t.edges(), edges) << name;
        EXPECT_EQ(t.neighbors(), rows3(*g.find("neighbors"))) << name;
        EXPECT_EQ(t.triangles(), rows3(*g.find("after"))) << name;
    }
}
