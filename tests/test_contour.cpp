// Contour lines and filled regions as contourpy 1.4.0's mpl2014 generator gives them to matplotlib 3.11.2
// (tests/oracle/contour.json, written by tools/oracle/make_contour_oracle.py).
#include "contour/contour.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace contour = calculate_plot::contour;

namespace {

double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }
std::vector<double> hexes(const Json& list) {
    std::vector<double> out;
    for (const Json& v : list.items()) out.push_back(hex(v));
    return out;
}

std::string difference(const contour::Paths& got, const Json& want) {
    const Json& verts = *want.find("vertices");
    const Json& codes = *want.find("codes");
    if (got.vertices.size() != verts.size()) return "paths: " + std::to_string(got.vertices.size()) + ", want " + std::to_string(verts.size());
    for (std::size_t i = 0; i < verts.size(); ++i) {
        const auto v = hexes(verts[i]);
        if (got.vertices[i] != v) return "vertices of path " + std::to_string(i);
        std::vector<unsigned char> c;
        for (const Json& k : codes[i].items()) c.push_back((unsigned char)k.asNumber());
        if (got.codes[i] != c) return "codes of path " + std::to_string(i);
    }
    return "";
}

TEST(Contour, Mpl2014IsContourpys) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/contour.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json oracle = *calculate_plot::support::parseJson(text.str(), nullptr);
    int checked = 0;
    for (const Json& c : oracle.find("cases")->items()) {
        const int ny = int(c.find("ny")->asNumber()), nx = int(c.find("nx")->asNumber());
        const auto x = hexes(*c.find("x")), y = hexes(*c.find("y")), z = hexes(*c.find("z"));
        const Json& chunk = *c.find("chunk_size");
        std::string error;
        auto gen = contour::Generator::make(x, y, z, ny, nx, c.find("corner_mask")->asBool(),
                                            chunk.isNull() ? 0 : (long long)chunk.asNumber(), &error);
        ASSERT_TRUE(gen) << error;
        const auto levels = hexes(*c.find("levels"));
        for (std::size_t i = 0; i < levels.size(); ++i)
            EXPECT_EQ(difference(gen->lines(levels[i]), (*c.find("lines"))[i]), "") << "case " << checked << " level " << i;
        for (std::size_t i = 0; i + 1 < levels.size(); ++i) {
            contour::Paths filled;
            ASSERT_TRUE(gen->filled(levels[i], levels[i + 1], &filled, &error)) << error;
            EXPECT_EQ(difference(filled, (*c.find("filled"))[i]), "") << "case " << checked << " band " << i;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 60);
}

TEST(Contour, RefusesWhatContourpyRefuses) {
    std::string error;
    EXPECT_FALSE(contour::Generator::make({0, 1}, {0, 0}, {1, 2}, 1, 2, true, 0, &error));
    EXPECT_EQ(error, "Input z must be at least a (2, 2) shaped array, but has shape (1, 2)");
    auto gen = contour::Generator::make({0, 1, 0, 1}, {0, 0, 1, 1}, {1, 2, 3, 4}, 2, 2, true, 0, &error);
    ASSERT_TRUE(gen);
    contour::Paths p;
    EXPECT_FALSE(gen->filled(2, 2, &p, &error));
    EXPECT_EQ(error, "upper_level must be larger than lower_level");
}

}  // namespace
