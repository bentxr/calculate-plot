// plt.subplots(sharex=, sharey=) sets up what matplotlib 3.11.2 does (tests/oracle/shared_axes.json,
// every grid up to 3 x 3 and every way of sharing): who shares an axis with whom and in which order,
// whose axis the shared locators belong to, which tick labels and offset texts stay, and how scales,
// limits and autoscaling reach the Axes that share them.
#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::engine;

namespace {

std::vector<int> ints(const Json& j) {
    std::vector<int> out;
    for (const Json& v : j.items()) out.push_back(int(v.asNumber()));
    return out;
}

std::vector<bool> bools(const Json& j) {
    std::vector<bool> out;
    for (const Json& v : j.items()) out.push_back(v.asBool());
    return out;
}

int indexOf(const std::vector<Axes*>& axes, const Axes* ax) {
    for (std::size_t i = 0; i < axes.size(); ++i)
        if (axes[i] == ax) return int(i);
    return -1;
}

void expectState(const std::vector<Axes*>& axes, const Json& want) {
    for (const bool x : {true, false}) {
        SCOPED_TRACE(x ? "x" : "y");
        const Json& w = *want.find(x ? "x" : "y");
        std::vector<int> owner;
        std::vector<bool> label1, label2, minor1, minor2, offsetText, autoscale;
        std::vector<std::string> scale;
        for (std::size_t i = 0; i < axes.size(); ++i) {
            const Axis& axis = x ? *axes[i]->xaxis : *axes[i]->yaxis;
            std::vector<int> siblings;
            for (const Axes* s : axes[i]->shared(x).members) siblings.push_back(indexOf(axes, s));
            EXPECT_EQ(siblings, ints((*w.find("siblings"))[i])) << "the Axes sharing with Axes " << i;
            int own = -1;
            for (std::size_t k = 0; k < axes.size(); ++k)
                if (&axis.tickerAxis() == (x ? axes[k]->xaxis.get() : axes[k]->yaxis.get())) own = int(k);
            owner.push_back(own);
            label1.push_back(axis.tickSwitches(true).label1On);
            label2.push_back(axis.tickSwitches(true).label2On);
            minor1.push_back(axis.tickSwitches(false).label1On);
            minor2.push_back(axis.tickSwitches(false).label2On);
            offsetText.push_back(axis.offsetText.visible);
            autoscale.push_back(axis.autoscaleOn);
            scale.push_back(axis.scale());
        }
        EXPECT_EQ(owner, ints(*w.find("owner")));
        EXPECT_EQ(label1, bools(*w.find("label1")));
        EXPECT_EQ(label2, bools(*w.find("label2")));
        EXPECT_EQ(minor1, bools(*w.find("minor_label1")));
        EXPECT_EQ(minor2, bools(*w.find("minor_label2")));
        EXPECT_EQ(offsetText, bools(*w.find("offset_text")));
        EXPECT_EQ(autoscale, bools(*w.find("autoscale")));
        std::vector<std::string> wantScale;
        for (const Json& v : w.find("scale")->items()) wantScale.push_back(v.asString());
        EXPECT_EQ(scale, wantScale);
    }
}

TEST(SharedAxes, SubplotsShareAsMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/shared_axes.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto cases = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(cases.has_value());
    ASSERT_EQ(cases->size(), 288u);
    for (const Json& c : cases->items()) {
        const int nrows = int(c.find("nrows")->asNumber()), ncols = int(c.find("ncols")->asNumber());
        const std::string sharex = c.find("sharex")->asString(), sharey = c.find("sharey")->asString();
        SCOPED_TRACE(std::to_string(nrows) + " x " + std::to_string(ncols) + ", sharex=" + sharex + ", sharey=" + sharey +
                     (c.find("rc")->members().empty() ? "" : ", labels on the top and the right"));
        auto rc = calculate_plot::rc::Params::defaults();
        for (const auto& [key, value] : c.find("rc")->members()) {
            std::string error;
            ASSERT_TRUE(rc.set(key, value, &error)) << error;
        }
        Figure figure(rc, {});
        const std::vector<Axes*> axes = figure.subplots(nrows, ncols, sharex, sharey);
        {
            SCOPED_TRACE("as made");
            expectState(axes, *c.find("made"));
        }
        const Json& log = *c.find("log");
        axes[std::size_t(log.find("x_on")->asNumber())]->setScale(true, "log");
        axes[std::size_t(log.find("y_on")->asNumber())]->setScale(false, "log");
        {
            SCOPED_TRACE("after the log scales");
            expectState(axes, *log.find("state"));
        }
        const Json& limits = *c.find("limits");
        axes[std::size_t(limits.find("x_on")->asNumber())]->xaxis->setLim(2, 5, false);
        axes[std::size_t(limits.find("y_on")->asNumber())]->yaxis->setLim(3, 4, false);
        {
            SCOPED_TRACE("after the limits");
            expectState(axes, *limits.find("state"));
            for (std::size_t i = 0; i < axes.size(); ++i) {
                const auto xl = axes[i]->xaxis->viewInterval(), yl = axes[i]->yaxis->viewInterval();
                const Json& wx = (*limits.find("xlim"))[i];
                const Json& wy = (*limits.find("ylim"))[i];
                EXPECT_EQ(xl[0], wx[0].asNumber()) << "xlim of Axes " << i;
                EXPECT_EQ(xl[1], wx[1].asNumber()) << "xlim of Axes " << i;
                EXPECT_EQ(yl[0], wy[0].asNumber()) << "ylim of Axes " << i;
                EXPECT_EQ(yl[1], wy[1].asNumber()) << "ylim of Axes " << i;
            }
        }
    }
}

}  // namespace
