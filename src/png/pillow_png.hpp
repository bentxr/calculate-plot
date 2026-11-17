// Derived from Pillow 12.3.0: src/libImaging/GetBBox.c (getcolors32), src/libImaging/ZipEncode.c (the PNG
// filters and the deflate settings), src/libImaging/Pack.c (pack1, packP2, packP4) and
// src/PIL/PngImagePlugin.py / src/PIL/ImageFile.py (_save: the bit depth of palette images, the chunks).
// Copyright (c) 1997-2011 by Secret Labs AB; Copyright (c) 1995-2011 by Fredrik Lundh and contributors;
// Copyright (c) 2010 by Jeffrey 'Alex' Clark and contributors
// Licence: LICENSES/Pillow.txt (MIT-CMU). Changes: licensing/changes/pillow.md
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::png {

// One entry of Image.getcolors(): how many pixels have the colour.
struct ColorCount {
    std::uint32_t count;
    std::array<unsigned char, 3> rgb;
};

// Image.getcolors(maxcolors) of an RGB image (rows of width * 3 bytes), in Pillow's order (its hash
// table's); nullopt when the image has more than `maxcolors` colours.
std::optional<std::vector<ColorCount>> pillowGetColors(const unsigned char* rgb, int width, int height, int maxcolors);

// Image.save(format="png") with Pillow's defaults: the whole file. `channels` 1 (L), 3 (RGB) or 4 (RGBA);
// `text`: tEXt chunks (PngInfo.add_text), written after the header as pnginfo's chunks are.
std::string pillowSavePng(const unsigned char* pixels, int width, int height, int channels,
                          const std::vector<std::pair<std::string, std::string>>& text = {});
// the same for a P image: one index per pixel and its palette (3 bytes per colour, 1 to 256 colours)
std::string pillowSavePngPalette(const unsigned char* indices, int width, int height, const std::string& palette);

}  // namespace calculate_plot::png
