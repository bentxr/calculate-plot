// The Text artist lays out and draws text as matplotlib 3.11.2 does: 600 random texts recorded from
// matplotlib's Agg (several dpi) and SVG renderers (tests/oracle/text_layout.json, written by
// tools/oracle/make_text_oracle.py): Text._get_layout's box, lines and corner, the window extent and
// the draw_text calls, all equal to the last bit.
#include "engine/figure.hpp"
#include "engine/text.hpp"
#include "engine/trace_renderer.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "text/font_manager.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::engine;

namespace {

std::string fonts() { return std::string(CALCULATE_PLOT_SOURCE_DIR) + "/fonts/ttf"; }

Json oracle() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/text_layout.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return *calculate_plot::support::parseJson(text.str(), nullptr);
}

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

// Where the values went through sin and cos (rotated texts), they are exact only with the libm the
// references were made with (glibc); elsewhere (WASM's musl) they may differ in the last bits, as
// matplotlib's own do from platform to platform. Everything else is exact everywhere.
#ifdef __GLIBC__
constexpr bool kReferenceLibm = true;
#else
constexpr bool kReferenceLibm = false;
#endif
bool trigonometric = false;  // set for each case: the text is rotated

std::string near(const char* what, double got, double want) {
    if (got == want) return {};
    // A few ulps of the coordinates (a few hundred pixels), not of the result: small results come
    // from cancellation.
    if (trigonometric && !kReferenceLibm && std::fabs(got - want) <= 1e-12) return {};
    std::ostringstream out;
    out.precision(17);
    out << what << ": " << got << " instead of " << want;
    return out.str();
}

// Builds the Text of a case the way the oracle did: Text(x, y, s, ha=, va=, rotation=, ...).
std::unique_ptr<Text> textFor(const Json& p, const calculate_plot::rc::Params& rc) {
    auto t = std::make_unique<Text>(rc, p.find("x")->asNumber(), p.find("y")->asNumber(), p.find("text")->asString());
    t->horizontalAlignment = p.find("ha")->asString();
    t->verticalAlignment = p.find("va")->asString();
    const Json& rotation = *p.find("rotation");
    if (rotation.isNumber()) t->setRotation(rotation.asNumber());
    else if (rotation.isString()) t->setRotation(rotation.asString());
    if (p.find("rotation_mode")->isString()) t->rotationMode = p.find("rotation_mode")->asString();
    if (p.find("linespacing")->isNumber()) t->linespacing = p.find("linespacing")->asNumber();
    if (p.find("multialignment")->isString()) t->multialignment = p.find("multialignment")->asString();
    const Json& size = *p.find("fontsize");
    t->font.size = size.isNumber() ? size.asNumber() : *calculate_plot::text::fontSize(size.asString(), rc);
    if (p.find("family")->isString()) t->font.family = {p.find("family")->asString()};
    const Json& weight = *p.find("weight");
    if (weight.isNumber()) t->font.weight = int(weight.asInteger());
    else if (weight.isString()) t->font.weight = weight.asString();
    if (p.find("style")->isString()) t->font.style = p.find("style")->asString();
    if (p.find("language")->isString()) t->language = p.find("language")->asString();
    return t;
}

