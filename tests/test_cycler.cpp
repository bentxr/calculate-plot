// Property cycles: the algebra of the cycler package, and a cycle written as text (as a style sheet
// writes rcParams["axes.prop_cycle"]) against matplotlib 3.11.2's reading of it
// (tests/oracle/cycler.json, written by tools/oracle/make_cycler_oracle.py).
#include "rc/cycler.hpp"
#include "rc/rc_validate.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::rc::Cycler;
using calculate_plot::rc::Validated;
using calculate_plot::support::Json;

namespace {

Json parse(const std::string& text) { return *calculate_plot::support::parseJson(text, nullptr); }

std::vector<Json> list(const std::string& text) { return parse(text).items(); }

// A value as compact JSON text (numbers as they are written: 1 is the integer, 1.0 the float).
std::string text(const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: return "null";
    case Json::Type::Bool: return v.asBool() ? "true" : "false";
    case Json::Type::Number: return v.numberText();
    case Json::Type::String: return "\"" + v.asString() + "\"";
    case Json::Type::Array: {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) out += (i ? "," : "") + text(v[i]);
        return out + "]";
    }
    case Json::Type::Object: {
        std::string out = "{";
        for (std::size_t i = 0; i < v.members().size(); ++i)
            out += (i ? ",\"" : "\"") + v.members()[i].first + "\":" + text(v.members()[i].second);
        return out + "}";
    }
    }
    return "?";
}

// The values of a property, as JSON text.
std::string values(const Cycler& c, const std::string& key) { return text(Json::array(c.values(key))); }

// What is stored against what matplotlib stores: an integer is never a float, and a float that is
// not finite is {"$float": ...} in the oracle.
bool same(const Json& got, const Json& want) {
    if (want.isObject() && want.members().size() == 1 && want.members()[0].first == "$float") {
        const std::string& name = want.members()[0].second.asString();
        if (!got.isNumber() || got.isInteger()) return false;
        if (name == "nan") return std::isnan(got.asNumber());
        return std::isinf(got.asNumber()) && (got.asNumber() > 0) == (name == "inf");
    }
    if (got.type() != want.type()) return false;
    switch (got.type()) {
    case Json::Type::Null: return true;
    case Json::Type::Bool: return got.asBool() == want.asBool();
    case Json::Type::Number:
        return got.isInteger() == want.isInteger() && got.asNumber() == want.asNumber() &&
               (!got.isInteger() || got.numberText() == want.numberText());
    case Json::Type::String: return got.asString() == want.asString();
    case Json::Type::Array:
        if (got.size() != want.size()) return false;
        for (std::size_t i = 0; i < got.size(); ++i)
            if (!same(got[i], want[i])) return false;
        return true;
    case Json::Type::Object:
        if (got.members().size() != want.members().size()) return false;
        for (std::size_t i = 0; i < got.members().size(); ++i)
            if (got.members()[i].first != want.members()[i].first || !same(got.members()[i].second, want.members()[i].second))
                return false;
        return true;
    }
    return false;
}

Validated read(const std::string& text) { return calculate_plot::rc::validate("axes.prop_cycle", Json::string(text)); }

}  // namespace

TEST(Cycler, OnePropertyIsItsValues) {
    const Cycler c = Cycler::of("color", list(R"(["r", "g", "b"])"));
    EXPECT_EQ(c.size(), 3u);
    EXPECT_EQ(c.keys(), std::vector<std::string>({"color"}));
    EXPECT_EQ(values(c, "color"), R"(["r","g","b"])");
    EXPECT_EQ(text(c.toJson()), R"({"$cycler":{"color":["r","g","b"]}})");
    EXPECT_EQ(Cycler::of("color", {}).size(), 0u);
}

