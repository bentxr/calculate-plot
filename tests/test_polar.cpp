// The polar projection's transforms equal matplotlib 3.11.2's (tests/oracle/polar.json): PolarTransform of points
// and of paths (arcs at a constant radius, straight lines at a constant angle, interpolation, NaNs removed, long
// paths simplified), with radial origins, reversed radii and a log radial scale; InvertedPolarTransform;
// PolarAffine's matrix; _WedgeBbox's points. Exact where the C library is glibc, as the oracle's.
#include "engine/polar.hpp"
#include "rc/rc_params.hpp"
#include "support/histogram.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using namespace calculate_plot::engine;
namespace transforms = calculate_plot::transforms;

namespace {

double hex(const Json& j) { return std::strtod(j.asString().c_str(), nullptr); }

Json load() {
    std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/polar.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    auto oracle = calculate_plot::support::parseJson(text.str(), nullptr);
    EXPECT_TRUE(oracle.has_value());
    return *oracle;
}

// the same double with the sign of zero, NaN as NaN; within 1e-12 where the C library's sin, cos, atan2 and hypot
// are not glibc's (trace_compare.hpp)
void expectSame(double expected, double actual, const std::string& where) {
    if (std::isnan(expected))
        EXPECT_TRUE(std::isnan(actual)) << where;
    else
        EXPECT_TRUE(calculate_plot::testing_support::sameNumber(expected, actual)) << where << ": " << expected << " vs " << actual;
}

std::vector<double> flat(const Json& pairs) {
    std::vector<double> out;
    for (const Json& p : pairs.items()) out.push_back(hex(p.items()[0])), out.push_back(hex(p.items()[1]));
    return out;
}

transforms::BboxPtr box(const Json& view) {
    const auto v = flat(view);
    return std::make_shared<transforms::Bbox>(std::array<double, 4>{v[0], v[1], v[2], v[3]});
}

transforms::TransformPtr scaleOf(const std::string& name) {
    transforms::TransformPtr identity = std::make_shared<transforms::IdentityTransform>();
    if (name == "linear") return std::make_shared<transforms::TransformWrapper>(identity);
    return std::make_shared<transforms::TransformWrapper>(transforms::blend(identity, std::make_shared<transforms::LogTransform>(10)));
}

Path pathOf(const std::string& name, const calculate_plot::rc::Params& rc) {
    const double pi = M_PI;
    const auto line = [&](std::vector<double> ts, std::vector<double> rs, std::vector<unsigned char> codes, int steps) {
        std::vector<double> v;
        for (std::size_t i = 0; i < ts.size(); ++i) v.push_back(ts[i]), v.push_back(rs[i]);
        Path p = Path::make(std::move(v), std::move(codes), rc);
        p.interpolationSteps = steps;
        return p;
    };
    const double nan = std::nan("");
    if (name == "pointwise") return line({0, 1, 2, 3}, {1, 2, 1.5, 0.5}, {}, 1);
    if (name == "gridline r") return line({0, 2 * pi}, {2, 2}, {}, 180);
    if (name == "gridline theta") return line({0.7, 0.7}, {0, 1}, {}, 180);
    if (name == "interpolated") return line({0, 1.3, 2.9}, {0.5, 2.5, 1.0}, {}, 4);
    if (name == "arc back") return line({3, 0.5}, {1.5, 1.5}, {}, 2);
    if (name == "arc beyond") return line({0, 9}, {1, 1}, {}, 2);
    if (name == "arc beyond back") return line({2, -13}, {2.5, 2.5}, {}, 2);
    if (name == "arc exactly") return line({0, 4 * pi}, {1, 1}, {}, 2);
    if (name == "rectangle")
        return line({0.2, 1.1, 1.1, 0.2, 0.2}, {1, 1, 2, 2, 1}, {MOVETO, LINETO, LINETO, LINETO, CLOSEPOLY}, 100);
    if (name == "nans") return line({0, 0.5, nan, 1.5, 2.0, 2.5}, {1, 1, 1, 2, nan, 2}, {}, 3);
    if (name == "curves")
        return line({0, 0.5, 1, 1.5, 2, 2.5, 3}, {1, 2, 1, 2, 1, 2, 2}, {MOVETO, CURVE3, CURVE3, CURVE4, CURVE4, CURVE4, LINETO}, 2);
    if (name == "subpaths") return line({0, 1, 2, 2, 3}, {1, 1, 1, 2, 2}, {MOVETO, LINETO, MOVETO, LINETO, LINETO}, 5);
    if (name == "long" || name == "long same radius") {
        const bool same = name == "long same radius";
        const long long n = same ? 140 : 150;
        const auto ts = calculate_plot::support::npLinspace(0, 6, n);
        std::vector<double> rs;
        for (double s : calculate_plot::support::npLinspace(0, 30, n)) rs.push_back(same ? 1.0 : 2 + std::sin(s));
        return line(ts, rs, {}, 2);
    }
    if (name == "negative") return line({0, 1, 1}, {-0.5, -0.5, 2}, {}, 2);
    if (name == "empty") return line({}, {}, {}, 2);
    ADD_FAILURE() << "no path " << name;
    return {};
}

}  // namespace

