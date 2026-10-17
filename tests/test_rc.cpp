#include "rc/rc_params.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <utility>

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
    ASSERT_EQ(cycle[0].size(), 3u);
    EXPECT_EQ(cycle[0][0].asNumber(), 0.12156862745098039);
    EXPECT_EQ(cycle[0][1].asNumber(), 0.4666666666666667);
    EXPECT_EQ(cycle[0][2].asNumber(), 0.7058823529411765);
    const auto entries = p.propCycle();
    ASSERT_EQ(entries.size(), 10u);
    ASSERT_EQ(entries[9].size(), 1u);
    EXPECT_EQ(entries[9][0].first, "color");
    EXPECT_EQ(entries[9][0].second[0].asNumber(), 0.09019607843137255);
}

// rcParams["axes.prop_cycle"] is set as the text that makes the cycle (as a style sheet writes it).
TEST(RcParams, APropertyCycleIsSetAsText) {
    Params p = Params::defaults();
    std::string error;
    ASSERT_TRUE(p.set("axes.prop_cycle", json("\"cycler(color='rg') + cycler(lw=[1, 2.5])\""), &error)) << error;
    const auto entries = p.propCycle();
    ASSERT_EQ(entries.size(), 2u);
    ASSERT_EQ(entries[0].size(), 2u);
    EXPECT_EQ(entries[0][0].first, "color");
    EXPECT_EQ(entries[0][0].second.asString(), "r");
    EXPECT_EQ(entries[0][1].first, "linewidth");
    EXPECT_EQ(entries[0][1].second.asNumber(), 1.0);
    EXPECT_EQ(entries[1][0].second.asString(), "g");
    EXPECT_EQ(entries[1][1].second.asNumber(), 2.5);
    ASSERT_EQ(p.colorCycle().size(), 2u);
    EXPECT_EQ(p.colorCycle()[1].asString(), "g");
    // a cycle without colours: "C0" and the rest are black
    ASSERT_TRUE(p.set("axes.prop_cycle", json("\"cycler(ls=['-', '--', ':'])\""), &error)) << error;
    EXPECT_EQ(p.propCycle().size(), 3u);
    ASSERT_EQ(p.colorCycle().size(), 1u);
    EXPECT_EQ(p.colorCycle()[0].asString(), "k");
    // every kind of value the engine goes through
    EXPECT_TRUE(p.set("axes.prop_cycle",
                      json("\"cycler(color=['r', (0, 0.5, 1), '#00ff0080', '0.3'], lw=[1, 2, 3, 4], ls=['-', '--', ':', 'None'], "
                           "marker=['o', 's', 'x', 'None'], ms=[1, 2, 3, 4], mew=[1, 2, 3, 4], mec='rgbk', mfc='rgbk', alpha=[0, 0.5, 1, 1], "
                           "fc='rgbk', ec='rgbk')\""),
                      &error))
        << error;
    EXPECT_EQ(p.propCycle().size(), 4u);
    EXPECT_EQ(p.propCycle()[0].size(), 11u);
    // what matplotlib takes and the engine does not go through yet
    for (const char* text : {"cycler(color=[])", "cycler(color='rg') * 0", "cycler(color=[('red', 0.5)])", "cycler(mfc=[('red', 0.5)])",
                             "cycler(ls=[(0, (1, 2))])", "cycler(marker=[1, 2])", "cycler(lw=['inf'])", "cycler(alpha=['nan'])",
                             "cycler(color='r', ms=[1e400])"}) {
        EXPECT_FALSE(p.set("axes.prop_cycle", Json::string(text), &error)) << text;
        EXPECT_EQ(error, "this value of axes.prop_cycle is not supported yet") << text;
    }
    EXPECT_EQ(p.propCycle().size(), 4u);
    // what matplotlib refuses
    EXPECT_FALSE(p.set("axes.prop_cycle", json("\"cycler(colour='rg')\""), &error));
    EXPECT_EQ(error, "the value given for axes.prop_cycle is not valid: 'cycler(colour='rg')' is not a valid cycler construction: "
                     "Unknown artist property: colour");
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
            EXPECT_EQ(error, std::string("the value given for ") + key + " is not valid: could not convert the value to str");
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
    // the other lists take a string as well
    ASSERT_TRUE(p.set("figure.figsize", json("\"6, 4.5\""), &error)) << error;
    EXPECT_EQ(p.numbers("figure.figsize"), (std::vector<double>{6.0, 4.5}));
    EXPECT_FALSE(p.set("figure.figsize", json("\"6, 4, 3\""), &error));
    EXPECT_EQ(error, "the value given for figure.figsize is not valid: expected 2 values, but there are 3");
}

