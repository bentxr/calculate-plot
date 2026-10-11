// The validators of the rcParams against matplotlib 3.11.2's (tests/oracle/rc_validate.json, written
// by tools/oracle/make_rc_oracle.py): each of the 339 keys with each of 631 values, which matplotlib
// takes (and stores in some form) or refuses.
#include "rc/rc_validate.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::rc::Validated;
using calculate_plot::support::Json;

namespace {

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/rc_validate.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

// A value for a message.
std::string show(const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: return "null";
    case Json::Type::Bool: return v.asBool() ? "true" : "false";
    case Json::Type::Number: return v.numberText();
    case Json::Type::String: return "\"" + v.asString() + "\"";
    case Json::Type::Array: {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + show(v[i]);
        return out + "]";
    }
    case Json::Type::Object: return "{" + v.members()[0].first + ": " + show(v.members()[0].second) + "}";
    }
    return "?";
}

// What is stored against what matplotlib stores: an integer is never a float, and a float that is
// not finite is {"$float": ...} in the oracle.
bool same(const Json& got, const Json& want) {
    if (want.isObject() && want.members()[0].first == "$float") {
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
               std::signbit(got.asNumber()) == std::signbit(want.asNumber()) &&
               (!got.isInteger() || got.numberText() == want.numberText());
    case Json::Type::String: return got.asString() == want.asString();
    case Json::Type::Array:
        if (got.size() != want.size()) return false;
        for (std::size_t i = 0; i < got.size(); ++i)
            if (!same(got[i], want[i])) return false;
        return true;
    case Json::Type::Object: {
        // a cycler: its properties, whatever their order (Cycler.by_key() has none)
        if (got.members().size() != want.members().size()) return false;
        for (const auto& [key, value] : got.members()) {
            const Json* other = want.find(key);
            if (!other || !same(value, *other)) return false;
        }
        return true;
    }
    }
    return false;
}

}  // namespace

TEST(RcValidate, EveryKeyTakesAndStoresWhatMatplotlibDoes) {
    const Json o = oracle();
    const auto& pool = o.find("pool")->items();
    const auto& rows = o.find("rows")->items();
    ASSERT_EQ(pool.size(), 631u);
    ASSERT_EQ(o.find("keys")->members().size(), 339u);
    long taken = 0, refused = 0, unsupported = 0, unsupportedTaken = 0;
    std::map<std::string, int> unsupportedKeys;
    std::vector<std::string> wrong;
    for (const auto& [key, row] : o.find("keys")->members()) {
        const auto& outcomes = rows[std::size_t(row.asInteger())].items();
        ASSERT_EQ(outcomes.size(), pool.size());
        for (std::size_t i = 0; i < pool.size(); ++i) {
            const Validated got = calculate_plot::rc::validate(key, pool[i]);
            const bool want = outcomes[i].size() == 1;
            if (got.state == Validated::State::Unsupported) {
                ++unsupported;
                unsupportedTaken += want;
                ++unsupportedKeys[key];
                if (got.error.find("is not supported yet") == std::string::npos) wrong.push_back(key + ": no reason for " + show(pool[i]));
                continue;
            }
            if (got.state != Validated::State::Taken && got.error.empty()) wrong.push_back(key + ": no reason for " + show(pool[i]));
            if (want != (got.state == Validated::State::Taken))
                wrong.push_back(key + " = " + show(pool[i]) + (want ? ": matplotlib takes it; " + got.error : ": matplotlib refuses it"));
            else if (want && !same(got.value, outcomes[i][0]))
                wrong.push_back(key + " = " + show(pool[i]) + " is stored as " + show(got.value) + ", not " + show(outcomes[i][0]));
            ++(want ? taken : refused);
        }
    }
    std::string first;
    for (std::size_t i = 0; i < wrong.size() && i < 60; ++i) first += wrong[i] + "\n";
    EXPECT_EQ(wrong.size(), 0u) << first;
    // What is not known here is counted, key by key, so that it cannot grow unseen.
    std::string keys;
    for (const auto& [key, n] : unsupportedKeys) keys += key + " " + std::to_string(n) + "\n";
    EXPECT_EQ(taken, 45414) << keys;
    EXPECT_EQ(refused, 165753);
    EXPECT_EQ(unsupported, 2742);  // 631 each for the backend and date.epoch; text outside ASCII; 10 cyclers
    EXPECT_EQ(unsupportedTaken, 603);
    EXPECT_EQ(taken + refused + unsupported, 631 * 339);
}

TEST(RcValidate, AnUnknownKeyIsRefused) {
    const auto got = calculate_plot::rc::validate("no.such.key", Json::integer(1));
    EXPECT_EQ(got.state, Validated::State::Refused);
    EXPECT_EQ(got.error, "no.such.key is not a valid rc parameter");
}
