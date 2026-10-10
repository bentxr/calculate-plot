#include "support/base64.hpp"

#include <gtest/gtest.h>

using calculate_plot::support::decodeBase64;

TEST(Base64, Rfc4648Vectors) {
    EXPECT_EQ(decodeBase64(""), std::string(""));
    EXPECT_EQ(decodeBase64("Zg=="), std::string("f"));
    EXPECT_EQ(decodeBase64("Zm8="), std::string("fo"));
    EXPECT_EQ(decodeBase64("Zm9v"), std::string("foo"));
    EXPECT_EQ(decodeBase64("Zm9vYg=="), std::string("foob"));
    EXPECT_EQ(decodeBase64("Zm9vYmE="), std::string("fooba"));
    EXPECT_EQ(decodeBase64("Zm9vYmFy"), std::string("foobar"));
}

TEST(Base64, EveryByteValue) {
    // base64 of bytes 0xff 0xfe 0x00 0x80 / and +
    EXPECT_EQ(decodeBase64("//4AgA=="), std::string("\xff\xfe\x00\x80", 4));
    EXPECT_EQ(decodeBase64("+/+/"), std::string("\xfb\xff\xbf", 3));
}

TEST(Base64, RefusesMalformedText) {
    EXPECT_FALSE(decodeBase64("Zm9").has_value());     // not a multiple of 4
    EXPECT_FALSE(decodeBase64("Zm9v!A==").has_value()); // a character outside the alphabet
    EXPECT_FALSE(decodeBase64("Z===").has_value());     // too much padding
    EXPECT_FALSE(decodeBase64("Zg==Zg==").has_value()); // padding before the end
}
