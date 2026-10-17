// Python literals as ast.literal_eval reads them (tests/oracle/pyliteral.json, written by
// tools/oracle/make_pyliteral_oracle.py with Python 3.14): 5103 strings, 1812 of them literals made
// of None, booleans, integers, floats, strings, tuples and lists.
#include "support/pyliteral.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using calculate_plot::support::PythonLiteral;
using calculate_plot::support::pythonLiteral;

namespace {

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/pyliteral.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

// The same value, an integer never being a float.
bool same(const Json& a, const Json& b) {
    if (a.type() != b.type()) return false;
    switch (a.type()) {
    case Json::Type::Null: return true;
    case Json::Type::Bool: return a.asBool() == b.asBool();
    case Json::Type::Number:
        return a.isInteger() == b.isInteger() && a.asNumber() == b.asNumber() &&
               std::signbit(a.asNumber()) == std::signbit(b.asNumber()) && (!a.isInteger() || a.numberText() == b.numberText());
    case Json::Type::String: return a.asString() == b.asString();
    case Json::Type::Array:
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!same(a[i], b[i])) return false;
        return true;
    case Json::Type::Object:
        if (a.members().size() != 1 || b.members().size() != 1 || a.members()[0].first != b.members()[0].first) return false;
        return same(a.members()[0].second, b.members()[0].second);
    }
    return false;
}

}  // namespace

TEST(PythonLiteral, ReadsWhatLiteralEvalReads) {
    const Json cases = oracle();
    ASSERT_EQ(cases.size(), 5103u);
    int values = 0, notOne = 0;
    std::map<std::string, int> unsupported;
    for (const Json& c : cases.items()) {
        const std::string& s = c.find("s")->asString();
        const Json& want = *c.find("v");
        const PythonLiteral got = pythonLiteral(s);
        if (got.state == PythonLiteral::State::Unsupported) {
            ++unsupported[want.size() == 1 ? "literals" : "others"];
            continue;
        }
        if (got.state == PythonLiteral::State::Value) {
            ++values;
            ASSERT_EQ(want.size(), 1u) << "not a literal: " << s;
            EXPECT_TRUE(same(got.value, want[0])) << s;
        } else {
            ++notOne;
            EXPECT_EQ(want.size(), 0u) << "a literal: " << s;
        }
    }
    // What is left unread is counted, so that it cannot grow unseen.
    EXPECT_EQ(values, 1758);
    EXPECT_EQ(notOne, 2844);
    EXPECT_EQ(unsupported["literals"], 54);
    EXPECT_EQ(unsupported["others"], 447);
}

TEST(PythonLiteral, SaysWhatItDoesNotRead) {
    for (const char* s : {"r'a'", "u'a'", "'a\\n'", "'''a'''", "\"\"\"a\"\"\"", "0x10", "0o7", "0b1", "(1,\n2)", "1\n",
                          "123456789012345678901234567890", "\xc3\xa9", "(1, \\\n2)", "\x01"})
        EXPECT_EQ(pythonLiteral(s).state, PythonLiteral::State::Unsupported) << s;
    // Deeper than Python's parser goes is not read either
    EXPECT_EQ(pythonLiteral(std::string(150, '(') + std::string(150, ')')).state, PythonLiteral::State::Unsupported);
    EXPECT_EQ(pythonLiteral(std::string(50, '(') + "1," + std::string(50, ')')).state, PythonLiteral::State::Value);
}
