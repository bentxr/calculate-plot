#include "png/png.hpp"
#include "support/json.hpp"
#include "support/sha256.hpp"

#include <gtest/gtest.h>
#include <zlib.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::png::decodePng;
using calculate_plot::png::encodePng;
using calculate_plot::png::Image;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

Image randomImage(int width, int height, unsigned seed) {
    std::mt19937 rng(seed);
    Image image{width, height, std::vector<unsigned char>(std::size_t(width) * height * 4)};
    for (auto& byte : image.rgba) byte = static_cast<unsigned char>(rng() % 7 == 0 ? 255 : rng() & 0xff);
    return image;
}

}  // namespace

TEST(Png, RoundTripsEverySize) {
    for (auto [w, h] : {std::pair{1, 1}, {2, 3}, {7, 1}, {1, 9}, {33, 17}, {320, 240}}) {
        const Image image = randomImage(w, h, unsigned(w * 1000 + h));
        std::string error;
        const auto back = decodePng(encodePng(image, {}), &error);
        ASSERT_TRUE(back.has_value()) << error;
        EXPECT_EQ(back->width, w);
        EXPECT_EQ(back->height, h);
        EXPECT_EQ(back->rgba, image.rgba) << w << "x" << h;
    }
}

TEST(Png, StartsWithTheSignatureAndHeader) {
    const std::string png = encodePng(randomImage(5, 4, 1), {});
    ASSERT_GE(png.size(), 33u);
    EXPECT_EQ(png.substr(0, 8), std::string("\x89PNG\r\n\x1a\n", 8));
    EXPECT_EQ(png.substr(12, 4), "IHDR");
    EXPECT_EQ(png.substr(png.size() - 8, 4), "IEND");
}

TEST(Png, WritesTextChunks) {
    const std::string png = encodePng(randomImage(2, 2, 2), {{"Software", "calculate-plot 0.1.0"}});
    EXPECT_NE(png.find(std::string("tEXtSoftware\0calculate-plot 0.1.0", 33)), std::string::npos);
}

TEST(Png, DecodesEveryReferenceImageExactly) {
    // The references were written by Pillow; their raw RGBA hash was recorded when they were made.
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/corpus")) {
        const auto meta = calculate_plot::support::parseJson(readFile(entry.path() / "ref.json"), nullptr);
        ASSERT_TRUE(meta.has_value()) << entry.path();
        std::string error;
        const auto image = decodePng(readFile(entry.path() / "ref.png"), &error);
        ASSERT_TRUE(image.has_value()) << entry.path() << ": " << error;
        EXPECT_EQ(image->width, meta->find("width")->asNumber());
        EXPECT_EQ(image->height, meta->find("height")->asNumber());
        const std::string bytes(image->rgba.begin(), image->rgba.end());
        EXPECT_EQ(calculate_plot::support::sha256Hex(bytes), meta->find("rgba_sha256")->asString()) << entry.path();
        ++checked;
    }
    EXPECT_GE(checked, 27);
}

TEST(Png, RefusesDamagedFiles) {
    const std::string good = encodePng(randomImage(4, 4, 3), {});
    std::string error;
    EXPECT_FALSE(decodePng("", &error).has_value());
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(decodePng(good.substr(0, good.size() / 2), &error).has_value());  // truncated
    std::string badSignature = good;
    badSignature[1] = 'Q';
    EXPECT_FALSE(decodePng(badSignature, &error).has_value());
    std::string badCrc = good;
    badCrc[29] ^= 1;  // the IHDR CRC
    EXPECT_FALSE(decodePng(badCrc, &error).has_value());
    EXPECT_NE(error.find("CRC"), std::string::npos) << error;
}

TEST(Png, RefusesFormatsItDoesNotRead) {
    // A 16-bit image: the reader handles 8-bit RGBA, RGB, grey and grey+alpha only, and says so.
    std::string png = encodePng(randomImage(2, 2, 4), {});
    png[24] = 16;  // bit depth
    const auto crc = crc32(0L, reinterpret_cast<const Bytef*>(png.data() + 12), 17);  // type + data
    for (int i = 0; i < 4; ++i) png[29 + i] = static_cast<char>((crc >> (24 - 8 * i)) & 0xff);
    std::string error;
    EXPECT_FALSE(decodePng(png, &error).has_value());
    EXPECT_NE(error.find("bit depth"), std::string::npos) << error;
}
