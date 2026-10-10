#include "support/json.hpp"
#include "support/json_writer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

using calculate_plot::support::JsonWriter;

TEST(JsonWriter, WritesWhatTheReaderReadsBack) {
    JsonWriter w;
    w.beginObject();
    w.key("op").value("draw_path");
    w.key("n").value(0.1 + 0.2);
    w.key("big").value(1e300);
    w.key("i").value(3);
    w.key("flag").value(true);
    w.key("none").null();
    w.key("text").value("quote \" back \\ new\nline \xc3\xa9");
    w.key("nan").value(std::numeric_limits<double>::quiet_NaN());
    w.key("list").beginArray().value(1.5).value(-0.0).endArray();
    w.endObject();
    const auto back = calculate_plot::support::parseJson(w.text(), nullptr);
    ASSERT_TRUE(back.has_value()) << w.text();
    EXPECT_EQ(back->find("op")->asString(), "draw_path");
    EXPECT_EQ(back->find("n")->asNumber(), 0.1 + 0.2);
    EXPECT_EQ(back->find("big")->asNumber(), 1e300);
    EXPECT_EQ(back->find("i")->numberText(), "3");
    EXPECT_TRUE(back->find("flag")->asBool());
    EXPECT_TRUE(back->find("none")->isNull());
    EXPECT_EQ(back->find("text")->asString(), "quote \" back \\ new\nline \xc3\xa9");
    EXPECT_EQ(back->find("nan")->asString(), "nan");  // the trace format's spelling of non-finite numbers
    const double minusZero = (*back->find("list"))[1].asNumber();
    EXPECT_TRUE(minusZero == 0.0 && std::signbit(minusZero));
}

TEST(JsonWriter, SpellsNumbersAsPythonsRepr) {
    JsonWriter w;
    w.beginArray().value(1.0).value(1e-05).value(0.30000000000000004).endArray();
    EXPECT_EQ(w.text(), "[1.0,1e-05,0.30000000000000004]");
}