// The sum pairs two cycles of one length (zip); they cannot share a property.
TEST(Cycler, TheSumPairsTheValues) {
    const Cycler colors = Cycler::of("color", list(R"(["r", "g"])")), widths = Cycler::of("linewidth", list("[1.0, 2.0]"));
    std::string error;
    const auto both = Cycler::add(colors, widths, &error);
    ASSERT_TRUE(both.has_value()) << error;
    EXPECT_EQ(both->size(), 2u);
    EXPECT_EQ(both->keys(), std::vector<std::string>({"color", "linewidth"}));
    EXPECT_EQ(values(*both, "color"), R"(["r","g"])");
    EXPECT_EQ(values(*both, "linewidth"), "[1.0,2.0]");
    // the properties come in alphabetical order whatever the order of the sum
    EXPECT_EQ(Cycler::add(widths, colors, &error)->keys(), std::vector<std::string>({"color", "linewidth"}));
    EXPECT_FALSE(Cycler::add(colors, Cycler::of("linewidth", list("[1.0]")), &error).has_value());
    EXPECT_EQ(error, "Can only add equal length cycles, not 2 and 1");
    EXPECT_FALSE(Cycler::add(colors, Cycler::of("color", list(R"(["b", "k"])")), &error).has_value());
    EXPECT_EQ(error, "Can not compose overlapping cycles");
    // (the package looks at the first item of the right-hand cycle: there is none)
    EXPECT_FALSE(Cycler::add(Cycler::of("color", {}), Cycler::of("linewidth", {}), &error).has_value());
    EXPECT_EQ(error, "an empty cycle cannot be composed");
}

// The product has every value of the right-hand cycle for each of the left-hand one.
TEST(Cycler, TheProductCombinesTheValues) {
    const Cycler colors = Cycler::of("color", list(R"(["r", "g", "b"])")), styles = Cycler::of("linestyle", list(R"(["-", "--"])"));
    std::string error;
    const auto product = Cycler::multiply(colors, styles, &error);
    ASSERT_TRUE(product.has_value()) << error;
    EXPECT_EQ(product->size(), 6u);
    EXPECT_EQ(values(*product, "color"), R"(["r","r","g","g","b","b"])");
    EXPECT_EQ(values(*product, "linestyle"), R"(["-","--","-","--","-","--"])");
    const auto other = Cycler::multiply(styles, colors, &error);
    ASSERT_TRUE(other.has_value()) << error;
    EXPECT_EQ(values(*other, "color"), R"(["r","g","b","r","g","b"])");
    EXPECT_EQ(values(*other, "linestyle"), R"(["-","-","-","--","--","--"])");
    EXPECT_FALSE(Cycler::multiply(colors, Cycler::of("color", list(R"(["k"])")), &error).has_value());
    EXPECT_EQ(error, "Can not compose overlapping cycles");
    EXPECT_FALSE(Cycler::multiply(colors, Cycler::of("linestyle", {}), &error).has_value());
    EXPECT_EQ(error, "an empty cycle cannot be composed");
    EXPECT_FALSE(Cycler::multiply(Cycler::of("linestyle", {}), colors, &error).has_value());
    EXPECT_EQ(error, "an empty cycle cannot be composed");
    // a product of three: the last one turns fastest
    const auto three = Cycler::multiply(*Cycler::multiply(Cycler::of("color", list(R"(["r", "g"])")), Cycler::of("linewidth", list("[1.0, 2.0]")), &error),
                                        Cycler::of("linestyle", list(R"(["-", ":"])")), &error);
    ASSERT_TRUE(three.has_value()) << error;
    EXPECT_EQ(values(*three, "color"), R"(["r","r","r","r","g","g","g","g"])");
    EXPECT_EQ(values(*three, "linewidth"), "[1.0,1.0,2.0,2.0,1.0,1.0,2.0,2.0]");
    EXPECT_EQ(values(*three, "linestyle"), R"(["-",":","-",":","-",":","-",":"])");
}