TEST(RcParams, IntegersAreAcceptedWhereFloatsAre) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("font.size", json("12"), &error)) << error;
    EXPECT_EQ(p.number("font.size"), 12.0);
}

// A value is stored as matplotlib's validator of the key stores it.
TEST(RcParams, ValuesAreStoredAsMatplotlibStoresThem) {
    Params p = Params::defaults();
    std::string error;
    ASSERT_TRUE(p.set("axes.grid", json("\"Yes\""), &error)) << error;
    EXPECT_TRUE(p.value("axes.grid").isBool());
    EXPECT_TRUE(p.boolean("axes.grid"));
    ASSERT_TRUE(p.set("axes.grid", json("0"), &error)) << error;
    EXPECT_TRUE(p.value("axes.grid").isBool());
    EXPECT_FALSE(p.boolean("axes.grid"));
    ASSERT_TRUE(p.set("lines.linewidth", json("\" 2.5 \""), &error)) << error;
    EXPECT_EQ(p.number("lines.linewidth"), 2.5);
    ASSERT_TRUE(p.set("lines.linewidth", json("true"), &error)) << error;
    EXPECT_TRUE(p.value("lines.linewidth").isNumber());
    EXPECT_EQ(p.number("lines.linewidth"), 1.0);
    ASSERT_TRUE(p.set("lines.linestyle", json("\"DASHED\""), &error)) << error;
    EXPECT_EQ(p.text("lines.linestyle"), "dashed");
    ASSERT_TRUE(p.set("axes.titlesize", json("\"X-Large\""), &error)) << error;
    EXPECT_EQ(p.text("axes.titlesize"), "x-large");
    ASSERT_TRUE(p.set("axes.titlesize", json("\"14\""), &error)) << error;
    EXPECT_EQ(p.number("axes.titlesize"), 14.0);
    ASSERT_TRUE(p.set("axes.facecolor", json("\"ff000080\""), &error)) << error;
    EXPECT_EQ(p.text("axes.facecolor"), "#ff000080");
    ASSERT_TRUE(p.set("axes.facecolor", json("\"NONE\""), &error)) << error;
    EXPECT_EQ(p.text("axes.facecolor"), "none");
    ASSERT_TRUE(p.set("savefig.bbox", json("\"Tight\""), &error)) << error;
    EXPECT_EQ(p.text("savefig.bbox"), "tight");
    ASSERT_TRUE(p.set("savefig.bbox", json("\"standard\""), &error)) << error;
    EXPECT_TRUE(p.value("savefig.bbox").isNull());
    ASSERT_TRUE(p.set("font.weight", json("700.9"), &error)) << error;
    EXPECT_TRUE(p.value("font.weight").isInteger());
    EXPECT_EQ(p.number("font.weight"), 700.0);
    // what matplotlib refuses is refused, with the key in the message
    EXPECT_FALSE(p.set("axes.grid", json("2"), &error));
    EXPECT_EQ(error, "the value given for axes.grid is not valid: cannot convert the value to bool");
    EXPECT_FALSE(p.set("axes.facecolor", json("\"nocolor\""), &error));
    EXPECT_EQ(error, "the value given for axes.facecolor is not valid: the value does not look like a color arg");
    EXPECT_FALSE(p.set("xtick.direction", json("\"IN\""), &error));
    EXPECT_EQ(error, "the value given for xtick.direction is not valid: 'IN' is not a valid value for xtick.direction; "
                     "supported values are out in inout");
    EXPECT_FALSE(p.set("axes.xmargin", json("-0.5"), &error));
    EXPECT_EQ(error, "the value given for axes.xmargin is not valid: the value must be >-0.5");
    EXPECT_EQ(p.number("axes.xmargin"), 0.05);
}

