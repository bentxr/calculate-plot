// Derived from Pillow 12.3.0 (see pillow_png.hpp).
// Copyright (c) 1997-2011 by Secret Labs AB; Copyright (c) 1995-2011 by Fredrik Lundh and contributors;
// Copyright (c) 2010 by Jeffrey 'Alex' Clark and contributors
// Licence: LICENSES/Pillow.txt (MIT-CMU). Changes: licensing/changes/pillow.md
#include "png/pillow_png.hpp"

#include <zlib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace calculate_plot::png {

std::optional<std::vector<ColorCount>> pillowGetColors(const unsigned char* rgb, int width, int height, int maxcolors) {
    // GetBBox.c getcolors32: open addressing on the pixel value (R | G << 8 | B << 16 with the fourth
    // byte masked, as an RGB image's 32-bit pixel reads on a little-endian machine); the table packed in
    // index order. (The hash algorithm is based on the dictionary code in Python 2.1.3.)
    static const unsigned int SIZES[] = {4,          3,  8,         3,  16,        3,  32,        5,
                                         64,         3,  128,       3,  256,       29, 512,       17,
                                         1024,       9,  2048,      5,  4096,      83, 8192,      27,
                                         16384,      43, 32768,     3,  65536,     45, 131072,    9,
                                         262144,     39, 524288,    39, 1048576,   9,  2097152,   5,
                                         4194304,    3,  8388608,   33, 16777216,  27, 33554432,  9,
                                         67108864,   71, 134217728, 39, 268435456, 9,  536870912, 5,
                                         1073741824, 83, 0};
    unsigned int codeSize = 0, codePoly = 0, codeMask = 0;
    for (unsigned int i = 0; SIZES[i]; i += 2) {
        if (SIZES[i] > static_cast<unsigned int>(maxcolors)) {
            codeSize = SIZES[i];
            codePoly = SIZES[i + 1];
            codeMask = codeSize - 1;
            break;
        }
    }
    if (!codeSize) return std::nullopt;
    struct Item {
        std::uint32_t pixel = 0;
        std::uint32_t count = 0;
    };
    std::vector<Item> table(codeSize + 1);
    int colors = 0;
    bool overflow = false;
    for (int y = 0; y < height && !overflow; y++) {
        for (int x = 0; x < width; x++) {
            const unsigned char* p = rgb + (std::size_t(y) * width + x) * 3;
            const std::uint32_t pixel = std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16;
            const unsigned int h = pixel;  // null hashing
            unsigned int i = (~h) & codeMask;
            Item* v = &table[i];
            if (!v->count) {
                if (colors++ == maxcolors) {
                    overflow = true;
                    break;
                }
                v->pixel = pixel, v->count = 1;
                continue;
            } else if (v->pixel == pixel) {
                v->count++;
                continue;
            }
            unsigned int incr = (h ^ (h >> 3)) & codeMask;
            if (!incr) incr = codeMask;
            for (;;) {
                i = (i + incr) & codeMask;
                v = &table[i];
                if (!v->count) {
                    if (colors++ == maxcolors) {
                        overflow = true;
                        break;
                    }
                    v->pixel = pixel, v->count = 1;
                    break;
                } else if (v->pixel == pixel) {
                    v->count++;
                    break;
                }
                incr = incr << 1;
                if (incr > codeMask) incr = incr ^ codePoly;
            }
            if (overflow) break;
        }
    }
    // (_imaging.c's getcolors gives None when there are more colours than maxcolors)
    if (colors > maxcolors) return std::nullopt;
    std::vector<ColorCount> out;
    for (unsigned int x = 0; x < codeSize; x++)
        if (table[x].count)
            out.push_back({table[x].count, {static_cast<unsigned char>(table[x].pixel & 0xff),
                                            static_cast<unsigned char>((table[x].pixel >> 8) & 0xff),
                                            static_cast<unsigned char>((table[x].pixel >> 16) & 0xff)}});
    return out;
}

namespace {

void putUint32(std::string& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((value >> shift) & 0xff));
}

// PngImagePlugin.putchunk: length, type, data, CRC of type and data
void putChunk(std::string& out, const char* type, const std::string& data) {
    putUint32(out, static_cast<std::uint32_t>(data.size()));
    const std::string typed = std::string(type, 4) + data;
    out += typed;
    putUint32(out, static_cast<std::uint32_t>(crc32(0, reinterpret_cast<const Bytef*>(typed.data()), uInt(typed.size()))));
}

