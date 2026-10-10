#include "support/base64.hpp"

namespace calculate_plot::support {

namespace {

int sextet(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

std::optional<std::string> decodeBase64(std::string_view text) {
    if (text.size() % 4 != 0) return std::nullopt;
    std::string out;
    out.reserve(text.size() / 4 * 3);
    for (std::size_t at = 0; at < text.size(); at += 4) {
        const bool last = at + 4 == text.size();
        int padding = 0;
        unsigned long group = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[at + i];
            if (c == '=' && last && i >= 2) {
                ++padding;
                group <<= 6;
                continue;
            }
            const int value = sextet(c);
            if (value < 0 || padding) return std::nullopt;  // bad character, or data after '='
            group = (group << 6) | unsigned(value);
        }
        out.push_back(static_cast<char>((group >> 16) & 0xff));
        if (padding < 2) out.push_back(static_cast<char>((group >> 8) & 0xff));
        if (padding < 1) out.push_back(static_cast<char>(group & 0xff));
    }
    return out;
}

}  // namespace calculate_plot::support
