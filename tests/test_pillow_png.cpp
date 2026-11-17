// Pillow's own handling of the images matplotlib's PDF and SVG backends give it, against Pillow 12.3.0
// (tests/oracle/pillow_png.json, written by tools/oracle/make_pillow_png_oracle.py): getcolors' list in
// its order, and the bytes of the PNG files it saves.
#include "png/pillow_png.hpp"
#include "support/base64.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

namespace {

using calculate_plot::support::Json;

const Json& oracle() {
    static const Json doc = [] {
        std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/pillow_png.json", std::ios::binary);
        std::ostringstream s;
        s << in.rdbuf();
        return *calculate_plot::support::parseJson(s.str(), nullptr);
    }();
    return doc;
}

std::string bytes(const Json& base64) {
    const auto b = calculate_plot::support::decodeBase64(base64.asString());
    EXPECT_TRUE(b.has_value());
    return b ? std::string(b->begin(), b->end()) : std::string();
}

TEST(PillowPng, GetcolorsListsColoursInPillowsOrder) {
    int checked = 0;
    for (const Json& c : oracle().find("cases")->items()) {
        if (c.find("mode")->asString() != "RGB") continue;
        const Json& shape = *c.find("image")->find("shape");
        const std::string im = bytes(*c.find("image")->find("base64"));
        const auto got = calculate_plot::png::pillowGetColors(reinterpret_cast<const unsigned char*>(im.data()),
                                                              int(shape[1].asNumber()), int(shape[0].asNumber()), 256);
        const Json& want = *c.find("getcolors");
        ASSERT_EQ(got.has_value(), !want.isNull()) << checked;
        if (!got) continue;
        ASSERT_EQ(got->size(), want.size()) << checked;
        for (std::size_t i = 0; i < got->size(); ++i) {
            EXPECT_EQ((*got)[i].count, std::uint32_t(want[i][0].asNumber())) << checked << " " << i;
            for (int k = 0; k < 3; ++k) EXPECT_EQ(int((*got)[i].rgb[std::size_t(k)]), int(want[i][1][std::size_t(k)].asNumber())) << checked;
        }
        ++checked;
    }
    EXPECT_GE(checked, 40);
}

TEST(PillowPng, SavedFilesEqualPillows) {
    int checked = 0;
    for (const Json& c : oracle().find("cases")->items()) {
        const std::string mode = c.find("mode")->asString();
        const Json& shape = *c.find("image")->find("shape");
        const std::string im = bytes(*c.find("image")->find("base64"));
        const auto* pixels = reinterpret_cast<const unsigned char*>(im.data());
        const int height = int(shape[0].asNumber()), width = int(shape[1].asNumber());
        const std::string got = mode == "P" ? calculate_plot::png::pillowSavePngPalette(pixels, width, height, bytes(*c.find("palette")))
                                            : calculate_plot::png::pillowSavePng(pixels, width, height,
                                                                                 mode == "L" ? 1 : mode == "RGB" ? 3 : 4);
        EXPECT_TRUE(got == bytes(*c.find("png"))) << mode << " case " << checked << " (" << width << "x" << height << ")";
        ++checked;
    }
    EXPECT_EQ(checked, 102);
}

}  // namespace
