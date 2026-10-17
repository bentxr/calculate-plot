#include "cli.hpp"

#include <iostream>

int main(int argc, char** argv) {
    return calculate_plot::cli::run(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr);
}
