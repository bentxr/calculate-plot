// The trace renderers write what matplotlib's renderers are asked to draw: single calls compared
// with the events of a recorded figure (tests/figures/empty_axes, matplotlib 3.11.2).
#include "engine/trace_renderer.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using calculate_plot::testing_support::firstDifference;
using namespace calculate_plot::engine;

namespace {

Json recorded(const std::string& name) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/figures" / name / "trace.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

Json events(AggTraceRenderer& renderer) {
    return *calculate_plot::support::parseJson(renderer.trace(), nullptr)->find("events");
}

std::string fonts() { return std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf"; }

// The tick labels' graphics context: GraphicsContextBase's defaults.
GC textGc(const calculate_plot::rc::Params& rc) {
    GC gc(rc);
    return gc;
}

TEST(AggTraceRenderer, DrawsMarkersAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    AggTraceRenderer r(320, 240, 100, rc, "C.UTF-8", fonts());
    GC gc(rc);
    gc.linewidth = 0.8;
    gc.snap = true;
    // XTick's tick1line: the TICKDOWN marker at (0, 0) of the axes.
    const Path marker = Path::make({-0.0, 0.0, -0.0, 1.0}, {}, rc);
    const Path at = Path::make({0.0, 0.0}, {}, rc);
    r.drawMarkers(gc, marker, {4.861111111111111, 0.0, 0.0, -0.0, -4.861111111111111, -0.0}, at,
                  {248.0, 0.0, 40.0, 0.0, 184.79999999999998, 26.4}, std::vector<double>{0, 0, 0, 1});
    const Json want = recorded("empty_axes").find("events")->operator[](3);
    EXPECT_EQ(firstDifference(events(r)[0], want, "event"), "");
}

TEST(AggTraceRenderer, DrawsTextAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    AggTraceRenderer r(320, 240, 100, rc, "C.UTF-8", fonts());
    r.drawText(textGc(rc), 29.0, 233.87452256944445, "0.0", FontSpec{{"DejaVuSans.ttf"}, 10.0, {}}, 0.0, false, {});
    const Json got = events(r);
    const Json trace = recorded("empty_axes");
    const Json& want = *trace.find("events");
    ASSERT_EQ(got.size(), 5u);  // the call, its glyphs, and one bitmap per glyph
    for (std::size_t i = 0; i < 5; ++i)
        EXPECT_EQ(firstDifference(got[i], want[4 + i], "event"), "") << i;
    EXPECT_EQ(r.error(), "");
}

// RendererAgg.get_text_width_height_descent, against matplotlib's values (tests/oracle/text_extent.json).
TEST(AggTraceRenderer, MeasuresTextAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/text_extent.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json oracle = *calculate_plot::support::parseJson(text.str(), nullptr);
    int checked = 0;
    for (const Json& c : oracle.items()) {
        std::vector<std::string> names;
        for (const Json& f : c.find("fonts")->items()) names.push_back(f.asString());
        AggTraceRenderer r(320, 240, c.find("dpi")->asNumber(), rc, "C.UTF-8", fonts());
        const auto e = r.textWidthHeightDescent(c.find("s")->asString(), FontSpec{names, c.find("size")->asNumber(), {}}, false);
        EXPECT_EQ(e.width, std::strtod(c.find("width")->asString().c_str(), nullptr)) << c.find("s")->asString();
        EXPECT_EQ(e.height, std::strtod(c.find("height")->asString().c_str(), nullptr)) << c.find("s")->asString();
        EXPECT_EQ(e.descent, std::strtod(c.find("descent")->asString().c_str(), nullptr)) << c.find("s")->asString();
        ++checked;
    }
    EXPECT_EQ(checked, 192);
}

TEST(AggTraceRenderer, ReportsFontsItCannotOpen) {
    const auto rc = calculate_plot::rc::Params::defaults();
    AggTraceRenderer r(320, 240, 100, rc, "C.UTF-8", fonts());
    r.drawText(GC(rc), 1, 1, "x", FontSpec{{"NoSuchFont.ttf"}, 10.0, {}}, 0.0, false, {});
    EXPECT_NE(r.error(), "");
}

}  // namespace

namespace {

// draw_path_collection: the second (non-probe) call of the second scatter in scatter_basic, built
// from its own recorded fields, writes the same event.
TEST(AggTraceRenderer, DrawsPathCollectionsAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    const Json trace = recorded("scatter_basic");
    const Json* want = nullptr;
    for (const Json& e : trace.find("events")->items())
        if (e.find("op")->asString() == "draw_path_collection" && e.find("paths")->size() == 1) want = &e;
    ASSERT_NE(want, nullptr);
    const auto numbers = [](const Json& j) {
        std::vector<double> out;
        for (const Json& v : j.items()) out.push_back(v.asNumber());
        return out;
    };
    const auto matrix = [&](const Json& j) {
        const auto v = numbers(j);
        return calculate_plot::transforms::Matrix{v[0], v[1], v[2], v[3], v[4], v[5]};
    };
    PathCollectionArgs a;
    a.master = matrix(*want->find("master_transform"));
    const Json& p = (*want->find("paths"))[0];
    std::vector<unsigned char> codes;
    for (const Json& c : p.find("codes")->items()) codes.push_back(static_cast<unsigned char>(c.asInteger()));
    a.paths.push_back(Path::make(numbers(*p.find("vertices")), codes, rc));
    const auto t = numbers(*want->find("transforms")->find("data"));
    for (std::size_t i = 0; i + 8 < t.size() + 1; i += 9) a.transforms.push_back({t[i], t[i + 1], t[i + 2], t[i + 3], t[i + 4], t[i + 5]});
    const auto o = numbers(*want->find("offsets")->find("data"));
    for (std::size_t i = 0; i + 1 < o.size(); i += 2) a.offsets.push_back({o[i], o[i + 1]});
    a.offsetTrans = matrix(*want->find("offset_transform"));
    const auto colors = [&](const char* key) {
        std::vector<std::array<double, 4>> out;
        const auto d = numbers(*want->find(key)->find("data"));
        for (std::size_t i = 0; i + 3 < d.size(); i += 4) out.push_back({d[i], d[i + 1], d[i + 2], d[i + 3]});
        return out;
    };
    a.facecolors = colors("facecolors");
    a.edgecolors = colors("edgecolors");
    a.hatchcolors = colors("hatchcolors");
    a.linewidths = numbers(*want->find("linewidths")->find("data"));
    a.dashes = {{0.0, std::nullopt}};
    a.antialiaseds = {true};
    AggTraceRenderer r(400, 300, 100, rc, "C.UTF-8", fonts());
    GC gc(rc);
    gc.cliprect = std::array<double, 4>{50.0, 32.99999999999999, 360.0, 264.0};
    r.drawPathCollection(gc, a);
    EXPECT_EQ(firstDifference(events(r)[0], *want, "event"), "");
}

}  // namespace