// What matplotlib takes and the engine does not read yet is refused as such, never taken and ignored.
TEST(RcParams, ValuesTheEngineDoesNotReadYetAreRefused) {
    Params p = Params::defaults();
    std::string error;
    const std::pair<const char*, const char*> cases[] = {
        {"lines.linestyle", "[0, [1, 2]]"},           // an on-off sequence
        {"lines.linestyle", "\"(0, (1, 2))\""},
        {"lines.color", "[0.1, 0.2, 0.3]"},            // a colour as a tuple
        {"lines.color", "\"(0.1, 0.2, 0.3)\""},
        {"lines.color", "{\"$tuple\": [\"red\", 0.5]}"},
        {"lines.marker", "5"},                         // a marker by its number
        {"legend.loc", "3"},
        {"legend.loc", "{\"$tuple\": [0.5, 0.5]}"},
        {"lines.linewidth", "{\"$float\": \"nan\"}"},  // numbers that are not finite
        {"lines.linewidth", "\"inf\""},
        {"axes.titley", "{\"$float\": \"inf\"}"},
        {"figure.figsize", "[6.4, {\"$float\": \"nan\"}]"},
        {"axes.formatter.offset_threshold", "24"},     // 10 ** 23 is not a double
        {"axes.formatter.offset_threshold", "3000000000"},  // integers beyond 32 bits
        {"axes.formatter.min_exponent", "-3000000000"},
        {"axes.formatter.limits", "[-5, 2147483648]"},
        {"legend.numpoints", "1e10"},
        {"xtick.minor.ndivs", "\"AUTO\""},            // taken by matplotlib, and then AutoMinorLocator breaks
        {"ytick.minor.ndivs", "\"Auto\""},
        {"ytick.minor.ndivs", "3000000000"},
    };
    EXPECT_TRUE(p.set("xtick.minor.ndivs", json("\"auto\""), &error)) << error;
    EXPECT_TRUE(p.set("ytick.minor.ndivs", json("0"), &error)) << error;
    EXPECT_TRUE(p.set("ytick.minor.ndivs", json("\"7\""), &error)) << error;
    EXPECT_EQ(p.number("ytick.minor.ndivs"), 7.0);
    EXPECT_TRUE(p.set("axes.formatter.offset_threshold", json("23"), &error)) << error;
    EXPECT_TRUE(p.set("axes.formatter.limits", json("\"-2147483647, 2147483647\""), &error)) << error;
    EXPECT_TRUE(p.set("axes.formatter.limits", json("[-5, 6]"), &error)) << error;
    EXPECT_TRUE(p.set("lines.linewidth", json("1e10"), &error)) << error;  // (a float may be large)
    EXPECT_TRUE(p.set("lines.linewidth", json("1.5"), &error)) << error;
    EXPECT_TRUE(p.set("axes.formatter.offset_threshold", json("4"), &error)) << error;
    for (const auto& [key, value] : cases) {
        const auto before = p.value(key);
        EXPECT_FALSE(p.set(key, json(value), &error)) << key << " " << value;
        EXPECT_EQ(error, std::string("this value of ") + key + " is not supported yet") << value;
    }
    // text that only Python reads
    EXPECT_FALSE(p.set("axes.prop_cycle", json("\"cycler(color=u'rg')\""), &error));
    EXPECT_EQ(error, "this spelling of a Python literal is not supported yet");
    EXPECT_FALSE(p.set("lines.linewidth", json("\"\u0665\""), &error));
    EXPECT_EQ(error, "a number in the digits of another script is not supported yet");
    EXPECT_EQ(p.text("lines.linestyle"), "-");
    EXPECT_EQ(p.number("lines.linewidth"), 1.5);
}

// The classic style sets _internal.classic_mode, which a few defaults look at.
TEST(RcParams, TheClassicModeIsABool) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_FALSE(p.boolean("_internal.classic_mode"));
    EXPECT_TRUE(p.set("_internal.classic_mode", json("true"), &error)) << error;
    EXPECT_TRUE(p.boolean("_internal.classic_mode"));
    EXPECT_TRUE(p.set("_internal.classic_mode", json("\"no\""), &error)) << error;
    EXPECT_FALSE(p.boolean("_internal.classic_mode"));
    EXPECT_FALSE(p.set("_internal.classic_mode", json("\"classic\""), &error));
}

