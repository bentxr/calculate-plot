// The schema of the calls a figure file can make (calculate_plot::schema): matplotlib's parameters and
// properties, each with whether calculate-plot takes it.
#include "calculate-plot/calculate-plot.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <map>
#include <set>

using calculate_plot::support::Json;

namespace {

const std::string kFonts = std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf";

const Json& schemaJson() {
    static const Json doc = *calculate_plot::support::parseJson(calculate_plot::schema(kFonts), nullptr);
    return doc;
}

// on.call.keyword -> supported (true, false or null)
std::map<std::string, Json> flags() {
    std::map<std::string, Json> out;
    for (const Json& call : schemaJson().find("calls")->items()) {
        for (const char* part : {"parameters", "properties"}) {
            const Json* list = call.find(part);
            if (!list) continue;
            for (const Json& e : list->items())
                out[call.find("on")->asString() + "." + call.find("call")->asString() + "." + e.find("name")->asString()] =
                    *e.find("supported");
        }
    }
    return out;
}

}  // namespace

TEST(Schema, EveryCallIsOneTheEngineMakes) {
    const auto& calls = schemaJson().find("calls")->items();
    EXPECT_EQ(calls.size(), 50u);
    for (const Json& call : calls)
        for (const Json& p : call.find("parameters")->items()) {
            if (p.find("required")) {
                EXPECT_TRUE(p.find("supported")->isBool() && p.find("supported")->asBool()) << call.find("call")->asString();
            }
        }
}

TEST(Schema, PropertiesSayWhetherTheyAreTaken) {
    const auto f = flags();
    const auto is = [&](const std::string& key, bool value) {
        const auto it = f.find(key);
        ASSERT_NE(it, f.end()) << key;
        EXPECT_TRUE(it->second.isBool() && it->second.asBool() == value) << key;
    };
    is("axes.plot.color", true);
    is("axes.plot.drawstyle", true);
    is("axes.hist.bins", true);
    is("axes.hist.weights", true);
    is("axes.hist.joinstyle", false);
    is("axes.plot.marker", true);
    is("axes.fill_between.hatch", true);
    is("axes.axhspan.hatch", true);
    is("axes.legend.shadow", true);
    is("axes.legend.ncols", true);
    is("axes.plot.gapcolor", false);
    is("axes.scatter.hatch", false);
    is("axes.fill.closed", true);
    is("axes.fill.hatch", true);
    is("axes.fill.capstyle", false);
    is("axes.annotate.textcoords", true);
    is("axes.annotate.annotation_clip", true);
    is("axes.bar_label.fmt", true);
    is("axes.bar_label.padding", true);
    is("axes.step.where", true);
    is("axes.stairs.baseline", true);
}

TEST(Schema, WhatCannotBeToldIsCounted) {
    // the counts move as calculate-plot grows: then look at the list and set them again
    int taken = 0, refused = 0, unknown = 0;
    std::string unknowns;
    for (const auto& [key, value] : flags()) {
        if (value.isNull()) ++unknown, unknowns += key + "\n";
        else if (value.asBool()) ++taken;
        else ++refused;
    }
    std::printf("schema: %d taken, %d not supported yet, %d not told\n", taken, refused, unknown);
    EXPECT_EQ(taken + refused + unknown, 853);
    EXPECT_EQ(taken, 626);
    EXPECT_EQ(refused, 151);
    EXPECT_EQ(unknown, 76) << unknowns;  // objects and dicts a figure file does not hold (data, fontdict, prop...)
}
