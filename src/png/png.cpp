#include "png/png.hpp"

#include "png/pillow_png.hpp"

#include <zlib.h>

#include <cstdint>
#include <cstdlib>

namespace calculate_plot::png {

namespace {

const std::string_view kSignature("\x89PNG\r\n\x1a\n", 8);

std::uint32_t getUint32(std::string_view bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value = (value << 8) | static_cast<unsigned char>(bytes[at + i]);
    return value;
}

std::uint32_t crcOf(std::string_view bytes) {
    return static_cast<std::uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(bytes.data()), uInt(bytes.size())));
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

void fail(std::string* error, const std::string& message) {
    if (error) *error = message;
}

}  // namespace

std::string encodePng(const Image& image, const std::vector<std::pair<std::string, std::string>>& text) {
    // as Pillow saves an RGBA image (matplotlib saves its PNG files, and the images it embeds in SVG, with Pillow)
    return pillowSavePng(image.rgba.data(), image.width, image.height, 4, text);
}

std::optional<Image> decodePng(std::string_view file, std::string* error) {
    if (file.substr(0, 8) != kSignature) return fail(error, "not a PNG file (bad signature)"), std::nullopt;
    std::size_t at = 8;
    int width = 0, height = 0, channels = 0;
    bool seenHeader = false, seenEnd = false;
    std::string idat;
    while (at + 12 <= file.size()) {
        const std::uint32_t length = getUint32(file, at);
        if (length > file.size() - at - 12) return fail(error, "truncated chunk"), std::nullopt;
        const std::string_view type = file.substr(at + 4, 4);
        const std::string_view data = file.substr(at + 8, length);
        if (crcOf(file.substr(at + 4, 4 + length)) != getUint32(file, at + 8 + length))
            return fail(error, "CRC mismatch in chunk " + std::string(type)), std::nullopt;
        at += 12 + length;
        if (type == "IHDR") {
            if (length != 13) return fail(error, "bad IHDR"), std::nullopt;
            width = static_cast<int>(getUint32(data, 0));
            height = static_cast<int>(getUint32(data, 4));
            if (data[8] != 8) return fail(error, "unsupported bit depth " + std::to_string(int(data[8]))), std::nullopt;
            switch (data[9]) {
            case 0: channels = 1; break;
            case 2: channels = 3; break;
            case 4: channels = 2; break;
            case 6: channels = 4; break;
            default: return fail(error, "unsupported colour type " + std::to_string(int(data[9]))), std::nullopt;
            }
            if (data[10] != 0 || data[11] != 0) return fail(error, "unknown compression or filter method"), std::nullopt;
            if (data[12] != 0) return fail(error, "interlaced images are not supported"), std::nullopt;
            if (width <= 0 || height <= 0 || width > (1 << 16) || height > (1 << 16))
                return fail(error, "unsupported image size"), std::nullopt;
            seenHeader = true;
        } else if (type == "IDAT") {
            idat.append(data);
        } else if (type == "IEND") {
            seenEnd = true;
            break;
        } else if ((type[0] & 0x20) == 0) {  // an unknown critical chunk
            return fail(error, "unsupported critical chunk " + std::string(type)), std::nullopt;
        }
    }
    if (!seenHeader || !seenEnd) return fail(error, "truncated file (missing IHDR or IEND)"), std::nullopt;

    const std::size_t stride = std::size_t(width) * channels;
    std::string raw((stride + 1) * std::size_t(height), '\0');
    uLongf rawSize = uLongf(raw.size());
    if (uncompress(reinterpret_cast<Bytef*>(raw.data()), &rawSize, reinterpret_cast<const Bytef*>(idat.data()),
                   uLong(idat.size())) != Z_OK ||
        rawSize != raw.size())
        return fail(error, "damaged image data"), std::nullopt;

    std::vector<unsigned char> pixels(stride * std::size_t(height));
    for (int y = 0; y < height; ++y) {
        const int type = static_cast<unsigned char>(raw[std::size_t(y) * (stride + 1)]);
        if (type > 4) return fail(error, "unknown row filter"), std::nullopt;
        const auto* in = reinterpret_cast<const unsigned char*>(raw.data()) + std::size_t(y) * (stride + 1) + 1;
        unsigned char* row = pixels.data() + std::size_t(y) * stride;
        const unsigned char* above = y > 0 ? row - stride : nullptr;
        for (std::size_t x = 0; x < stride; ++x) {
            const int a = x >= std::size_t(channels) ? row[x - channels] : 0;
            const int b = above ? above[x] : 0;
            const int c = (above && x >= std::size_t(channels)) ? above[x - channels] : 0;
            int predictor = 0;
            switch (type) {
            case 1: predictor = a; break;
            case 2: predictor = b; break;
            case 3: predictor = (a + b) / 2; break;
            case 4: predictor = paeth(a, b, c); break;
            default: break;
            }
            row[x] = static_cast<unsigned char>(in[x] + predictor);
        }
    }

    Image image{width, height, std::vector<unsigned char>(std::size_t(width) * height * 4)};
    for (std::size_t i = 0, n = std::size_t(width) * height; i < n; ++i) {
        const unsigned char* p = pixels.data() + i * channels;
        unsigned char* q = image.rgba.data() + i * 4;
        switch (channels) {
        case 1: q[0] = q[1] = q[2] = p[0], q[3] = 255; break;
        case 2: q[0] = q[1] = q[2] = p[0], q[3] = p[1]; break;
        case 3: q[0] = p[0], q[1] = p[1], q[2] = p[2], q[3] = 255; break;
        default: q[0] = p[0], q[1] = p[1], q[2] = p[2], q[3] = p[3]; break;
        }
    }
    return image;
}

}  // namespace calculate_plot::png
