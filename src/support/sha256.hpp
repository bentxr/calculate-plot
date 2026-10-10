#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace calculate_plot::support {

// SHA-256 (FIPS 180-4). Used to prove that licence texts are verbatim copies of their upstream files.
class Sha256 {
public:
    Sha256();
    void update(std::string_view bytes);
    std::string hexDigest();  // finishes the hash; call once

private:
    void compress(const unsigned char* block);

    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t length_ = 0;  // bytes hashed so far
};

std::string sha256Hex(std::string_view bytes);

}  // namespace calculate_plot::support
