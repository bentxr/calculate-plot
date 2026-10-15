// The style sheets matplotlib 3.11.2 bundles, used as plt.style.use uses them, against what matplotlib
// leaves in its rcParams (tests/oracle/styles.json, written by tools/oracle/make_styles.py).
#include "rc/styles.hpp"

#include "rc/rc_params.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
namespace rc = calculate_plot::rc;
using calculate_plot::support::Json;

namespace {

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/styles.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

Json parsed(const std::string& text) { return *calculate_plot::support::parseJson(text, nullptr); }

// A value against matplotlib's: an integer is never a float, and a float that is not finite is
// {"$float": ...} in the oracle.
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
               std::signbit(got.asNumber()) == std::signbit(want.asNumber());
    case Json::Type::String: return got.asString() == want.asString();
    case Json::Type::Array:
        if (got.size() != want.size()) return false;
        for (std::size_t i = 0; i < got.size(); ++i)
            if (!same(got[i], want[i])) return false;
        return true;
    case Json::Type::Object: {
        const auto& g = got.members();
        const auto& w = want.members();
        if (g.size() != w.size()) return false;
        for (std::size_t i = 0; i < g.size(); ++i)
            if (g[i].first != w[i].first || !same(g[i].second, w[i].second)) return false;
        return true;
    }
    }
    return false;
}

std::string specText(const Json& spec) {
    if (spec.isString()) return "'" + spec.asString() + "'";
    std::string out = "[";
    for (std::size_t i = 0; i < spec.size(); ++i) out += (i ? ", " : "") + specText(spec[i]);
    return out + "]";
}

}  // namespace

TEST(Styles, TheLibraryIsMatplotlibs) {
    const Json o = oracle();
    std::vector<std::string> want;
    for (const Json& name : o.find("styles")->items()) want.push_back(name.asString());
    EXPECT_EQ(rc::styleNames(), want);
}

TEST(Styles, UsingThemLeavesTheRcParamsMatplotlibLeaves) {
    const Json o = oracle();
    // What matplotlib takes and the engine does not do yet: refused as such, never half done.
    const std::set<std::string> notSupportedYet = {};
    std::set<std::string> refusedAsNotSupported;
    int taken = 0, refused = 0;
    for (const Json& c : o.find("cases")->items()) {
        const Json& spec = *c.find("spec");
        const std::string name = specText(spec);
        rc::Params params = rc::Params::defaults();
        std::string error;
        const bool ok = rc::useStyle(params, spec, &error);
        if (const Json* want = c.find("error")) {
            EXPECT_FALSE(ok) << name << " is refused by matplotlib: " << want->asString();
            ++refused;
            continue;
        }
        if (!ok) {
            EXPECT_NE(error.find("not supported yet"), std::string::npos) << name << ": " << error;
            refusedAsNotSupported.insert(name);
            continue;
        }
        ++taken;
        const rc::Params defaults = rc::Params::defaults();
        const Json& changed = *c.find("rcParams");
        for (const std::string& key : params.keys()) {
            if (key == "backend") continue;
            const Json* want = changed.find(key);
            const Json& expected = want ? *want : defaults.value(key);
            EXPECT_TRUE(same(params.value(key), expected)) << name << " " << key;
        }
    }
    EXPECT_EQ(refusedAsNotSupported, notSupportedYet);
    EXPECT_EQ(refused, 11);
    EXPECT_GT(taken, 30);
}

TEST(Styles, AnUnknownNameIsRefusedAsMatplotlibRefusesIt) {
    rc::Params params = rc::Params::defaults();
    std::string error;
    EXPECT_FALSE(rc::useStyle(params, Json::string("seaborn"), &error));
    EXPECT_EQ(error, "'seaborn' is not a valid package style, path of style file, URL of style file, or library style "
                     "name (library styles are listed in `style.available`)");
    // A figure file names no files and no packages.
    EXPECT_FALSE(rc::useStyle(params, Json::string("matplotlib.ggplot"), &error));
    EXPECT_FALSE(rc::useStyle(params, Json::string("ggplot.mplstyle"), &error));
}

TEST(Styles, ASpecIsANameOrAListOfNames) {
    rc::Params params = rc::Params::defaults();
    std::string error;
    EXPECT_TRUE(rc::useStyle(params, parsed(R"(["ggplot", "dark_background"])"), &error)) << error;
    EXPECT_TRUE(rc::useStyle(params, parsed("[]"), &error)) << error;
    for (const char* bad : {"3", "null", "true", R"([["ggplot"]])", R"([1])", R"({"lines.linewidth": 2})"}) {
        EXPECT_FALSE(rc::useStyle(params, parsed(bad), &error)) << bad;
        EXPECT_NE(error.find("style"), std::string::npos) << bad << ": " << error;
    }
}

TEST(Styles, TheDefaultStyleLeavesWhatIsNotStyleAlone) {
    // _STYLE_BLACKLIST: "default" resets everything else.
    rc::Params params = rc::Params::defaults();
    std::string error;
    ASSERT_TRUE(params.set("figure.max_open_warning", Json::integer(5), &error)) << error;
    ASSERT_TRUE(params.set("lines.linewidth", Json::real(3.0), &error)) << error;
    ASSERT_TRUE(rc::useStyle(params, Json::string("default"), &error)) << error;
    EXPECT_EQ(params.number("figure.max_open_warning"), 5);
    EXPECT_EQ(params.number("lines.linewidth"), 1.5);
}
