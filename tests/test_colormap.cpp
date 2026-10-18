// Colormaps as matplotlib 3.11.2 has them: every registered colormap's lookup table and resampled tables
// (tests/oracle/colormaps.json), Colormap.__call__ on floats, float32, integers, masks, alphas and bytes, and
// ScalarMappable(Normalize(), 'viridis').to_rgba for 300 random data sets (tests/oracle/colormapping.json).
#include "colors/colormap.hpp"
#include "support/json.hpp"
#include "support/sha256.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::colors;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

Json load(const char* name) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle" / name, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

// the SHA-256 of a lookup table's float64 bytes (little-endian), row by row
std::string digest(const std::vector<Rgba>& lut) {
    std::string bytes;
    for (const Rgba& row : lut)
        for (double v : row) {
            unsigned char b[8];
            std::uint64_t u;
            std::memcpy(&u, &v, 8);
            for (int k = 0; k < 8; ++k) b[k] = (unsigned char)(u >> (8 * k));
            bytes.append(reinterpret_cast<const char*>(b), 8);
        }
    return calculate_plot::support::sha256Hex(bytes);
}

std::string tableDifference(const Colormap& cmap, const Json& want) {
    if ((long long)cmap.size() != (long long)want.find("N")->asNumber()) return "N " + std::to_string(cmap.size());
    for (const auto& [row, values] : want.find("rows")->members())
        for (std::size_t k = 0; k < 4; ++k)
            if (cmap.lut()[std::size_t(std::stoi(row))][k] != hex(values[k])) return "row " + row;
    if (digest(cmap.lut()) != want.find("sha256")->asString()) return "digest";
    return "";
}

TEST(Colormap, EveryRegisteredTableIsMatplotlibs) {
    const Json oracle = load("colormaps.json");
    int checked = 0, refused = 0;
    for (const Json& t : oracle.find("tables")->items()) {
        const std::string name = t.find("name")->asString();
        const auto cmap = Colormap::named(name);
        ASSERT_TRUE(cmap.has_value()) << name;
        EXPECT_EQ(cmap->name(), name);
        EXPECT_EQ(tableDifference(*cmap, *t.find("table")), "") << name;
        for (const auto& [size, want] : t.find("resampled")->members()) {
            Colormap r;
            std::string error;
            if (!cmap->resampled(std::stoll(size), &r, &error)) {
                // a channel matplotlib computes with a function is data here, at 256 entries only
                EXPECT_NE(error.find("not supported yet"), std::string::npos) << name << " " << size << ": " << error;
                EXPECT_TRUE(cmap->hasFunctionChannels()) << name;
                ++refused;
                continue;
            }
            EXPECT_EQ(tableDifference(r, want), "") << name << " resampled to " << size;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 182);
    EXPECT_EQ(refused, 26 * 4);
    EXPECT_FALSE(Colormap::named("no-such-map").has_value());
    EXPECT_EQ(Colormap::names().size(), 182u);
}

TEST(Colormap, CallsAreMatplotlibs) {
    const Json oracle = load("colormaps.json");
    int index = 0;
    for (const Json& c : oracle.find("calls")->items()) {
        SCOPED_TRACE("call " + std::to_string(index++) + " " + c.find("name")->asString());
        const auto cmap = Colormap::named(c.find("name")->asString());
        ASSERT_TRUE(cmap.has_value());
        MaskedArray x;
        x.dtype = *dtypeNamed(c.find("dtype")->asString());
        for (const Json& v : c.find("x")->items()) x.data.push_back(hex(v));
        if (const Json* m = c.find("mask"); m && m->isArray()) {
            x.mask.assign(x.data.size(), false);
            for (const Json& i : m->items()) x.mask[std::size_t(i.asNumber())] = true;
        }
        std::vector<double> alpha;
        const Json& a = *c.find("alpha");
        if (a.isString()) alpha = {hex(a)};
        else if (a.isArray())
            for (const Json& v : a.items()) alpha.push_back(hex(v));
        const bool bytes = c.find("bytes")->asBool();
        std::vector<Rgba> out;
        std::string error;
        ASSERT_TRUE((*cmap)(x, a.isNull() ? nullptr : &alpha, bytes, &out, &error)) << error;
        const Json& want = *c.find("rgba");
        ASSERT_EQ(out.size(), want.size());
        for (std::size_t i = 0; i < out.size(); ++i)
            for (std::size_t k = 0; k < 4; ++k)
                EXPECT_EQ(out[i][k], bytes ? want[i][k].asNumber() : hex(want[i][k])) << i << " " << k;
    }
    EXPECT_EQ(index, 400);
}

TEST(Colormap, AlphaOfAnotherShapeIsRefused) {
    const auto cmap = Colormap::named("viridis");
    MaskedArray x;
    x.data = {0.1, 0.2, 0.3};
    std::vector<double> alpha{0.5, 0.5};
    std::vector<Rgba> out;
    std::string error;
    EXPECT_FALSE((*cmap)(x, &alpha, false, &out, &error));
    EXPECT_EQ(error, "alpha is array-like but its shape (2,) does not match that of X (3,)");
}

TEST(Colormap, ReversedNamesAreTheBasesReversed) {
    const auto viridis = Colormap::named("viridis"), reversed = Colormap::named("viridis_r");
    ASSERT_TRUE(viridis && reversed);
    EXPECT_EQ(viridis->lut()[0], reversed->lut()[255]);
    EXPECT_EQ(reversed->lut()[256], reversed->lut()[0]);  // under: the first colour
}

TEST(Colormap, ScalarMappableMapsAsMatplotlib) {
    const Json cases = load("colormapping.json");
    const auto viridis = Colormap::named("viridis");
    ASSERT_TRUE(viridis.has_value());
    EXPECT_EQ(viridis->size(), 256u);
    int index = 0;
    for (const Json& c : cases.items()) {
        SCOPED_TRACE("case " + std::to_string(index++));
        MaskedArray data;
        for (const Json& v : c.find("data")->items()) data.data.push_back(hex(v));
        std::optional<double> alpha;
        if (c.find("alpha")->isNumber()) alpha = c.find("alpha")->asNumber();
        Norm norm;
        std::string error;
        ASSERT_TRUE(Norm::make(NormSpec{}, &norm, &error));
        std::vector<Rgba> rgba;
        ASSERT_TRUE(mapColors(data, norm, *viridis, alpha, &rgba, &error)) << error;
        const Json& want = *c.find("rgba");
        ASSERT_EQ(rgba.size(), want.size());
        for (std::size_t i = 0; i < rgba.size(); ++i)
            for (int k = 0; k < 4; ++k) EXPECT_EQ(rgba[i][std::size_t(k)], hex(want[i][std::size_t(k)])) << i << " " << k;
    }
    EXPECT_EQ(index, 300);
}

}  // namespace