// legend.framealpha may be None: the frame keeps the alpha of its colours.
TEST(RcParams, TheLegendsFrameAlphaMayBeNone) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_EQ(p.number("legend.framealpha"), 0.8);
    EXPECT_TRUE(p.set("legend.framealpha", json("null"), &error)) << error;
    EXPECT_TRUE(p.value("legend.framealpha").isNull());
    EXPECT_TRUE(p.set("legend.framealpha", json("\"None\""), &error)) << error;
    EXPECT_TRUE(p.value("legend.framealpha").isNull());
    EXPECT_TRUE(p.set("legend.framealpha", json("0.25"), &error)) << error;
    EXPECT_EQ(p.number("legend.framealpha"), 0.25);
    // (a key whose None is not read yet)
    EXPECT_FALSE(p.set("legend.fontsize", json("null"), &error));
}

// axes.axisbelow is 'line' by default, and True or False in any spelling of a bool.
TEST(RcParams, AxisbelowIsABoolOrLine) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_EQ(p.value("axes.axisbelow").asString(), "line");
    for (const char* value : {"true", "\"True\"", "\"yes\"", "1"}) {
        EXPECT_TRUE(p.set("axes.axisbelow", json(value), &error)) << value << error;
        EXPECT_TRUE(p.value("axes.axisbelow").isBool() && p.value("axes.axisbelow").asBool()) << value;
    }
    for (const char* value : {"false", "\"off\"", "0"}) {
        EXPECT_TRUE(p.set("axes.axisbelow", json(value), &error)) << value << error;
        EXPECT_TRUE(p.value("axes.axisbelow").isBool() && !p.value("axes.axisbelow").asBool()) << value;
    }
    EXPECT_TRUE(p.set("axes.axisbelow", json("\"line\""), &error)) << error;
    EXPECT_EQ(p.value("axes.axisbelow").asString(), "line");
    EXPECT_FALSE(p.set("axes.axisbelow", json("\"lines\""), &error));
    EXPECT_FALSE(p.set("axes.axisbelow", json("2"), &error));
    EXPECT_EQ(p.value("axes.axisbelow").asString(), "line");
}

// The keys of what the engine does not do yet take their default value and no other.
TEST(RcParams, KeysThatAreNotHonouredYetTakeOnlyTheirDefault) {
    const std::pair<const char*, const char*> other[] = {
        {"axes.formatter.use_locale", "true"},
        {"date.converter", "\"concise\""},
        {"figure.autolayout", "true"},
        {"figure.constrained_layout.use", "true"},
        {"figure.hooks", "[\"os:getcwd\"]"},
        {"font.enable_last_resort", "false"},
        {"path.effects", "[1]"},
        {"path.sketch", "[1, 100, 2]"},
        {"path.snap", "false"},
        {"text.hinting_factor", "8"},
        {"text.kerning_factor", "6"},
        {"text.usetex", "true"},
        {"timezone", "\"Europe/Madrid\""},
    };
    const auto& listed = Params::unsupportedKeys();
    EXPECT_EQ(listed.size(), std::size(other) + 2);  // and the backend, which takes nothing, and date.epoch
    const Params defaults = Params::defaults();
    for (const auto& [key, value] : other) {
        EXPECT_NE(std::find(listed.begin(), listed.end(), key), listed.end()) << key;
        Params p = Params::defaults();
        std::string error;
        EXPECT_FALSE(p.set(key, json(value), &error)) << key;
        EXPECT_EQ(error, std::string("rcParams[\"") + key + "\"] is not supported yet");
        // its default, also in another spelling, is taken
        EXPECT_TRUE(p.set(key, defaults.value(key), &error)) << key << ": " << error;
    }
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("figure.autolayout", json("\"no\""), &error)) << error;
    EXPECT_TRUE(p.set("path.sketch", json("\"None\""), &error)) << error;
    EXPECT_FALSE(p.set("figure.autolayout", json("\"maybe\""), &error));
    EXPECT_EQ(error, "the value given for figure.autolayout is not valid: cannot convert the value to bool");
    // a value of another kind than the default is not the default
    EXPECT_FALSE(p.set("text.kerning_factor", json("0"), &error));
    EXPECT_EQ(error, "rcParams[\"text.kerning_factor\"] is not supported yet");
    EXPECT_FALSE(p.set("path.snap", json("\"no\""), &error));
    EXPECT_TRUE(p.set("path.snap", json("\"yes\""), &error)) << error;
    EXPECT_FALSE(p.set("text.kerning_factor", json("0"), &error));
    EXPECT_FALSE(p.set("backend", json("\"agg\""), &error));
    EXPECT_EQ(error, "the choice of a backend is not supported yet");
    // date.epoch: its default only (its validator, np.datetime64 of the text, is not ported)
    EXPECT_FALSE(p.set("date.epoch", json("\"2000-01-01T00:00:00\""), &error));
    EXPECT_EQ(error, "a date is not supported yet");
    EXPECT_FALSE(p.set("date.epoch", json("\"1970-01-01\""), &error));
    EXPECT_TRUE(p.set("date.epoch", json("\"1970-01-01T00:00:00\""), &error)) << error;
}

