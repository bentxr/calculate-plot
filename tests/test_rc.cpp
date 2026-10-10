#include "rc/rc_params.hpp"

#include <gtest/gtest.h>

using calculate_plot::rc::Params;
using calculate_plot::support::Json;

namespace {

Json json(const std::string& text) { return *calculate_plot::support::parseJson(text, nullptr); }

}  // namespace

TEST(RcParams, StartsWithMatplotlibsDefaults) {
    const Params p = Params::defaults();
    EXPECT_EQ(p.size(), 339u);
    EXPECT_EQ(p.number("lines.linewidth"), 1.5);
    EXPECT_EQ(p.number("font.size"), 10.0);
    EXPECT_EQ(p.numbers("figure.figsize"), (std::vector<double>{6.4, 4.8}));
    EXPECT_EQ(p.number("figure.dpi"), 100.0);
    EXPECT_EQ(p.text("text.hinting"), "default");
    EXPECT_TRUE(p.boolean("axes.unicode_minus"));
    EXPECT_EQ(p.strings("font.sans-serif").front(), "DejaVu Sans");
    const auto cycle = p.colorCycle();
    ASSERT_EQ(cycle.size(), 10u);
    EXPECT_EQ(cycle[0], (std::vector<double>{0.12156862745098039, 0.4666666666666667, 0.7058823529411765}));
}

TEST(RcParams, SettingAValueChecksItsKind) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("lines.linewidth", json("2.5"), &error)) << error;
    EXPECT_EQ(p.number("lines.linewidth"), 2.5);
    EXPECT_TRUE(p.set("figure.figsize", json("[3.2, 2.4]"), &error)) << error;
    EXPECT_EQ(p.numbers("figure.figsize"), (std::vector<double>{3.2, 2.4}));
    EXPECT_FALSE(p.set("lines.linewidth", json("\"thick\""), &error));
    EXPECT_NE(error.find("lines.linewidth"), std::string::npos) << error;
    EXPECT_FALSE(p.set("no.such.key", json("1"), &error));
    EXPECT_NE(error.find("no.such.key"), std::string::npos) << error;
    EXPECT_FALSE(p.set("axes.unicode_minus", json("3"), &error));
}

TEST(RcParams, IntegersAreAcceptedWhereFloatsAre) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("font.size", json("12"), &error)) << error;
    EXPECT_EQ(p.number("font.size"), 12.0);
}