TEST(TextArtist, LaysOutAndDrawsAsMatplotlib) {
    const auto rc = calculate_plot::rc::Params::defaults();
    const Json cases = oracle();
    int index = 0, svg = 0;
    for (const Json& c : cases.items()) {
        SCOPED_TRACE("case " + std::to_string(index++));
        const Json& p = *c.find("props");
        FigureOptions options;
        options.figsize = std::array<double, 2>{(*c.find("figsize"))[0].asNumber(), (*c.find("figsize"))[1].asNumber()};
        options.dpi = c.find("dpi")->asNumber();
        Figure figure(rc, options);
        auto t = textFor(p, rc);
        t->figure = &figure;
        EXPECT_EQ(t->rotation(), hex(*c.find("rotation")));
        trigonometric = t->rotation() != 0;
        std::vector<std::string> files;
        for (const Json& f : c.find("fonts")->items()) files.push_back(f.asString());
        EXPECT_EQ(calculate_plot::text::findFontsByProps(t->font, rc), files);

        std::unique_ptr<TraceRenderer> renderer;
        const bool isSvg = c.find("backend")->asString() == "svg";
        const auto box = figure.bbox->points();
        if (isSvg) {
            ++svg;
            renderer = std::make_unique<SvgTraceRenderer>(options.figsize->at(0) * 72, options.figsize->at(1) * 72,
                                                          72.0, rc, "C.UTF-8", fonts(), SvgMetadata{}, "s");
        } else {
            auto agg = std::make_unique<AggTraceRenderer>(double(int(box[2] - box[0])), double(int(box[3] - box[1])),
                                                          figure.dpi, rc, "C.UTF-8", fonts());
            renderer = std::move(agg);
        }

        if (const Json* bbox = c.find("bbox")) {
            const auto layout = t->layout(*renderer);
            const auto b = layout.bbox.bounds();
            for (int i = 0; i < 4; ++i) EXPECT_EQ(near("bbox", b[i], hex((*bbox)[i])), "") << i;
            const Json& lines = *c.find("lines");
            ASSERT_EQ(layout.lines.size(), lines.size());
            for (std::size_t i = 0; i < lines.size(); ++i) {
                const Json& l = lines[i];
                EXPECT_EQ(layout.lines[i].text, l.find("s")->asString());
                EXPECT_EQ(near("width", layout.lines[i].width, hex((*l.find("wad"))[0])), "");
                EXPECT_EQ(near("ascent", layout.lines[i].ascent, hex((*l.find("wad"))[1])), "");
                EXPECT_EQ(near("descent", layout.lines[i].descent, hex((*l.find("wad"))[2])), "");
                EXPECT_EQ(near("x", layout.lines[i].x, hex((*l.find("xy"))[0])), "");
                EXPECT_EQ(near("y", layout.lines[i].y, hex((*l.find("xy"))[1])), "");
            }
            const Json& corner = *c.find("corner");
            EXPECT_EQ(near("corner x", layout.cornerX, hex(corner[0])), "");
            EXPECT_EQ(near("corner y", layout.cornerY, hex(corner[1])), "");
            EXPECT_EQ(near("box width", layout.boxWidth, hex(corner[2])), "");
            EXPECT_EQ(near("box height", layout.boxHeight, hex(corner[3])), "");
        }
        const auto extent = t->windowExtent(*renderer).bounds();
        for (int i = 0; i < 4; ++i) EXPECT_EQ(near("extent", extent[i], hex((*c.find("extent"))[i])), "") << i;

        t->draw(*renderer);
        EXPECT_EQ(renderer->error(), "");
        const auto trace = calculate_plot::support::parseJson(renderer->trace(), nullptr);
        std::vector<const Json*> calls;
        for (const Json& e : trace->find("events")->items()) {
            const std::string op = e.find("op")->asString();
            if (op == (isSvg ? "draw_text" : "text")) calls.push_back(&e);
        }
        const Json& want = *c.find("draw");
        ASSERT_EQ(calls.size(), want.size());
        for (std::size_t i = 0; i < want.size(); ++i) {
            const Json& w = want[i];
            EXPECT_EQ(calls[i]->find("s")->asString(), w.find("s")->asString());
            EXPECT_EQ(near("draw x", calls[i]->find("x")->asNumber(), hex(*w.find("x"))), "");
            EXPECT_EQ(near("draw y", calls[i]->find("y")->asNumber(), hex(*w.find("y"))), "");
            EXPECT_EQ(near("angle", calls[i]->find("angle")->asNumber(), hex(*w.find("angle"))), "");
            EXPECT_EQ(near("size", calls[i]->find("size")->asNumber(), hex(*w.find("size"))), "");
            // Only a one-line text hands itself (its language, its features) to draw_text.
            const Json& language = *calls[i]->find("language");
            if (w.find("language")->isString()) EXPECT_EQ(language.isString() ? language.asString() : "(none)", w.find("language")->asString());
            else EXPECT_TRUE(language.isNull());
        }
    }
    EXPECT_EQ(index, 600);
    EXPECT_GT(svg, 100);
}

}  // namespace