// The keys of what is not drawn at all (animations, box plots, images, PDF output, the windows of
// the interactive backends...) take whatever matplotlib takes: they change nothing.
TEST(RcParams, KeysOfWhatIsNotDrawnTakeWhatMatplotlibTakes) {
    Params p = Params::defaults();
    std::string error;
    EXPECT_EQ(Params::inertKeys().size(), 87u);
    // (boxplot's keys are read now; they still take any value their validators take)
    EXPECT_TRUE(p.set("boxplot.whiskers", json("[5, 95]"), &error)) << error;
    EXPECT_TRUE(p.set("boxplot.whiskers", json("2.5"), &error)) << error;
    EXPECT_TRUE(p.set("boxplot.flierprops.marker", json("5"), &error)) << error;
    EXPECT_TRUE(p.set("boxplot.boxprops.linestyle", json("[0, [1, 2]]"), &error)) << error;
    EXPECT_TRUE(p.set("axes3d.xaxis.panecolor", json("[0.1, 0.2, 0.3]"), &error)) << error;
    EXPECT_TRUE(p.set("hist.bins", json("7"), &error)) << error;  // (hist reads it: a count, a rule by name, edges)
    EXPECT_TRUE(p.set("hist.bins", json("\"auto\""), &error)) << error;
    EXPECT_TRUE(p.set("hist.bins", json("[0, 1.5, 4]"), &error)) << error;
    EXPECT_FALSE(p.set("hist.bins", json("\"stone\""), &error));  // (not among rcsetup's names)
    EXPECT_TRUE(p.set("pdf.fonttype", json("\"truetype\""), &error)) << error;
    EXPECT_EQ(p.number("pdf.fonttype"), 42.0);
    EXPECT_TRUE(p.set("keymap.save", json("\"s, ctrl+s\""), &error)) << error;
    EXPECT_EQ(p.strings("keymap.save"), (std::vector<std::string>{"s", "ctrl+s"}));
    EXPECT_TRUE(p.set("contour.linewidth", json("{\"$float\": \"inf\"}"), &error)) << error;
    EXPECT_FALSE(p.set("pdf.fonttype", json("5"), &error));
    EXPECT_EQ(error, "the value given for pdf.fonttype is not valid: supported Postscript/PDF font types are 3, 42");
    for (const std::string& key : Params::inertKeys()) {
        EXPECT_TRUE(p.has(key)) << key;
        const auto& refused = Params::unsupportedKeys();
        EXPECT_EQ(std::find(refused.begin(), refused.end(), key), refused.end()) << key;
    }
}

