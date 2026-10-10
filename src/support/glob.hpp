#pragma once

#include <string_view>

namespace calculate_plot::support {

// Matches a '/'-separated relative path against a pattern: '*' and '?' never cross a '/', and a
// "**" segment matches any number of whole segments (also none).
bool globMatch(std::string_view pattern, std::string_view path);

}  // namespace calculate_plot::support
