#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace calculate_plot::support {

// Decodes standard base64 (RFC 4648, with padding); nullopt for malformed text.
std::optional<std::string> decodeBase64(std::string_view text);

// Encodes standard base64 with padding, on one line.
std::string encodeBase64(std::string_view bytes);

}  // namespace calculate_plot::support
