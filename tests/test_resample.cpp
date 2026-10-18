// Image resampling as matplotlib 3.11.2's _image.resample (tests/oracle/resample.json, written by
// tools/oracle/make_resample_oracle.py): grey and RGBA images of four dtypes, every filter, affine transforms and a
// lookup mesh, byte for byte.
#include "render/resample.hpp"
#include "support/base64.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace render = calculate_plot::render;

namespace {

std::string bytes(const Json& v) { return *calculate_plot::support::decodeBase64(v.asString()); }
double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }

TEST(Resample, EqualsMatplotlibs) {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/resample.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    const Json oracle = *calculate_plot::support::parseJson(text.str(), nullptr);
    int checked = 0;
    for (const Json& c : oracle.find("cases")->items()) {
        const std::string where = "case " + std::to_string(checked++);
        const Json& shape = *c.find("shape");
        const Json& outShape = *c.find("out_shape");
        const bool rgba = shape.size() == 3;
        const std::string dtype = c.find("dtype")->asString();
        const auto type = render::pixelTypeOf(dtype, rgba);
        ASSERT_TRUE(type.has_value()) << where;
        render::ResampleParams p;
        p.interpolation = *render::interpolationNamed(c.find("interpolation")->asString());
        p.resample = c.find("resample")->asBool();
        p.alpha = c.find("alpha")->asNumber();
        p.norm = c.find("norm")->asBool();
        p.radius = c.find("radius")->asNumber();
        std::vector<double> mesh;
        if (const Json* a = c.find("affine")) {
            p.affine = calculate_plot::transforms::Matrix{hex((*a)[0]), hex((*a)[2]), hex((*a)[4]),
                                                          hex((*a)[1]), hex((*a)[3]), hex((*a)[5])};
        } else {
            const std::string m = bytes(*c.find("mesh"));
            mesh.resize(m.size() / sizeof(double));
            std::memcpy(mesh.data(), m.data(), m.size());
            p.mesh = &mesh;
        }
        const std::string input = bytes(*c.find("input"));
        const std::string want = bytes(*c.find("output"));
        std::string out(want.size(), '\0');
        std::string error;
        ASSERT_TRUE(render::resample(*type, input.data(), int(shape[1].asNumber()), int(shape[0].asNumber()), out.data(),
                                     int(outShape[1].asNumber()), int(outShape[0].asNumber()), p, &error))
            << where << ": " << error;
        std::size_t first = 0;
        while (first < out.size() && out[first] == want[first]) ++first;
        EXPECT_EQ(first, want.size()) << where << " (" << dtype << (rgba ? " rgba, " : " grey, ")
                                      << c.find("interpolation")->asString() << (c.find("affine") ? "" : ", mesh")
                                      << (p.resample ? ", resample" : "") << "): first difference at byte " << first;
    }
    EXPECT_EQ(checked, 480);
}

TEST(Resample, NamesEveryInterpolation) {
    for (const char* name : {"NEAREST", "BILINEAR", "BICUBIC", "SPLINE16", "SPLINE36", "HANNING", "HAMMING", "HERMITE",
                             "KAISER", "QUADRIC", "CATROM", "GAUSSIAN", "BESSEL", "MITCHELL", "SINC", "LANCZOS", "BLACKMAN"})
        EXPECT_TRUE(render::interpolationNamed(name).has_value()) << name;
    EXPECT_FALSE(render::interpolationNamed("antialiased").has_value());
    EXPECT_FALSE(render::pixelTypeOf("int64", false).has_value());
}

}  // namespace
