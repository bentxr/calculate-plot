// The norms of matplotlib.colors against matplotlib 3.11.2 (tests/oracle/norms.json, written by
// tools/oracle/make_norms_oracle.py): outputs with their dtype and mask, the limits a call autoscales, the
// inverse, and the errors raised.
#include "colors/norms.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace support = calculate_plot::support;
namespace colors = calculate_plot::colors;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/norms.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double number(const Json& v) {
    if (v.isString()) return std::strtod(v.asString().c_str(), nullptr);
    if (v.isBool()) return v.asBool() ? 1 : 0;
    return v.asNumber();
}

std::string hex(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%a", v);
    return buf;
}

bool same(double got, double want) {
    return (std::isnan(got) && std::isnan(want)) || calculate_plot::testing_support::sameNumber(got, want);
}

colors::MaskedArray array(const Json& v) {
    colors::MaskedArray a;
    a.dtype = *colors::dtypeNamed(v.find("dtype")->asString());
    for (const Json& d : v.find("data")->items()) a.data.push_back(number(d));
    a.mask.assign(a.data.size(), false);
    for (const Json& i : v.find("mask")->items()) a.mask[std::size_t(i.asNumber())] = true;
    return a;
}

colors::NormSpec spec(const Json& c) {
    colors::NormSpec s;
    s.kind = c.find("norm")->asString();
    if (s.kind == "TwoSlopeNorm" || s.kind == "CenteredNorm") s.vcenter = s.kind == "CenteredNorm" ? std::optional<double>(0) : std::nullopt;
    for (const auto& [name, value] : c.find("kwargs")->members()) {
        if (name == "vmin") s.vmin = number(value);
        else if (name == "vmax") s.vmax = number(value);
        else if (name == "clip") s.clip = value.asBool();
        else if (name == "gamma") s.gamma = number(value);
        else if (name == "vcenter") s.vcenter = number(value);
        else if (name == "halfrange") s.halfrange = number(value);
        else if (name == "linthresh") s.linthresh = number(value);
        else if (name == "linscale") s.linscale = number(value);
        else if (name == "base") s.base = number(value);
        else if (name == "linear_width") s.linearWidth = number(value);
        else if (name == "ncolors") s.ncolors = (long long)value.asNumber();
        else if (name == "extend") s.extend = value.asString();
        else if (name == "boundaries")
            for (const Json& b : value.items()) s.boundaries.push_back(number(b));
        else ADD_FAILURE() << "unknown keyword " << name;
    }
    return s;
}

// The same masked array: dtype, mask, and the values that are not masked.
std::string difference(const colors::MaskedArray& got, const Json& want) {
    const colors::MaskedArray w = array(want);
    if (colors::dtypeName(got.dtype) != want.find("dtype")->asString())
        return std::string("dtype ") + colors::dtypeName(got.dtype) + ", want " + want.find("dtype")->asString();
    if (got.data.size() != w.data.size()) return "size";
    for (std::size_t i = 0; i < w.data.size(); ++i) {
        if (got.masked(i) != w.masked(i)) return "mask at " + std::to_string(i);
        if (!w.masked(i) && !same(got.data[i], w.data[i]))
            return "value " + std::to_string(i) + ": " + hex(got.data[i]) + ", want " + hex(w.data[i]);
    }
    return "";
}

std::string pyError(const Json& e) {
    const std::string text = e.asString();
    const auto colon = text.find(": ");
    return colon == std::string::npos ? text : text.substr(colon + 2);
}

}  // namespace

TEST(Norms, CallsInversesAndErrorsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("cases")->items()) {
        const std::string where = "case " + std::to_string(checked) + " (" + c.find("norm")->asString() + ", " +
                                  c.find("input")->find("dtype")->asString() + ")";
        ++checked;
        colors::Norm norm;
        std::string error;
        if (!colors::Norm::make(spec(c), &norm, &error)) {
            ASSERT_TRUE(c.find("error") && !c.find("vmin")) << where << ": " << error;
            EXPECT_EQ(error, pyError(*c.find("error"))) << where;
            continue;
        }
        ASSERT_TRUE(c.find("vmin")) << where << ": the constructor raises " << c.find("error")->asString();
        colors::MaskedArray out;
        const Json* clip = c.find("clip");
        const bool ok = norm(array(*c.find("input")), &out, &error,
                             clip ? std::optional<bool>(clip->asBool()) : std::nullopt);
        if (const Json* e = c.find("error")) {
            EXPECT_FALSE(ok) << where << ": raises " << e->asString();
            EXPECT_EQ(error, pyError(*e)) << where;
        } else {
            ASSERT_TRUE(ok) << where << ": " << error;
            EXPECT_EQ(difference(out, *c.find("output")), "") << where;
        }
        const Json& vmin = *c.find("vmin");
        const Json& vmax = *c.find("vmax");
        EXPECT_EQ(norm.vmin().has_value(), !vmin.isNull()) << where;
        EXPECT_EQ(norm.vmax().has_value(), !vmax.isNull()) << where;
        if (norm.vmin() && !vmin.isNull()) {
            EXPECT_TRUE(same(*norm.vmin(), number(vmin))) << where << " vmin";
        }
        if (norm.vmax() && !vmax.isNull()) {
            EXPECT_TRUE(same(*norm.vmax(), number(vmax))) << where << " vmax";
        }
        if (!ok) continue;
        colors::MaskedArray probe;
        probe.data = {0.0, 0.25, 0.5, 1.0, -0.5, 1.5};
        probe.mask.assign(6, false);
        colors::MaskedArray back;
        const bool inverted = norm.inverse(probe, &back, &error);
        if (const Json* e = c.find("inverse_error")) {
            EXPECT_FALSE(inverted) << where << ": inverse raises " << e->asString();
            EXPECT_EQ(error, pyError(*e)) << where;
        } else if (const Json* want = c.find("inverse")) {
            ASSERT_TRUE(inverted) << where << ": " << error;
            EXPECT_EQ(difference(back, *want), "") << where << " inverse";
        }
    }
    EXPECT_EQ(checked, 2403);
}

TEST(Norms, DtypeNamesRoundTrip) {
    for (const char* name : {"bool", "uint8", "int16", "uint16", "int32", "int64", "float32", "float64"})
        EXPECT_STREQ(colors::dtypeName(*colors::dtypeNamed(name)), name);
    EXPECT_FALSE(colors::dtypeNamed("complex128"));
}
