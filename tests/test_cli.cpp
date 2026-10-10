#include "cli.hpp"

#include "calculate-plot/calculate-plot.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

struct Outcome {
    int status;
    std::string out;
    std::string err;
};

Outcome cplot(std::vector<std::string> args) {
    std::ostringstream out, err;
    const int status = calculate_plot::cli::run(args, out, err);
    return {status, out.str(), err.str()};
}

}  // namespace

TEST(Cli, Version) {
    const Outcome r = cplot({"--version"});
    EXPECT_EQ(r.status, 0);
    EXPECT_EQ(r.out, "cplot " + std::string(calculate_plot::versionString()) + "\n");
    EXPECT_EQ(r.err, "");
}

TEST(Cli, LicensesPrintsEveryNoticeVerbatim) {
    const Outcome r = cplot({"--licenses"});
    EXPECT_EQ(r.status, 0);
    std::string expected;
    for (const auto& item : calculate_plot::licenceTexts())
        expected += "==> " + std::string(item.path) + " <==\n" + std::string(item.text) + "\n";
    EXPECT_EQ(r.out, expected);
}

TEST(Cli, HelpMentionsTheLicences) {
    const Outcome r = cplot({"--help"});
    EXPECT_EQ(r.status, 0);
    EXPECT_NE(r.out.find("--licenses"), std::string::npos);
    EXPECT_NE(r.out.find("--version"), std::string::npos);
}

TEST(Cli, NoArgumentsShowsUsageAndFails) {
    const Outcome r = cplot({});
    EXPECT_EQ(r.status, 2);
    EXPECT_NE(r.err.find("usage"), std::string::npos);
}

TEST(Cli, AnUnknownOptionFails) {
    const Outcome r = cplot({"--frobnicate"});
    EXPECT_EQ(r.status, 2);
    EXPECT_NE(r.err.find("--frobnicate"), std::string::npos);
    EXPECT_EQ(r.out, "");
}