// ZipEncode.c in PNG mode: each row as the packer leaves it (`bytes` long), filtered by the filter whose
// output is closest to zero among none, up, sub and Paeth (tried in that order, a later one taken only
// when strictly better; average only with optimize), then deflated (level 6, memory level 9, Z_FILTERED).
// In ZIP_PNG_PALETTE mode (encode.c: the raw modes P and PA, data that are not continuous) the rows are not
// filtered and the strategy is Z_DEFAULT_STRATEGY.
std::string zipEncode(const std::vector<std::string>& rows, int bits, bool palette) {
    const std::size_t bytes = rows.empty() ? 0 : rows[0].size();
    const int bpp = (bits + 7) / 8;
    std::vector<unsigned char> buffer(bytes + 1, 0), previous(bytes + 1, 0), prior(bytes + 1), up(bytes + 1), paeth(bytes + 1);
    prior[0] = 1, up[0] = 2, paeth[0] = 4;
    std::string filtered;
    filtered.reserve((bytes + 1) * rows.size());
    const auto distance = [](unsigned char v) { return (v < 128) ? int(v) : 256 - int(v); };
    for (const std::string& row : rows) {
        std::memcpy(buffer.data() + 1, row.data(), bytes);
        const unsigned char* output = buffer.data();
        int sum = 0, s = 0;
        std::size_t i;
        if (palette) {
            filtered.append(reinterpret_cast<const char*>(output), bytes + 1);
            std::swap(buffer, previous);
            continue;
        }
        for (i = 1, sum = 0; i <= bytes; i++) sum += distance(buffer[i]);  // 0. none
        if (sum > 0) {  // 2. up (first, to save time when a row repeats the one above)
            for (i = 1, s = 0; i <= bytes; i++) {
                const unsigned char v = static_cast<unsigned char>(buffer[i] - previous[i]);
                up[i] = v;
                s += distance(v);
            }
            if (s < sum) output = up.data(), sum = s;
        }
        if (sum > 0) {  // 1. prior (sub)
            for (i = 1, s = 0; i <= std::size_t(bpp) && i <= bytes; i++) {
                const unsigned char v = buffer[i];
                prior[i] = v;
                s += distance(v);
            }
            for (; i <= bytes; i++) {
                const unsigned char v = static_cast<unsigned char>(buffer[i] - buffer[i - std::size_t(bpp)]);
                prior[i] = v;
                s += distance(v);
            }
            if (s < sum) output = prior.data(), sum = s;
        }
        if (sum > 0) {  // 4. Paeth
            for (i = 1, s = 0; i <= std::size_t(bpp) && i <= bytes; i++) {
                const unsigned char v = static_cast<unsigned char>(buffer[i] - previous[i]);
                paeth[i] = v;
                s += distance(v);
            }
            for (; i <= bytes; i++) {
                const int a = buffer[i - std::size_t(bpp)], b = previous[i], c = previous[i - std::size_t(bpp)];
                const int pa = std::abs(b - c), pb = std::abs(a - c), pc = std::abs(a + b - 2 * c);
                const unsigned char v =
                    static_cast<unsigned char>(buffer[i] - ((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c));
                paeth[i] = v;
                s += distance(v);
            }
            if (s < sum) output = paeth.data(), sum = s;
        }
        filtered.append(reinterpret_cast<const char*>(output), bytes + 1);
        std::swap(buffer, previous);
    }
    std::string compressed(compressBound(uLong(filtered.size())) + 64, '\0');
    z_stream z{};
    deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 9, palette ? Z_DEFAULT_STRATEGY : Z_FILTERED);
    z.next_in = reinterpret_cast<Bytef*>(filtered.data());
    z.avail_in = uInt(filtered.size());
    z.next_out = reinterpret_cast<Bytef*>(compressed.data());
    z.avail_out = uInt(compressed.size());
    deflate(&z, Z_FINISH);
    compressed.resize(z.total_out);
    deflateEnd(&z);
    return compressed;
}