TEST(Polar, TransformsPointsAndPathsAsMatplotlibs) {
    const Json oracle = load();
    const auto rc = calculate_plot::rc::Params::defaults();
    ASSERT_EQ(oracle.find("transforms")->size(), 7u);
    for (const Json& t : oracle.find("transforms")->items()) {
        const std::string name = t.find("name")->asString();
        transforms::BboxPtr origin;
        if (!t.find("view")->isNull()) {
            auto lockable = std::make_shared<transforms::LockableBbox>(box(*t.find("view")));
            if (!t.find("rorigin")->isNull()) lockable->locked[1] = hex(*t.find("rorigin"));
            origin = lockable;
        }
        const PolarTransform forward(origin, scaleOf(t.find("scale")->asString()), true, rc);
        // points
        const auto points = flat(*oracle.find("points"));
        const auto expectedPoints = flat(*t.find("points"));
        for (std::size_t i = 0; i + 1 < points.size(); i += 2) {
            const auto q = forward.transformNonAffine(points[i], points[i + 1]);
            expectSame(expectedPoints[i], q[0], name + " x #" + std::to_string(i / 2));
            expectSame(expectedPoints[i + 1], q[1], name + " y #" + std::to_string(i / 2));
        }
        // paths
        for (const Json& p : t.find("paths")->items()) {
            const std::string pname = p.find("name")->asString();
            const Path out = transformPathNonAffine(pathOf(pname, rc), forward);
            const auto expected = flat(*p.find("vertices"));
            const std::string where = name + " / " + pname;
            ASSERT_EQ(expected.size(), out.vertices ? out.vertices->size() : 0) << where;
            for (std::size_t i = 0; i < expected.size(); ++i) expectSame(expected[i], (*out.vertices)[i], where + " #" + std::to_string(i));
            if (p.find("codes")->isNull()) {
                EXPECT_FALSE(out.codes) << where;
            } else {
                ASSERT_TRUE(out.codes) << where;
                std::vector<unsigned char> codes;
                for (const Json& c : p.find("codes")->items()) codes.push_back(static_cast<unsigned char>(c.asNumber()));
                EXPECT_EQ(codes, *out.codes) << where;
            }
            EXPECT_EQ(out.interpolationSteps, 1) << where;
        }
        // the inverse
        const InvertedPolarTransform inverse(origin, true, rc);
        const auto xy = flat(*t.find("inverse")->find("xy"));
        const auto expected = flat(*t.find("inverse")->find("out"));
        for (std::size_t i = 0; i + 1 < xy.size(); i += 2) {
            const auto q = inverse.transformNonAffine(xy[i], xy[i + 1]);
            expectSame(expected[i], q[0], name + " inverse theta #" + std::to_string(i / 2));
            expectSame(expected[i + 1], q[1], name + " inverse r #" + std::to_string(i / 2));
        }
    }
}

TEST(Polar, PolarAffineAsMatplotlibs) {
    const Json oracle = load();
    for (const Json& a : oracle.find("affines")->items()) {
        const PolarAffine affine(scaleOf(a.find("scale")->asString()), box(*a.find("view")));
        const auto m = affine.matrix();
        const auto& e = a.find("matrix")->items();
        const double got[6] = {m.sx, m.shx, m.tx, m.shy, m.sy, m.ty};
        for (std::size_t i = 0; i < 6; ++i) EXPECT_EQ(hex(e[i]), got[i]) << i;
    }
}

TEST(Polar, WedgeBboxAsMatplotlibs) {
    const Json oracle = load();
    ASSERT_EQ(oracle.find("wedges")->size(), 10u);
    for (const Json& w : oracle.find("wedges")->items()) {
        const auto view = box(*w.find("view"));
        transforms::Affine2D direction, offset;
        direction.scale(w.find("direction")->asNumber(), 1.0);
        offset.translate(hex(*w.find("offset")), 0.0);
        const auto shift = transforms::compose(std::make_shared<transforms::Affine2D>(direction), std::make_shared<transforms::Affine2D>(offset));
        auto origin = std::make_shared<transforms::LockableBbox>(view);
        if (!w.find("rorigin")->isNull()) origin->locked[1] = hex(*w.find("rorigin"));
        const WedgeBbox wedge({0.5, 0.5}, std::make_shared<transforms::TransformedBbox>(view, shift), origin);
        const auto got = wedge.points();
        const auto expected = flat(*w.find("points"));
        for (std::size_t i = 0; i < 4; ++i) expectSame(expected[i], got[i], "wedge #" + std::to_string(i));
    }
}
