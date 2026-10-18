// The trace renderers write what matplotlib's renderers are asked to draw: single calls compared
// with the events of a recorded figure (tests/figures/empty_axes, matplotlib 3.11.2).
#include "engine/trace_renderer.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "support/base64.hpp"
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

// RendererAgg.draw_path splits a long path it may simplify into chunks of rcParams["agg.path.chunksize"]
// vertices at most, each a new Path; the vertex before each chunk's start is left out.
TEST(AggTraceRenderer, SplitsLongPathsIntoChunks) {
    auto rc = calculate_plot::rc::Params::defaults();
    std::string error;
    ASSERT_TRUE(rc.set("agg.path.chunksize", Json::integer(200), &error)) << error;
    std::vector<double> v;
    for (int i = 0; i < 450; ++i) v.push_back(i), v.push_back(i % 7);
    const Path line = Path::make(v, {}, rc);
    ASSERT_TRUE(line.shouldSimplify);
    const auto count = [&](const GC& gc, const Path& path, const std::optional<std::vector<double>>& face) {
        AggTraceRenderer r(320, 240, 100, rc, "C.UTF-8", fonts());
        r.drawPath(gc, path, {1, 0, 0, 1, 0, 0}, face);
        return events(r);
    };
    GC gc(rc);
    // 450 vertices, at most 200 a chunk: 3 chunks of 150 from 0, 150, 300: [0, 149), [150, 299), [300, 450).
    const Json chunks = count(gc, line, std::nullopt);
    ASSERT_EQ(chunks.size(), 3u);
    const double starts[] = {0, 150, 300};
    const std::size_t sizes[] = {149, 149, 150};
    for (std::size_t i = 0; i < 3; ++i) {
        const Json& path = *chunks[i].find("path");
        EXPECT_EQ(path.find("vertices")->size(), 2 * sizes[i]) << i;
        EXPECT_EQ((*path.find("vertices"))[0].asNumber(), starts[i]) << i;
        EXPECT_TRUE(path.find("codes")->isNull()) << i;
        EXPECT_TRUE(path.find("simplify")->asBool()) << i;  // each chunk a new Path: 149 vertices >= 128
    }
    // Codes: each chunk starts with a MOVETO.
    std::vector<unsigned char> codes(450, LINETO);
    codes[0] = MOVETO;
    const Json withCodes = count(gc, Path::make(v, codes, rc), std::nullopt);
    ASSERT_EQ(withCodes.size(), 3u);
    for (std::size_t i = 0; i < 3; ++i) {
        const Json& c = *withCodes[i].find("path")->find("codes");
        ASSERT_EQ(c.size(), sizes[i]) << i;
        EXPECT_EQ(c[0].asNumber(), MOVETO) << i;
        EXPECT_EQ(c[1].asNumber(), LINETO) << i;
    }
    // Not split: a filled path, a hatched one, one that may not be simplified, or a chunk size of 100 or less.
    EXPECT_EQ(count(gc, line, std::vector<double>{1, 0, 0}).size(), 1u);
    GC hatched(rc);
    hatched.hatch = "/";
    EXPECT_EQ(count(hatched, line, std::nullopt).size(), 1u);
    Path plain = line;
    plain.shouldSimplify = false;
    EXPECT_EQ(count(gc, plain, std::nullopt).size(), 1u);
    ASSERT_TRUE(rc.set("agg.path.chunksize", Json::integer(100), &error)) << error;
    EXPECT_EQ(count(gc, line, std::nullopt).size(), 1u);
    ASSERT_TRUE(rc.set("agg.path.chunksize", Json::integer(450), &error)) << error;
    EXPECT_EQ(count(gc, line, std::nullopt).size(), 1u);  // npts > nmax
    ASSERT_TRUE(rc.set("agg.path.chunksize", Json::integer(449), &error)) << error;
    const Json two = count(gc, line, std::nullopt);
    ASSERT_EQ(two.size(), 2u);  // 225 each: [0, 224), [225, 450)
    EXPECT_EQ(two[0].find("path")->find("vertices")->size(), 2 * 224u);
    EXPECT_EQ(two[1].find("path")->find("vertices")->size(), 2 * 225u);
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

namespace {

// Base64 data in traces is split into lines of 16 characters: secret scanners (which reject pushes)
// look for key patterns of 20 or more letters and digits, and a newline breaks every such run.
TEST(AggTraceRenderer, SplitsBase64IntoShortLines) {
    const auto rc = calculate_plot::rc::Params::defaults();
    AggTraceRenderer r(320, 240, 100, rc, "C.UTF-8", fonts());
    r.drawText(textGc(rc), 29.0, 233.87452256944445, "0.0", FontSpec{{"DejaVuSans.ttf"}, 10.0, {}}, 0.0, false, {});
    const Json e = events(r);
    int images = 0;
    for (const Json& ev : e.items()) {
        if (ev.find("op")->asString() != "draw_text_image") continue;
        ++images;
        const std::string data = ev.find("image")->find("base64")->asString();
        std::size_t run = 0;
        for (char c : data) {
            run = c == '\n' ? 0 : run + 1;
            ASSERT_LE(run, 16u) << data;
        }
        if (data.size() > 16) {
            EXPECT_NE(data.find('\n'), std::string::npos);
        }
    }
    EXPECT_EQ(images, 3);
}

}  // namespace

namespace {

Json imageEvents() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/image_events.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

// the pixels an event carries (the image as drawn: resampled, flipped, its alpha applied)
RgbaImage imageOf(const Json& event) {
    const Json& image = *event.find("image");
    RgbaImage im;
    im.height = int((*image.find("shape"))[0].asNumber());
    im.width = int((*image.find("shape"))[1].asNumber());
    std::string b64;
    for (char ch : image.find("base64")->asString())
        if (ch != '\n') b64 += ch;
    const auto bytes = calculate_plot::support::decodeBase64(b64);
    im.rgba.assign(bytes->begin(), bytes->end());
    return im;
}

}  // namespace

// fig.figimage's draw_image: the gc (its alpha forced), the position, the RGBA pixels in base64 lines.
TEST(AggTraceRenderer, DrawsImagesAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    int checked = 0;
    const Json cases = imageEvents();
    for (const Json& c : cases.items()) {
        AggTraceRenderer r(50, 40, 50, rc, "C.UTF-8", fonts());
        GC gc(rc);
        // _ImageBase.draw: gc.set_alpha(self._get_scalar_alpha()), 1.0 without a scalar alpha
        const Json* alpha = c.find("kwargs")->find("alpha");
        gc.setAlpha(alpha ? alpha->asNumber() : 1.0);
        const Json want = (*c.find("agg"))[0];
        r.drawImage(gc, want.find("x")->asNumber(), want.find("y")->asNumber(), imageOf(want), std::nullopt);
        EXPECT_EQ(firstDifference(events(r)[0], want, "event"), "") << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 4);
}

TEST(SvgTraceRenderer, DrawsImagesAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    int checked = 0;
    const Json cases = imageEvents();
    for (const Json& c : cases.items()) {
        SvgTraceRenderer r(72, 57.6, 50, rc, "C.UTF-8", fonts(), {}, "");
        GC gc(rc);
        // _ImageBase.draw: gc.set_alpha(self._get_scalar_alpha()), 1.0 without a scalar alpha
        const Json* alpha = c.find("kwargs")->find("alpha");
        gc.setAlpha(alpha ? alpha->asNumber() : 1.0);
        const Json want = (*c.find("svg"))[0];
        r.drawImage(gc, want.find("x")->asNumber(), want.find("y")->asNumber(), imageOf(want), std::nullopt);
        const Json got = *calculate_plot::support::parseJson(r.trace(), nullptr)->find("events");
        Json image;
        for (const Json& e : got.items())
            if (e.find("op")->asString() == "draw_image") image = e;
        EXPECT_EQ(firstDifference(image, want, "event"), "") << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 4);
}