// A cycle times an integer is the cycle that many times over; none for zero or less.
TEST(Cycler, TimesAnIntegerRepeats) {
    std::string error;
    const Cycler colors = Cycler::of("color", list(R"(["r", "g"])"));
    EXPECT_EQ(values(*colors.times(2, &error), "color"), R"(["r","g","r","g"])");
    EXPECT_EQ(values(*colors.times(1, &error), "color"), R"(["r","g"])");
    EXPECT_EQ(colors.times(0, &error)->size(), 0u);
    EXPECT_EQ(colors.times(-1, &error)->size(), 0u);
    const Cycler two = *Cycler::add(colors, Cycler::of("linewidth", list("[1.0, 2.0]")), &error);
    const auto thrice = two.times(3, &error);
    ASSERT_TRUE(thrice.has_value()) << error;
    EXPECT_EQ(values(*thrice, "color"), R"(["r","g","r","g","r","g"])");
    EXPECT_EQ(values(*thrice, "linewidth"), "[1.0,2.0,1.0,2.0,1.0,2.0]");
    // (the package adds the emptied properties up again: the sum of empty cycles fails)
    EXPECT_FALSE(two.times(0, &error).has_value());
    EXPECT_EQ(error, "an empty cycle cannot be composed");
}

// A slice of a cycle is that slice of the values of each property, as Python slices a list.
TEST(Cycler, ASliceIsThatSliceOfEveryProperty) {
    std::string error;
    const Cycler c = Cycler::of("color", list(R"(["r", "g", "b", "k", "w"])"));
    const auto sliced = [&](std::optional<long long> lower, std::optional<long long> upper, std::optional<long long> step) {
        const auto made = c.slice(lower, upper, step, &error);
        return made ? values(*made, "color") : "error: " + error;
    };
    EXPECT_EQ(sliced({}, 2, {}), R"(["r","g"])");
    EXPECT_EQ(sliced(1, {}, {}), R"(["g","b","k","w"])");
    EXPECT_EQ(sliced({}, {}, 2), R"(["r","b","w"])");
    EXPECT_EQ(sliced({}, {}, -1), R"(["w","k","b","g","r"])");
    EXPECT_EQ(sliced({}, {}, {}), R"(["r","g","b","k","w"])");
    EXPECT_EQ(sliced(-2, {}, {}), R"(["k","w"])");
    EXPECT_EQ(sliced({}, -1, {}), R"(["r","g","b","k"])");
    EXPECT_EQ(sliced(1, 4, 2), R"(["g","k"])");
    EXPECT_EQ(sliced(3, 0, -2), R"(["k","g"])");
    EXPECT_EQ(sliced(-1, {}, -3), R"(["w","g"])");
    EXPECT_EQ(sliced({}, 1, -1), R"(["w","k","b"])");
    EXPECT_EQ(sliced(9, {}, {}), "[]");
    EXPECT_EQ(sliced(-9, 2, {}), R"(["r","g"])");
    EXPECT_EQ(sliced(2, 99, {}), R"(["b","k","w"])");
    EXPECT_EQ(sliced(99, {}, -1), R"(["w","k","b","g","r"])");
    EXPECT_EQ(sliced({}, -99, -1), R"(["w","k","b","g","r"])");
    EXPECT_EQ(sliced(0, 0, {}), "[]");
    EXPECT_EQ(sliced(3, 1, {}), "[]");
    EXPECT_EQ(sliced({}, {}, 0), "error: slice step cannot be zero");
    const Cycler two = *Cycler::add(Cycler::of("color", list(R"(["r", "g", "b"])")), Cycler::of("linewidth", list("[1.0, 2.0, 3.0]")), &error);
    const auto tail = two.slice(1, {}, {}, &error);
    ASSERT_TRUE(tail.has_value()) << error;
    EXPECT_EQ(values(*tail, "color"), R"(["g","b"])");
    EXPECT_EQ(values(*tail, "linewidth"), "[2.0,3.0]");
    EXPECT_FALSE(two.slice(5, {}, {}, &error).has_value());
    EXPECT_EQ(error, "an empty cycle cannot be composed");
}