// PngImagePlugin._save with ImageFile._save: IHDR, the palette, IDAT chunks of the encoder's blocks
// (max(MAXBLOCK, width * 4) bytes each), IEND.
std::string savePng(int width, int height, int bitDepth, int colorType, const std::string* palette,
                    const std::vector<std::string>& rows, int bits,
                    const std::vector<std::pair<std::string, std::string>>& text = {}) {
    std::string out("\x89PNG\r\n\x1a\n", 8);
    std::string header;
    putUint32(header, static_cast<std::uint32_t>(width));
    putUint32(header, static_cast<std::uint32_t>(height));
    header.push_back(static_cast<char>(bitDepth));
    header.push_back(static_cast<char>(colorType));
    header.append(3, '\0');  // compression, filter category, interlace
    putChunk(out, "IHDR", header);
    for (const auto& [keyword, value] : text) putChunk(out, "tEXt", keyword + std::string(1, '\0') + value);
    if (palette) putChunk(out, "PLTE", *palette);
    // (the raw mode P: an 8-bit palette image)
    const std::string data = zipEncode(rows, bits, colorType == 3 && bits == 8);
    const std::size_t bufsize = std::max<std::size_t>(65536, std::size_t(width) * 4);
    for (std::size_t at = 0; at < data.size(); at += bufsize) putChunk(out, "IDAT", data.substr(at, bufsize));
    putChunk(out, "IEND", "");
    return out;
}

}  // namespace

std::string pillowSavePng(const unsigned char* pixels, int width, int height, int channels,
                          const std::vector<std::pair<std::string, std::string>>& text) {
    // _OUTMODES: L 8 bits type 0, RGB 8 bits type 2, RGBA 8 bits type 6
    const int colorType = channels == 1 ? 0 : channels == 3 ? 2 : 6;
    std::vector<std::string> rows;
    const std::size_t stride = std::size_t(width) * std::size_t(channels);
    for (int y = 0; y < height; ++y) rows.emplace_back(reinterpret_cast<const char*>(pixels) + y * stride, stride);
    return savePng(width, height, 8, colorType, nullptr, rows, 8 * channels, text);
}

std::string pillowSavePngPalette(const unsigned char* indices, int width, int height, const std::string& palette) {
    // the palette's colours decide the bits of a row: up to 2 colours 1 bit (pack1), 4 2 bits (packP2),
    // 16 4 bits (packP4), else 8; the PLTE chunk holds those colours
    const int colors = std::max(std::min(int(palette.size() / 3), 256), 1);
    const int bits = colors <= 2 ? 1 : colors <= 4 ? 2 : colors <= 16 ? 4 : 8;
    std::string plte = palette.substr(0, std::size_t(colors) * 3);
    plte.resize(std::size_t(colors) * 3, '\0');
    std::vector<std::string> rows;
    for (int y = 0; y < height; ++y) {
        const unsigned char* in = indices + std::size_t(y) * width;
        std::string row((std::size_t(width) * bits + 7) / 8, '\0');
        auto* out = reinterpret_cast<unsigned char*>(row.data());
        if (bits == 8) {
            std::memcpy(out, in, std::size_t(width));
        } else if (bits == 1) {  // pack1: bilevel, black is 0
            int b = 0, m = 128;
            for (int i = 0; i < width; i++) {
                if (in[i] != 0) b |= m;
                m >>= 1;
                if (m == 0) *out++ = static_cast<unsigned char>(b), b = 0, m = 128;
            }
            if (m != 128) *out++ = static_cast<unsigned char>(b);
        } else if (bits == 2) {  // packP2
            int pixels = width;
            while (pixels >= 4) {
                *out++ = static_cast<unsigned char>((in[0] << 6) | ((in[1] & 3) << 4) | ((in[2] & 3) << 2) | (in[3] & 3));
                in += 4;
                pixels -= 4;
            }
            switch (pixels) {
                case 3: out[0] = static_cast<unsigned char>((in[0] << 6) | ((in[1] & 3) << 4) | ((in[2] & 3) << 2)); break;
                case 2: out[0] = static_cast<unsigned char>((in[0] << 6) | ((in[1] & 3) << 4)); break;
                case 1: out[0] = static_cast<unsigned char>(in[0] << 6);
            }
        } else {  // packP4
            int pixels = width;
            while (pixels >= 2) {
                *out++ = static_cast<unsigned char>((in[0] << 4) | (in[1] & 15));
                in += 2;
                pixels -= 2;
            }
            if (pixels) out[0] = static_cast<unsigned char>(in[0] << 4);
        }
        rows.push_back(row);
    }
    return savePng(width, height, bits, 3, &plte, rows, bits);
}

}  // namespace calculate_plot::png