// A font weight or stretch beyond 0-1000 is taken by rcParams and refused by FontProperties as soon
// as a text is made, which every Axes does: it is refused here. figure.titleweight is only used by a
// suptitle, which refuses it then.
TEST(RcParams, FontWeightsAndStretchesAreFromZeroToAThousand) {
    for (const char* key : {"font.weight", "font.stretch", "axes.titleweight", "axes.labelweight"}) {
        Params p = Params::defaults();
        std::string error;
        for (const char* bad : {"1001", "-1", "\"1001\"", "2000.5", "-1.5"}) {
            EXPECT_FALSE(p.set(key, json(bad), &error)) << key << " " << bad;
            EXPECT_EQ(error, std::string("the value given for ") + key + " is not valid: FontProperties takes a name or a number "
                                                                          "from 0 to 1000");
        }
        for (const char* good : {"1000", "0", "\"0\"", "1000.9", "-0.5", "500"})  // (int() cuts the fraction off)
            EXPECT_TRUE(p.set(key, json(good), &error)) << key << " " << good << ": " << error;
        EXPECT_EQ(p.number(key), 500.0);
    }
    Params p = Params::defaults();
    std::string error;
    EXPECT_TRUE(p.set("figure.titleweight", json("1001"), &error)) << error;
    EXPECT_TRUE(p.set("figure.labelweight", json("-7"), &error)) << error;
}

// What FontProperties and the artists refuse as soon as an Axes is made is refused when it is set:
// a font style or variant that is not one, and the transparency of the grid outside 0-1.
TEST(RcParams, ValuesEveryFigureWouldBreakOnAreRefused) {
    Params p = Params::defaults();
    std::string error;
    for (const char* style : {"\"normal\"", "\"italic\"", "\"oblique\""}) EXPECT_TRUE(p.set("font.style", json(style), &error)) << error;
    for (const char* bad : {"\"t\"", "\"Italic\"", "\"\"", "\"slanted\""}) {
        EXPECT_FALSE(p.set("font.style", json(bad), &error)) << bad;
        EXPECT_EQ(error, "the value given for font.style is not valid: FontProperties takes normal, italic or oblique");
    }
    for (const char* variant : {"\"normal\"", "\"small-caps\""}) EXPECT_TRUE(p.set("font.variant", json(variant), &error)) << error;
    for (const char* bad : {"\"smallcaps\"", "\"Normal\"", "\"\""}) {
        EXPECT_FALSE(p.set("font.variant", json(bad), &error)) << bad;
        EXPECT_EQ(error, "the value given for font.variant is not valid: FontProperties takes normal or small-caps");
    }
    for (const char* key : {"grid.alpha", "grid.major.alpha", "grid.minor.alpha"}) {
        for (const char* good : {"0", "1", "0.5", "-0.0", "true"}) EXPECT_TRUE(p.set(key, json(good), &error)) << key << good << error;
        for (const char* bad : {"2", "-0.1", "1.0000001"}) {
            EXPECT_FALSE(p.set(key, json(bad), &error)) << key << bad;
            EXPECT_EQ(error, std::string("the value given for ") + key + " is not valid: an alpha is from 0 to 1");
        }
    }
    EXPECT_TRUE(p.set("grid.major.alpha", json("null"), &error)) << error;
    // a width or a size may be any number
    EXPECT_TRUE(p.set("grid.linewidth", json("2"), &error)) << error;
}

// The grid's own line styles may be None (grid.linestyle then); an on-off sequence is not read yet.
TEST(RcParams, GridLineStylesAreNamesOrNone) {
    Params p = Params::defaults();
    std::string error;
    for (const char* key : {"grid.major.linestyle", "grid.minor.linestyle"}) {
        EXPECT_TRUE(p.set(key, json("\"--\""), &error)) << error;
        EXPECT_EQ(p.text(key), "--");
        EXPECT_TRUE(p.set(key, json("\"Dotted\""), &error)) << error;
        EXPECT_EQ(p.text(key), "dotted");
        EXPECT_TRUE(p.set(key, json("null"), &error)) << error;
        EXPECT_TRUE(p.value(key).isNull());
        for (const char* sequence : {"\"1,5\"", "[1, 5]", "\"(0, (1, 2))\"", "{\"$tuple\": [0, [1, 2]]}"}) {
            EXPECT_FALSE(p.set(key, json(sequence), &error)) << key << sequence;
            EXPECT_EQ(error, std::string("this value of ") + key + " is not supported yet");
        }
        EXPECT_TRUE(p.value(key).isNull());
    }
}
