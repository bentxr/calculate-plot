#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace calculate_plot::cli {

// Runs `cplot` with the arguments after the program name; returns the exit status.
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace calculate_plot::cli
