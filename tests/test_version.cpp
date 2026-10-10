#include "calculate-plot/calculate-plot.hpp"

#include <gtest/gtest.h>

#include <regex>
#include <string>

TEST(Version, IsMajorMinorPatch) {
    const std::string version(calculate_plot::versionString());
    EXPECT_TRUE(std::regex_match(version, std::regex(R"(\d+\.\d+\.\d+)"))) << version;
}
