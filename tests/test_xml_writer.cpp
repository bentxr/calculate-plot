#include "svg/svg_renderer.h"
#include "svg/xml_writer.h"

#include <gtest/gtest.h>

using calculate_plot::svg::XmlWriter;

TEST(XmlWriter, WritesElementsLikeMatplotlib) {
    std::string out;
    XmlWriter w(out);
    const int id = w.start("svg", {{"a", "1"}, {"empty", ""}, {"q", "say \"hi\""}});
    w.element("path", "", {{"d", "M 0 0"}});
    w.element("title", "a < b & c");
    w.comment("x--y");
    w.close(id);
    EXPECT_EQ(out, "<svg a=\"1\" q='say \"hi\"'>\n"
                   " <path d=\"M 0 0\"/>\n"
                   " <title>a &lt; b &amp; c</title>\n"
                   " <!-- x- -y -->\n"
                   "</svg>\n");
}

TEST(XmlWriter, IndentationStopsAt64Spaces) {
    // matplotlib indents with a 64-space string, so deeper elements stay at 64 spaces.
    std::string out;
    XmlWriter w(out);
    for (int i = 0; i < 66; ++i) w.start("g");
    w.element("p");
    w.close(0);
    std::string expected;
    for (int i = 0; i < 66; ++i) expected += std::string(std::min(i, 64), ' ') + "<g>\n";
    expected += std::string(64, ' ') + "<p/>\n";
    for (int i = 65; i >= 0; --i) expected += std::string(std::min(i, 64), ' ') + "</g>\n";
    EXPECT_EQ(out, expected);
}

TEST(PythonRepr, BytesAsPythonSpellsThem) {
    using calculate_plot::svg::pythonBytesRepr;
    // repr() of the same bytes in Python 3.14.
    EXPECT_EQ(pythonBytesRepr(std::string("\x00\x7f\xff'\"\\\n\t\r A", 11)), "b'\\x00\\x7f\\xff\\'\"\\\\\\n\\t\\r A'");
    EXPECT_EQ(pythonBytesRepr("it's"), "b\"it's\"");
    EXPECT_EQ(pythonBytesRepr("say \"hi\""), "b'say \"hi\"'");
}
