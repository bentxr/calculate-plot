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

// validate_string_or_None: a string, or None; "none" in any case of letters is None as well.
TEST(RcParams, StringsThatMayBeNone) {
    for (const char* key : {"text.language", "svg.hashsalt", "svg.id"}) {
        Params p = Params::defaults();
        std::string error;
        EXPECT_TRUE(p.value(key).isNull()) << key;
        ASSERT_TRUE(p.set(key, json("\"es\""), &error)) << error;
        EXPECT_EQ(p.text(key), "es");
        for (const char* none : {"null", "\"None\"", "\"none\"", "\"NONE\"", "\"nOnE\""}) {
            ASSERT_TRUE(p.set(key, json("\"es\""), &error)) << error;
            ASSERT_TRUE(p.set(key, json(none), &error)) << none << ": " << error;
            EXPECT_TRUE(p.value(key).isNull()) << key << " " << none;
        }
        ASSERT_TRUE(p.set(key, json("\"nones\""), &error)) << error;
        EXPECT_EQ(p.text(key), "nones");
        ASSERT_TRUE(p.set(key, json("\"\""), &error)) << error;
        EXPECT_EQ(p.text(key), "");
        for (const char* bad : {"5", "true", "[\"es\"]", "1.5"}) {
            EXPECT_FALSE(p.set(key, json(bad), &error)) << key << " " << bad;
            EXPECT_EQ(error, std::string("the value given for ") + key + " is not of the right kind");
            EXPECT_EQ(p.text(key), "") << key << " " << bad;
        }
    }
}

// The mathtext settings that are a choice take only matplotlib's names (ValidateInStrings and
// _validate_mathtext_fallback): the fallback also None, in any case of letters.
TEST(RcParams, MathtextChoicesAreChecked) {
    Params p = Params::defaults();
    std::string error;
    for (const char* name : {"dejavusans", "dejavuserif", "cm", "stix", "stixsans", "custom"})
        EXPECT_TRUE(p.set("mathtext.fontset", json(std::string("\"") + name + "\""), &error)) << name << error;
    EXPECT_EQ(p.text("mathtext.fontset"), "custom");
    EXPECT_FALSE(p.set("mathtext.fontset", json("\"nosuchset\""), &error));
    EXPECT_NE(error.find("'nosuchset' is not a valid value for mathtext.fontset"), std::string::npos) << error;
    EXPECT_FALSE(p.set("mathtext.fontset", json("\"STIX\""), &error));
    EXPECT_FALSE(p.set("mathtext.fontset", json("null"), &error));
    EXPECT_EQ(p.text("mathtext.fontset"), "custom");

    for (const char* name : {"rm", "cal", "bfit", "it", "tt", "sf", "bf", "default", "bb", "frak", "scr", "regular", "normal"})
        EXPECT_TRUE(p.set("mathtext.default", json(std::string("\"") + name + "\""), &error)) << name << error;
    EXPECT_FALSE(p.set("mathtext.default", json("\"bold\""), &error));
    EXPECT_NE(error.find("'bold' is not a valid value for mathtext.default"), std::string::npos) << error;
    EXPECT_EQ(p.text("mathtext.default"), "normal");

    EXPECT_TRUE(p.set("mathtext.fallback", json("\"STIX\""), &error)) << error;
    EXPECT_EQ(p.text("mathtext.fallback"), "stix");
    EXPECT_TRUE(p.set("mathtext.fallback", json("\"stixsans\""), &error)) << error;
    EXPECT_EQ(p.text("mathtext.fallback"), "stixsans");
    EXPECT_TRUE(p.set("mathtext.fallback", json("\"cm\""), &error)) << error;
    EXPECT_TRUE(p.set("mathtext.fallback", json("null"), &error)) << error;
    EXPECT_TRUE(p.value("mathtext.fallback").isNull());
    EXPECT_TRUE(p.set("mathtext.fallback", json("\"cm\""), &error)) << error;
    EXPECT_TRUE(p.set("mathtext.fallback", json("\"None\""), &error)) << error;
    EXPECT_TRUE(p.value("mathtext.fallback").isNull());
    EXPECT_FALSE(p.set("mathtext.fallback", json("\"arial\""), &error));
    EXPECT_NE(error.find("arial is not a valid fallback font name"), std::string::npos) << error;
    EXPECT_FALSE(p.set("mathtext.fallback", json("3"), &error));
    EXPECT_TRUE(p.value("mathtext.fallback").isNull());
}

// The font lists (validate_stringlist) also take one string: its parts between commas, trimmed,
// the empty ones dropped; in a list an empty string is dropped too.
TEST(RcParams, FontListsTakeAStringOfNames) {
    Params p = Params::defaults();
    std::string error;
    using Names = std::vector<std::string>;
    ASSERT_TRUE(p.set("font.family", json("\"serif\""), &error)) << error;
    EXPECT_EQ(p.strings("font.family"), Names{"serif"});
    ASSERT_TRUE(p.set("font.family", json("\" DejaVu Serif ,monospace,, \""), &error)) << error;
    EXPECT_EQ(p.strings("font.family"), (Names{"DejaVu Serif", "monospace"}));
    ASSERT_TRUE(p.set("font.family", json("\"\""), &error)) << error;
    EXPECT_EQ(p.strings("font.family"), Names{});
    ASSERT_TRUE(p.set("font.family", json("[\"cursive\", \"\", \" sans \"]"), &error)) << error;
    EXPECT_EQ(p.strings("font.family"), (Names{"cursive", " sans "}));
    for (const char* key : {"font.serif", "font.sans-serif", "font.cursive", "font.fantasy", "font.monospace"}) {
        ASSERT_TRUE(p.set(key, json("\"DejaVu Sans Mono, x\""), &error)) << key << error;
        EXPECT_EQ(p.strings(key), (Names{"DejaVu Sans Mono", "x"})) << key;
    }
    EXPECT_FALSE(p.set("font.family", json("[\"serif\", 3]"), &error));
    EXPECT_NE(error.find("font.family"), std::string::npos) << error;
    EXPECT_FALSE(p.set("font.family", json("3"), &error));
    EXPECT_FALSE(p.set("font.family", json("null"), &error));
    EXPECT_EQ(p.strings("font.family"), (Names{"cursive", " sans "}));
    // another list does not take a string
    EXPECT_FALSE(p.set("figure.figsize", json("\"6, 4\""), &error));
}

TEST(RcParams, IntegersAreAcceptedWhereFloatsAre) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("font.size", json("12"), &error)) << error;
    EXPECT_EQ(p.number("font.size"), 12.0);
}
