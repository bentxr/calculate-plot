#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculate_plot::png {

// An 8-bit RGBA image, rows top to bottom, 4 bytes per pixel (straight, not premultiplied alpha).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

// A PNG file (8-bit RGBA, not interlaced) with the given tEXt chunks (keyword, Latin-1 text).
std::string encodePng(const Image& image, const std::vector<std::pair<std::string, std::string>>& text);

// Reads 8-bit, non-interlaced PNGs: RGBA, RGB, grey and grey+alpha (converted to RGBA). Checks
// every CRC and the zlib stream; anything else is refused with a reason in *error.
std::optional<Image> decodePng(std::string_view file, std::string* error);

}  // namespace calculate_plot::png