// concat chains two cycles of the same properties.
TEST(Cycler, ConcatChainsTheValues) {
    std::string error;
    const Cycler rg = Cycler::of("color", list(R"(["r", "g"])")), b = Cycler::of("color", list(R"(["b"])"));
    EXPECT_EQ(values(*Cycler::concat(rg, b, &error), "color"), R"(["r","g","b"])");
    EXPECT_EQ(values(*Cycler::concat(b, rg, &error), "color"), R"(["b","r","g"])");
    EXPECT_EQ(values(*Cycler::concat(Cycler::of("color", {}), rg, &error), "color"), R"(["r","g"])");
    EXPECT_FALSE(Cycler::concat(rg, Cycler::of("linewidth", list("[1.0]")), &error).has_value());
    EXPECT_EQ(error, "Keys do not match");
    const Cycler wide = *Cycler::add(rg, Cycler::of("linewidth", list("[1.0, 2.0]")), &error);
    EXPECT_FALSE(Cycler::concat(wide, rg, &error).has_value());
    EXPECT_EQ(error, "Keys do not match");
    const auto chained = Cycler::concat(wide, *Cycler::add(Cycler::of("linewidth", list("[3.0]")), b, &error), &error);
    ASSERT_TRUE(chained.has_value()) << error;
    EXPECT_EQ(values(*chained, "color"), R"(["r","g","b"])");
    EXPECT_EQ(values(*chained, "linewidth"), "[1.0,2.0,3.0]");
}

// rcsetup.validate_cycler over 572 texts: the cycles of matplotlib's style sheets and every form.
TEST(Cycler, TextIsReadAsMatplotlibReadsIt) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/cycler.json", std::ios::binary);
    std::ostringstream content;
    content << in.rdbuf();
    const Json oracle = parse(content.str());
    const auto& cases = oracle.find("cases")->items();
    ASSERT_EQ(cases.size(), 572u);
    int taken = 0, refused = 0, unsupported = 0, unsupportedTaken = 0;
    std::vector<std::string> wrong, notRead;
    for (const Json& c : cases) {
        const std::string& given = c[0].asString();
        const Json& want = c[1];
        const Validated got = read(given);
        if (got.state == Validated::State::Unsupported) {
            ++unsupported;
            unsupportedTaken += !want.isNull();
            notRead.push_back(given + "   (" + got.error + ")");
            if (got.error.find("is not supported yet") == std::string::npos) wrong.push_back("no reason for " + given);
            continue;
        }
        if (got.state == Validated::State::Refused) {
            ++refused;
            if (!want.isNull()) wrong.push_back("matplotlib takes " + given + "; " + got.error);
            if (got.error.empty()) wrong.push_back("no reason for " + given);
            continue;
        }
        ++taken;
        if (want.isNull()) {
            wrong.push_back("matplotlib refuses " + given);
            continue;
        }
        const Json* cycle = got.value.find("$cycler");
        if (!cycle || !same(*cycle, want))
            wrong.push_back(given + " is " + text(got.value) + ", not " + text(want));
    }
    std::string first;
    for (std::size_t i = 0; i < wrong.size() && i < 40; ++i) first += wrong[i] + "\n";
    EXPECT_EQ(wrong.size(), 0u) << first;
    // What is not read here is counted, so that it cannot grow unseen.
    std::string all;
    for (const std::string& t : notRead) all += t + "\n";
    EXPECT_EQ(taken, 246) << all;
    EXPECT_EQ(refused, 246);
    // the six properties not held in a cycle yet (13), named colour sequences (18), and Python's other
    // spellings: string prefixes and escapes, 0x10, sets, bytes, arithmetic, characters outside ASCII
    EXPECT_EQ(unsupported, 80);
    EXPECT_EQ(unsupportedTaken, 44);
    EXPECT_EQ(taken + refused + unsupported, 572);
}

