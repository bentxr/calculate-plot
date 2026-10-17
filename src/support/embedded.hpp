#pragma once

#include <string_view>

namespace calculate_plot::embedded {

// A data file compiled into the library (see EMBEDDED_FILES in CMakeLists.txt); empty if unknown.
std::string_view file(std::string_view path);

}  // namespace calculate_plot::embedded
