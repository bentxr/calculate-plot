#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <clocale>
#include <cstring>
#include <limits>
#include <string>

using calculate_plot::support::Json;
using calculate_plot::support::JsonError;
using calculate_plot::support::parseJson;

namespace {

Json parsed(std::string_view text) {
    JsonError error;
    auto value = parseJson(text, &error);
    EXPECT_TRUE(value.has_value()) << text << ": " << error.message << " at " << error.offset;
    return value ? *value : Json();
}

JsonError failure(std::string_view text) {
    JsonError error;
    EXPECT_FALSE(parseJson(text, &error).has_value()) << text;
    return error;
}

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

}  // namespace

TEST(Json, Literals) {
    EXPECT_TRUE(parsed("null").isNull());
    EXPECT_TRUE(parsed("true").isBool());
    EXPECT_TRUE(parsed("true").asBool());
    EXPECT_FALSE(parsed("false").asBool());
    EXPECT_FALSE(parsed("false").isNull());
}

TEST(Json, NumbersAreExactDoubles) {
    EXPECT_TRUE(sameBits(parsed("0.1").asNumber(), 0.1));
    EXPECT_TRUE(sameBits(parsed("-0.0").asNumber(), -0.0));
    EXPECT_TRUE(sameBits(parsed("5e-324").asNumber(), 5e-324));
    EXPECT_TRUE(sameBits(parsed("1.7976931348623157e308").asNumber(), 1.7976931348623157e308));
    EXPECT_TRUE(sameBits(parsed("2.2250738585072014E-308").asNumber(), 2.2250738585072014e-308));
    EXPECT_TRUE(sameBits(parsed("123456789012345678").asNumber(), 123456789012345678.0));
    EXPECT_TRUE(sameBits(parsed("0.30000000000000004").asNumber(), 0.1 + 0.2));
    EXPECT_EQ(parsed("-12").asNumber(), -12.0);
    EXPECT_EQ(parsed("1e+2").asNumber(), 100.0);
}

TEST(Json, NumbersKeepTheirText) {
    EXPECT_EQ(parsed("9007199254740993").numberText(), "9007199254740993");
    EXPECT_EQ(parsed("-0.50").numberText(), "-0.50");
}

TEST(Json, IntegersBeyondDoublePrecisionAreReadExactly) {
    EXPECT_EQ(parsed("9007199254740993").asInteger(), 9007199254740993LL);
    EXPECT_EQ(parsed("-9223372036854775808").asInteger(), std::numeric_limits<long long>::min());
}

TEST(Json, Strings) {
    EXPECT_EQ(parsed(R"("")").asString(), "");
    EXPECT_EQ(parsed(R"("abc")").asString(), "abc");
    EXPECT_EQ(parsed(R"("a\"b\\c\/d\b\f\n\r\t")").asString(), "a\"b\\c/d\b\f\n\r\t");
    EXPECT_EQ(parsed(R"("\u00e9\u20AC")").asString(), "\xc3\xa9\xe2\x82\xac");  // é €
    EXPECT_EQ(parsed(R"("\ud83d\ude00")").asString(), "\xf0\x9f\x98\x80");      // a surrogate pair
    EXPECT_EQ(parsed("\"\xce\xb1\"").asString(), "\xce\xb1");                     // raw UTF-8 passes
}

TEST(Json, Arrays) {
    const Json a = parsed(" [ 1 , [ ] , [true] , \"x\" ] ");
    ASSERT_TRUE(a.isArray());
    ASSERT_EQ(a.size(), 4u);
    EXPECT_EQ(a[0].asNumber(), 1.0);
    EXPECT_TRUE(a[1].isArray());
    EXPECT_EQ(a[1].size(), 0u);
    EXPECT_TRUE(a[2][0].asBool());
    EXPECT_EQ(a[3].asString(), "x");
}

TEST(Json, ObjectsKeepTheirOrder) {
    const Json o = parsed(R"({"b": 1, "a": {"c": null}, "z": []})");
    ASSERT_TRUE(o.isObject());
    ASSERT_EQ(o.members().size(), 3u);
    EXPECT_EQ(o.members()[0].first, "b");
    EXPECT_EQ(o.members()[1].first, "a");
    EXPECT_EQ(o.members()[2].first, "z");
    ASSERT_NE(o.find("a"), nullptr);
    EXPECT_TRUE(o.find("a")->find("c")->isNull());
    EXPECT_EQ(o.find("missing"), nullptr);
    EXPECT_EQ(o.find("b")->asNumber(), 1.0);
}

TEST(Json, FindOnANonObjectIsNull) {
    EXPECT_EQ(parsed("[1]").find("a"), nullptr);
}

TEST(Json, ErrorsSayWhere) {
    EXPECT_EQ(failure("").offset, 0u);
    EXPECT_EQ(failure("[1,]").offset, 3u);
    EXPECT_EQ(failure("{\"a\" 1}").offset, 5u);
    EXPECT_EQ(failure("[1] x").offset, 4u);
    EXPECT_EQ(failure("\"abc").offset, 4u);
}

TEST(Json, RejectsEveryMalformedClass) {
    for (const char* bad : {"nul", "tru", "falsy", "01", "-", "1.", ".5", "1e", "1e+", "+1", "0x10",
                            "\"\\x\"", "\"\\u12\"", "\"\\ud800\"", "\"\\udc00\"", "\"a\nb\"", "[", "]",
                            "{", "{\"a\":}", "{\"a\":1,}", "{1:2}", "[1 2]", "NaN", "Infinity",
                            "{\"a\":1,\"a\":2}", "\"\xff\"", "\"\xc3\""}) {
        JsonError error;
        EXPECT_FALSE(parseJson(bad, &error).has_value()) << bad;
        EXPECT_FALSE(error.message.empty()) << bad;
    }
}

TEST(Json, DeepNestingIsRefusedNotCrashed) {
    const std::string deep = std::string(100000, '[') + std::string(100000, ']');
    EXPECT_FALSE(parseJson(deep, nullptr).has_value());
    const std::string fine = std::string(500, '[') + std::string(500, ']');
    EXPECT_TRUE(parseJson(fine, nullptr).has_value());
}

TEST(Json, NumbersIgnoreTheProgramsLocale) {
    // A comma-decimal locale must not change how "0.5" reads (Qt sets the user's locale).
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous ? previous : "C";
    const bool switched = std::setlocale(LC_NUMERIC, "es_ES.UTF-8") || std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
    EXPECT_EQ(parsed("0.5").asNumber(), 0.5);
    EXPECT_EQ(parsed("[1.25e1]")[0].asNumber(), 12.5);
    std::setlocale(LC_NUMERIC, saved.c_str());
    if (!switched) GTEST_SKIP() << "no comma-decimal locale installed";
}

TEST(Json, OverflowingNumbersAreRefused) {
    EXPECT_FALSE(parseJson("1e400", nullptr).has_value());
    EXPECT_FALSE(parseJson("-1e400", nullptr).has_value());
    EXPECT_TRUE(sameBits(parsed("1e-400").asNumber(), 0.0));  // underflow rounds to zero, as Python does
}
