#include "support/glob.hpp"

#include <gtest/gtest.h>

using calculate_plot::support::globMatch;

TEST(Glob, LiteralPaths) {
    EXPECT_TRUE(globMatch("src/a.cpp", "src/a.cpp"));
    EXPECT_FALSE(globMatch("src/a.cpp", "src/a.cp"));
    EXPECT_FALSE(globMatch("src/a.cpp", "src/a.cppx"));
    EXPECT_FALSE(globMatch("a", ""));
    EXPECT_TRUE(globMatch("", ""));
}

TEST(Glob, StarStaysInsideOneSegment) {
    EXPECT_TRUE(globMatch("src/*.cpp", "src/a.cpp"));
    EXPECT_TRUE(globMatch("src/*.cpp", "src/.cpp"));
    EXPECT_FALSE(globMatch("src/*.cpp", "src/x/a.cpp"));
    EXPECT_TRUE(globMatch("*", "README.md"));
    EXPECT_FALSE(globMatch("*", "a/b"));
    EXPECT_TRUE(globMatch("a*b*c", "aXXbYYc"));
    EXPECT_FALSE(globMatch("a*b*c", "aXXbYY"));
}

TEST(Glob, QuestionMarkIsOneCharacterNotASlash) {
    EXPECT_TRUE(globMatch("a?c", "abc"));
    EXPECT_FALSE(globMatch("a?c", "a/c"));
    EXPECT_FALSE(globMatch("a?c", "ac"));
}

TEST(Glob, DoubleStarCrossesSegments) {
    EXPECT_TRUE(globMatch("extern/**", "extern/agg/include/agg_basics.h"));
    EXPECT_TRUE(globMatch("extern/**", "extern/x"));
    EXPECT_FALSE(globMatch("extern/**", "externx/y"));
    EXPECT_TRUE(globMatch("**/agg_conv_gpc.h", "agg_conv_gpc.h"));
    EXPECT_TRUE(globMatch("**/agg_conv_gpc.h", "extern/agg/include/agg_conv_gpc.h"));
    EXPECT_FALSE(globMatch("**/agg_conv_gpc.h", "extern/my_agg_conv_gpc.h"));
    EXPECT_TRUE(globMatch("src/**/*.hpp", "src/a.hpp"));
    EXPECT_TRUE(globMatch("src/**/*.hpp", "src/x/y/a.hpp"));
    EXPECT_FALSE(globMatch("src/**/*.hpp", "src/x/y/a.cpp"));
    EXPECT_TRUE(globMatch("**/__pycache__/**", "tools/oracle/__pycache__/x.pyc"));
}
