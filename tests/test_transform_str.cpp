// str() of transforms, as RendererSVG hashes it for the ids of clip paths, and NumPy's str() of the float64
// arrays an Affine2D prints (tests/oracle/transform_str.json, recorded from matplotlib and NumPy).
#include "support/json.hpp"
#include "support/numpy_str.hpp"
#include "transforms/transforms.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;
namespace tr = calculate_plot::transforms;
using calculate_plot::support::Json;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/transform_str.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *calculate_plot::support::parseJson(text.str(), nullptr);
    }();
    return o;
}

std::shared_ptr<tr::Affine2D> affine() { return std::make_shared<tr::Affine2D>(); }

}  // namespace

TEST(NumpyStr, FloatArraysAreNumpys) {
    int checked = 0;
    for (const Json& c : oracle().find("numpy_str")->items()) {
        std::vector<std::vector<double>> rows;
        for (const Json& r : c.find("rows")->items()) {
            rows.emplace_back();
            for (const Json& v : r.items()) rows.back().push_back(std::strtod(v.asString().c_str(), nullptr));
        }
        EXPECT_EQ(calculate_plot::support::numpyStr(rows), c.find("str")->asString()) << "case " << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 166);
}

// repr() and str() of 2-D object arrays (subplot_mosaic's layouts in its errors), as NumPy writes them
TEST(NumpyStr, ObjectArraysAreNumpys) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/object_arrays.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const auto o = calculate_plot::support::parseJson(text.str(), nullptr);
    ASSERT_TRUE(o.has_value());
    int checked = 0;
    for (const Json& c : o->find("cases")->items()) {
        std::vector<std::vector<std::string>> words;
        for (const Json& r : c.find("words")->items()) {
            words.emplace_back();
            for (const Json& w : r.items()) words.back().push_back(w.asString());
        }
        EXPECT_EQ(calculate_plot::support::numpyObjectArray(words, true), c.find("repr")->asString()) << "case " << checked;
        EXPECT_EQ(calculate_plot::support::numpyObjectArray(words, false), c.find("str")->asString()) << "case " << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 13);
}

TEST(TransformStr, TreesAreMatplotlibs) {
    const Json& want = *oracle().find("transforms");
    auto dpi = affine();
    dpi->scale(72.0);
    const auto figBbox = std::make_shared<tr::TransformedBbox>(tr::Bbox::fromBounds(0, 0, 6.4, 4.8), dpi);
    const auto axBbox = std::make_shared<tr::TransformedBbox>(
        std::make_shared<tr::Bbox>(std::array<double, 4>{0.7837500000000002, 0.10999999999999999, 0.8126250000000002, 0.88}),
        std::make_shared<tr::BboxTransformTo>(figBbox));
    const auto flip = [] {
        auto f = affine();
        f->scale(1, -1).translate(0, 345.6);
        return f;
    };
    const auto check = [&](const char* name, const tr::TransformPtr& t) {
        const auto s = t->str();
        ASSERT_TRUE(s.has_value()) << name;
        EXPECT_EQ(*s, want.find(name)->asString()) << name;
    };
    check("identity", std::make_shared<tr::IdentityTransform>());
    auto scale = affine();
    scale->scale(72.0);
    check("scale", scale);
    auto scaleXY = affine();
    scaleXY->scale(2.0, 3.5);
    check("scale_xy", scaleXY);
    check("flip", flip());
    auto rotate = affine();
    rotate->rotateDeg(30);
    check("rotate", rotate);
    check("axes_flip", tr::compose(std::make_shared<tr::BboxTransformTo>(axBbox), flip()));
    auto two = affine();
    two->scale(2.0);
    auto shift = affine();
    shift->translate(1, 2);
    check("affines", tr::compose(two, shift));
    check("identity_dropped", tr::compose(std::make_shared<tr::IdentityTransform>(), flip()));
    check("from_to", std::make_shared<tr::BboxTransform>(tr::Bbox::fromBounds(0, 0, 1, 1), tr::Bbox::fromBounds(1, 2, 3, 4)));
    check("from", std::make_shared<tr::BboxTransformFrom>(tr::Bbox::fromBounds(1, 2, 3, 4)));
    check("blended", tr::blend(two, std::make_shared<tr::BboxTransformTo>(axBbox)));
}
