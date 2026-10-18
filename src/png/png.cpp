#include "png/png.hpp"

#include <zlib.h>

#include <cstdint>
#include <cstdlib>

namespace calculate_plot::png {

namespace {

const std::string_view kSignature("\x89PNG\r\n\x1a\n", 8);

void putUint32(std::string& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((value >> shift) & 0xff));
}

std::uint32_t getUint32(std::string_view bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value = (value << 8) | static_cast<unsigned char>(bytes[at + i]);
    return value;
}

std::uint32_t crcOf(std::string_view bytes) {
    return static_cast<std::uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(bytes.data()), uInt(bytes.size())));
}

void putChunk(std::string& out, std::string_view type, std::string_view data) {
    putUint32(out, static_cast<std::uint32_t>(data.size()));
    std::string body(type);
    body.append(data);
    out.append(body);
    putUint32(out, crcOf(body));
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

// The filtered byte for filter type `type` (PNG spec 9.2); x is the byte index in the row.
unsigned char filtered(int type, const unsigned char* row, const unsigned char* above, std::size_t x, int bpp) {
    const int a = x >= std::size_t(bpp) ? row[x - bpp] : 0;
    const int b = above ? above[x] : 0;
    const int c = (above && x >= std::size_t(bpp)) ? above[x - bpp] : 0;
    switch (type) {
    case 1: return static_cast<unsigned char>(row[x] - a);
    case 2: return static_cast<unsigned char>(row[x] - b);
    case 3: return static_cast<unsigned char>(row[x] - (a + b) / 2);
    case 4: return static_cast<unsigned char>(row[x] - paeth(a, b, c));
    default: return row[x];
    }
}

void fail(std::string* error, const std::string& message) {
    if (error) *error = message;
}

}  // namespace

std::string encodePng(const Image& image, const std::vector<std::pair<std::string, std::string>>& text) {
    const std::size_t stride = std::size_t(image.width) * 4;
    // Each row gets the filter whose output has the smallest sum of absolute values (as libpng does).
    std::string raw;
    raw.reserve((stride + 1) * std::size_t(image.height));
    std::string candidate(stride, '\0');
    for (int y = 0; y < image.height; ++y) {
        const unsigned char* row = image.rgba.data() + std::size_t(y) * stride;
        const unsigned char* above = y > 0 ? row - stride : nullptr;
        int best = 0;
        unsigned long bestCost = ~0ul;
        for (int type = 0; type < 5; ++type) {
            unsigned long cost = 0;
            for (std::size_t x = 0; x < stride; ++x) {
                const auto v = static_cast<signed char>(filtered(type, row, above, x, 4));
                cost += static_cast<unsigned long>(std::abs(int(v)));
            }
            if (cost < bestCost) bestCost = cost, best = type;
        }
        raw.push_back(static_cast<char>(best));
        for (std::size_t x = 0; x < stride; ++x) candidate[x] = static_cast<char>(filtered(best, row, above, x, 4));
        raw.append(candidate);
    }

    // deflated as Pillow's PNG encoder does: level 6, memory level 9, the Z_FILTERED strategy
    std::string compressed(compressBound(uLong(raw.size())) + 64, '\0');
    z_stream z{};
    deflateInit2(&z, 6, Z_DEFLATED, 15, 9, Z_FILTERED);
    z.next_in = reinterpret_cast<Bytef*>(raw.data());
    z.avail_in = uInt(raw.size());
    z.next_out = reinterpret_cast<Bytef*>(compressed.data());
    z.avail_out = uInt(compressed.size());
    deflate(&z, Z_FINISH);
    compressed.resize(z.total_out);
    deflateEnd(&z);

    std::string out(kSignature);
    std::string header;
    putUint32(header, static_cast<std::uint32_t>(image.width));
    putUint32(header, static_cast<std::uint32_t>(image.height));
    header.append({'\x08', '\x06', '\0', '\0', '\0'});  // 8 bits, RGBA, deflate, adaptive filtering, no interlace
    putChunk(out, "IHDR", header);
    for (const auto& [keyword, value] : text) putChunk(out, "tEXt", keyword + std::string(1, '\0') + value);
    putChunk(out, "IDAT", compressed);
    putChunk(out, "IEND", "");
    return out;
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
