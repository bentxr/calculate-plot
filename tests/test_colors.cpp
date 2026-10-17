// matplotlib.colors.to_rgba, ported: 6503 specifications recorded from matplotlib give the same
// RGBA, bit for bit, or the same refusal.
#include "colors/colors.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace c = calculate_plot::colors;

TEST(Colors, ToRgbaMatchesMatplotlib) {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/tests/oracle/colors.json");
    std::ostringstream text;
    text << in.rdbuf();
    const auto cases = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(cases.has_value());
    const auto rc = calculate_plot::rc::Params::defaults();
    int checked = 0, mismatches = 0;
    for (const auto& k : cases->items()) {
        const std::string spec = k.find("spec")->asString();
        std::optional<double> alpha;
        if (!k.find("alpha")->isNull()) alpha = k.find("alpha")->asNumber();
        std::string error;
        const auto got = c::toRgba(spec, alpha, rc, &error);
        std::string want = "error", have = got ? "" : "error";
        if (const auto* rgba = k.find("rgba")) {
            want.clear();
            for (const auto& x : rgba->items()) want += x.asString() + " ";
        }
        if (got) {
            char buffer[64];
            for (double x : *got) {
                std::snprintf(buffer, sizeof buffer, "%a ", x);
                have += buffer;
            }
            // Python's float.hex spells 1.0 as 0x1.0000000000000p+0; printf's %a as 0x1p+0: compare values.
            want.clear();
            for (const auto& x : k.find("rgba") ? k.find("rgba")->items() : std::vector<calculate_plot::support::Json>{}) {
                std::snprintf(buffer, sizeof buffer, "%a ", std::strtod(x.asString().c_str(), nullptr));
                want += buffer;
            }
            if (!k.find("rgba")) want = "error";
        }
        if (have != want && ++mismatches <= 10)
            ADD_FAILURE() << "to_rgba(" << spec << ", " << (alpha ? std::to_string(*alpha) : "None") << "): ours " << have
                          << " matplotlib " << want << " " << error;
        ++checked;
    }
    EXPECT_EQ(checked, 6503);
    EXPECT_EQ(mismatches, 0);
}

TEST(Colors, TuplesAreCheckedAndTakeTheAlpha) {
    std::string error;
    EXPECT_EQ(*c::toRgba(std::vector<double>{0.1, 0.2, 0.3}, std::nullopt, &error),
              (std::array<double, 4>{0.1, 0.2, 0.3, 1.0}));
    EXPECT_EQ(*c::toRgba(std::vector<double>{0.1, 0.2, 0.3, 0.4}, 0.9, &error),
              (std::array<double, 4>{0.1, 0.2, 0.3, 0.9}));
    EXPECT_FALSE(c::toRgba(std::vector<double>{0.1, 0.2}, std::nullopt, &error).has_value());
    EXPECT_FALSE(c::toRgba(std::vector<double>{0.1, 1.2, 0.3}, std::nullopt, &error).has_value());
    EXPECT_FALSE(c::toRgba(std::vector<double>{0.1, 0.2, 0.3}, 1.5, &error).has_value());
}

// "C" and digits is a colour of the cycle however many the digits (Python's integers have no end).
// _has_alpha_channel, for valid colours: the hex strings with a fourth pair or digit, and RGBA tuples.
TEST(Colors, AnAlphaChannelIsAFourthComponent) {
    using calculate_plot::colors::hasAlphaChannel;
    using calculate_plot::colors::Spec;
    EXPECT_TRUE(hasAlphaChannel(Spec(std::string("#fff8"))));
    EXPECT_TRUE(hasAlphaChannel(Spec(std::string("#0f0f0f80"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("#fff"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("#0f0f0f"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("red"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("none"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("0.75"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string("tab:blue_"))));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::string(""))));
    EXPECT_TRUE(hasAlphaChannel(Spec(std::vector<double>{1, 0, 0, 0.5})));
    EXPECT_FALSE(hasAlphaChannel(Spec(std::vector<double>{1, 0, 0})));
}

TEST(Colors, TheNthColourOfTheCycleTakesAnyNumberOfDigits) {
    const auto rc = calculate_plot::rc::Params::defaults();
    const auto c = [&](const std::string& spec) { return calculate_plot::colors::toRgba(spec, std::nullopt, rc, nullptr); };
    ASSERT_TRUE(c("C12345678901234567890").has_value());
    EXPECT_EQ(*c("C12345678901234567890"), *c("C0"));  // 12345678901234567890 % 10
    EXPECT_EQ(*c("C12345678901234567897"), *c("C7"));
    EXPECT_EQ(*c("C340282366920938463463374607431768211457"), *c("C7"));
    EXPECT_EQ(*c("C0017"), *c("C7"));
}