// Why a text is no cycle, in words.
TEST(Cycler, RefusalsSayWhy) {
    const auto why = [](const std::string& text) {
        const Validated got = read(text);
        EXPECT_NE(got.state, Validated::State::Taken) << text;
        const std::string notValid = "the value given for axes.prop_cycle is not valid: ";
        if (got.state != Validated::State::Refused) return got.error;
        EXPECT_EQ(got.error.compare(0, notValid.size(), notValid), 0) << got.error;
        return got.error.substr(std::min(notValid.size(), got.error.size()));
    };
    EXPECT_EQ(why("cycler(colour=['r'])"), "'cycler(colour=['r'])' is not a valid cycler construction: Unknown artist property: colour");
    EXPECT_EQ(why("cycler('color', 'rgb') + cycler('lw', [1, 2])"),
              "'cycler('color', 'rgb') + cycler('lw', [1, 2])' is not a valid cycler construction: Can only add equal length cycles, not 3 and 2");
    EXPECT_EQ(why("cycler(color='rg') * cycler(c='gb')"),
              "'cycler(color='rg') * cycler(c='gb')' is not a valid cycler construction: Can not compose overlapping cycles");
    EXPECT_EQ(why("cycler(color='rgb')[0]"), "'cycler(color='rgb')[0]' is not a valid cycler construction: only slicing is supported, not indexing");
    EXPECT_EQ(why("Cycler(color='rg')"),
              "'Cycler(color='rg')' is not a valid cycler construction: only the 'cycler()' and 'concat()' functions are allowed");
    EXPECT_EQ(why("cycler(color='rg') - cycler(lw=[1, 2])"),
              "'cycler(color='rg') - cycler(lw=[1, 2])' is not a valid cycler construction: this operator is not allowed");
    EXPECT_EQ(why("cycler()"), "'cycler()' is not a valid cycler construction: cycler() must have positional OR keyword arguments");
    EXPECT_EQ(why("cycler('color', ['r'], lw=[1])"),
              "'cycler('color', ['r'], lw=[1])' is not a valid cycler construction: cycler() can only accept positional OR keyword "
              "arguments -- not both.");
    EXPECT_EQ(why("cycler(['r'])"),
              "'cycler(['r'])' is not a valid cycler construction: If only one positional argument given, it must be a Cycler instance.");
    EXPECT_EQ(why("cycler(color=['C0'])"),
              "'cycler(color=['C0'])' is not a valid cycler construction: Cannot put cycle reference ('C0') in prop_cycler");
    EXPECT_EQ(why("['r', 'g']"), "Object is not a string or Cycler instance: ['r', 'g']");
    for (const std::string op : {"-", "/", "%", "@", "&", "|", "^", "<", ">"})
        EXPECT_EQ(why("cycler(color='rg') " + op + " cycler(lw=[1, 2])"),
                  "'cycler(color='rg') " + op + " cycler(lw=[1, 2])' is not a valid cycler construction: this operator is not allowed");
    EXPECT_EQ(why("cycler(color='rg') cycler(lw=[1, 2])"),
              "'cycler(color='rg') cycler(lw=[1, 2])' is not a valid cycler construction: this is not an expression that makes a cycler");
    EXPECT_EQ(why("cycler(color='rg') ?"), "'cycler(color='rg') ?' is not a valid cycler construction: invalid character");
    EXPECT_EQ(why(" cycler(color='rg')"), "' cycler(color='rg')' is not a valid cycler construction: unexpected indent");
    EXPECT_EQ(why("cycler(color='rg) "), "'cycler(color='rg) ' is not a valid cycler construction: unterminated string literal");
    EXPECT_EQ(why("cycler(color='rg') \\ "),
              "'cycler(color='rg') \\ ' is not a valid cycler construction: unexpected character after line continuation character");
    EXPECT_EQ(why("cycler(color='rg')\ncycler(lw=[1])"), "'cycler(color='rg')\ncycler(lw=[1])' is not a valid cycler construction: invalid syntax");
    EXPECT_EQ(why(""), "'' is not a valid cycler construction: there is nothing");
    for (const std::string text : {"cycler(color='rg'", "cycler(color='rg',", "cycler(", "cycler(lw=[1, 2", "(cycler(color='rg') + (", "cycler(color='rgb')[1:"})
        EXPECT_EQ(why(text), "'" + text + "' is not a valid cycler construction: a bracket is never closed");
    EXPECT_EQ(why("cycler(color=['r'], ['g'])"),
              "'cycler(color=['r'], ['g'])' is not a valid cycler construction: positional argument follows keyword argument");
    EXPECT_EQ(why("cycler('color', ['r'], ['g'])"), "'cycler('color', ['r'], ['g'])' is not a valid cycler construction: cycler() takes 0-2 arguments");
    EXPECT_EQ(why("concat(cycler(color='r'))"), "'concat(cycler(color='r'))' is not a valid cycler construction: concat() takes 2 arguments");
    EXPECT_EQ(why("concat(cycler(color='r'), left=cycler(color='g'))"),
              "'concat(cycler(color='r'), left=cycler(color='g'))' is not a valid cycler construction: concat() takes left and right");
    EXPECT_EQ(why("concat(cycler(color='r'), 1)"), "'concat(cycler(color='r'), 1)' is not a valid cycler construction: concat() takes cyclers");
    EXPECT_EQ(why("concat(cycler(color='r'), cycler(lw=[1]))"),
              "'concat(cycler(color='r'), cycler(lw=[1]))' is not a valid cycler construction: Keys do not match");
    EXPECT_EQ(why("cycler(color='rgb')[::0]"), "'cycler(color='rgb')[::0]' is not a valid cycler construction: slice step cannot be zero");
    EXPECT_EQ(why("cycler(color='rgb')[1.0:]"),
              "'cycler(color='rgb')[1.0:]' is not a valid cycler construction: slice indices must be integers or None");
    EXPECT_EQ(why("cycler(color='rg') * 2.0"),
              "'cycler(color='rg') * 2.0' is not a valid cycler construction: a cycler can only be multiplied by a cycler or an integer");
    EXPECT_EQ(why("cycler(color='rg') + 1"), "'cycler(color='rg') + 1' is not a valid cycler construction: a cycler can only be added to a cycler");
    EXPECT_EQ(why("cycler(color='rg') + [1]"),
              "'cycler(color='rg') + [1]' is not a valid cycler construction: Can only add equal length cycles, not 2 and 1");
    EXPECT_EQ(why("cycler(color='abc')"),
              "'cycler(color='abc')' is not a valid cycler construction: 'abc' is neither a color sequence name nor can it be interpreted as a "
              "list of colors");
    // a text may ask for a cycle of any length: beyond 10 000 entries it is not made
    for (const char* text : {"cycler(color='rgb') * 3334", "3334 * cycler(color='rgb')", "cycler(lw=[1, 2] * 5001)", "cycler(color='rg' * 5001)",
                             "cycler(lw=[1] * 101) * cycler(color='r' * 100)", "concat(cycler(color='r' * 6000), cycler(color='r' * 4001))"})
        EXPECT_EQ(why(text), "a cycle of more than 10000 entries is not supported yet") << text;
    EXPECT_EQ(read("cycler(color='rgb') * 3333").state, Validated::State::Taken);
    EXPECT_EQ(read("3333 * cycler(color='rgb')").state, Validated::State::Taken);
    EXPECT_EQ(read("cycler(lw=[1, 2] * 5000)").state, Validated::State::Taken);
    EXPECT_EQ(read("cycler(color='rg' * 5000)").state, Validated::State::Taken);
    EXPECT_EQ(read("cycler(lw=[1] * 100) * cycler(color='r' * 100)").state, Validated::State::Taken);
    EXPECT_EQ(read("concat(cycler(color='r' * 6000), cycler(color='r' * 4000))").state, Validated::State::Taken);
    EXPECT_EQ(read("cycler(lw=[1] * -5 + [2])").state, Validated::State::Taken);
    EXPECT_EQ(why("cycler(color='tab10')"), "a named colour sequence in a cycle is not supported yet");
    EXPECT_EQ(why("cycler(hatch=['/', 'x'])"), "the hatch of a cycle is not supported yet");
    EXPECT_EQ(why("cycler(color=['réd'])"), "a cycler with characters outside ASCII is not supported yet");
}

// What is not a text is a cycle only as a Cycler object, which a figure file cannot hold.
TEST(Cycler, OnlyTextMakesOne) {
    for (const char* value : {"1", "null", "true", "[\"r\", \"g\"]", "1.5"}) {
        const Validated got = calculate_plot::rc::validate("axes.prop_cycle", parse(value));
        EXPECT_EQ(got.state, Validated::State::Refused) << value;
        EXPECT_EQ(got.error, "the value given for axes.prop_cycle is not valid: the value is not a cycler") << value;
    }
}
