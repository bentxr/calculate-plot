// bezier.py's routines against matplotlib 3.11.2 (tests/oracle/bezier.json, written by
// tools/oracle/make_bezier_oracle.py).
#include "engine/bezier.hpp"
#include "rc/rc_params.hpp"
#include "support/json.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
namespace support = calculate_plot::support;
namespace bezier = calculate_plot::engine::bezier;
using calculate_plot::engine::Path;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/bezier.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double number(const Json& v) { return v.isString() ? std::strtod(v.asString().c_str(), nullptr) : v.asNumber(); }

bezier::Points points(const Json& list) {
    bezier::Points out;
    for (const Json& p : list.items()) out.push_back({number(p[0]), number(p[1])});
    return out;
}

// equal bit for bit natively; under node, libm's last bits may differ (as the figure traces allow)
bool same(double got, double want) { return calculate_plot::testing_support::sameNumber(got, want); }

::testing::AssertionResult samePoints(const bezier::Points& got, const Json& want) {
    const auto w = points(want);
    if (got.size() != w.size()) return ::testing::AssertionFailure() << got.size() << " points instead of " << w.size();
    for (std::size_t i = 0; i < w.size(); ++i)
        for (int d = 0; d < 2; ++d)
            if (!same(got[i][std::size_t(d)], w[i][std::size_t(d)]))
                return ::testing::AssertionFailure() << "point " << i << "[" << d << "]: " << got[i][std::size_t(d)]
                                                     << " instead of " << w[i][std::size_t(d)];
    return ::testing::AssertionSuccess();
}

bezier::Points pathPoints(const Path& p) {
    bezier::Points out;
    for (std::size_t k = 0; k < p.size(); ++k) out.push_back({(*p.vertices)[2 * k], (*p.vertices)[2 * k + 1]});
    return out;
}

}  // namespace

TEST(Bezier, SegmentsAndSplitsAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("evals")->items()) {
        const auto cp = points(*c.find("points"));
        for (std::size_t i = 0; i < c.find("t")->size(); ++i) {
            const auto p = bezier::at(cp, number((*c.find("t"))[i]));
            EXPECT_TRUE(samePoints({p}, (*c.find("values"))[i])) << "eval " << checked << " at t " << i;
        }
        ++checked;
    }
    for (const Json& c : oracle().find("splits")->items()) {
        const auto halves = bezier::splitDeCasteljau(points(*c.find("points")), number(*c.find("t")));
        EXPECT_TRUE(samePoints(halves.first, *c.find("left"))) << "split " << checked;
        EXPECT_TRUE(samePoints(halves.second, *c.find("right"))) << "split " << checked;
        ++checked;
    }
    EXPECT_EQ(checked, 23 + 92);
}

TEST(Bezier, IntersectionsWithCirclesAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("intersections")->items()) {
        const auto cp = points(*c.find("points"));
        const Json& circle = *c.find("circle");
        const auto inside = bezier::insideCircle(number(circle[0]), number(circle[1]), number(circle[2]));
        double t0 = 0, t1 = 1;
        const bool ok = bezier::findTIntersecting([&](double t) { return bezier::at(cp, t); }, inside, &t0, &t1);
        ASSERT_EQ(ok, c.find("error") == nullptr) << checked;
        if (ok) {
            EXPECT_TRUE(same(t0, number((*c.find("t"))[0])) && same(t1, number((*c.find("t"))[1]))) << "case " << checked;
            bezier::Points left, right;
            ASSERT_TRUE(bezier::splitIntersecting(cp, inside, &left, &right));
            EXPECT_TRUE(samePoints(left, *c.find("left"))) << "case " << checked;
            EXPECT_TRUE(samePoints(right, *c.find("right"))) << "case " << checked;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 184);
}

TEST(Bezier, PathsSplitInAndOutAsMatplotlibSplitsThem) {
    const auto& rc = calculate_plot::rc::Params::defaults();
    int checked = 0;
    for (const Json& c : oracle().find("inouts")->items()) {
        std::vector<double> v;
        for (const Json& p : c.find("vertices")->items()) v.push_back(number(p[0])), v.push_back(number(p[1]));
        std::vector<unsigned char> codes;
        if (!c.find("codes")->isNull())
            for (const Json& k : c.find("codes")->items()) codes.push_back((unsigned char)k.asNumber());
        const Path path = Path::make(v, codes, rc);
        const Json& circle = *c.find("circle");
        Path in, out;
        const bool ok = bezier::splitPathInout(path, bezier::insideCircle(number(circle[0]), number(circle[1]), number(circle[2])), rc,
                                               &in, &out);
        ASSERT_EQ(ok, c.find("error") == nullptr) << checked;
        if (ok) {
            for (const auto& [got, key] : {std::pair<const Path*, const char*>{&in, "in"}, {&out, "out"}}) {
                const Json& want = *c.find(key);
                EXPECT_TRUE(samePoints(pathPoints(*got), *want.find("vertices"))) << "case " << checked << " " << key;
                std::vector<unsigned char> wantCodes;
                if (!want.find("codes")->isNull())
                    for (const Json& k : want.find("codes")->items()) wantCodes.push_back((unsigned char)k.asNumber());
                const std::vector<unsigned char> gotCodes = got->codes ? *got->codes : std::vector<unsigned char>{};
                EXPECT_EQ(gotCodes, wantCodes) << "case " << checked << " " << key;
            }
        }
        ++checked;
    }
    EXPECT_EQ(checked, 60);
}

TEST(Bezier, ParallelAndWedgedCurvesAreMatplotlibs) {
    int checked = 0;
    for (const Json& c : oracle().find("geometry")->items()) {
        const auto q = points(*c.find("bezier2"));
        const double w = number(*c.find("width"));
        const auto par = bezier::parallels(q, w);
        EXPECT_TRUE(samePoints(par.first, (*c.find("parallels"))[0])) << checked;
        EXPECT_TRUE(samePoints(par.second, (*c.find("parallels"))[1])) << checked;
        for (const Json& wc : c.find("wedged")->items()) {
            const Json& ws = *wc.find("w");
            const auto wd = bezier::wedged(q, w, number(ws[0]), number(ws[1]), number(ws[2]));
            EXPECT_TRUE(samePoints(wd.first, *wc.find("left"))) << checked;
            EXPECT_TRUE(samePoints(wd.second, *wc.find("right"))) << checked;
        }
        ++checked;
    }
    for (const Json& c : oracle().find("lines")->items()) {
        const Json& p = *c.find("p");
        const double x0 = number(p[0]), y0 = number(p[1]), x1 = number(p[2]), y1 = number(p[3]);
        const auto cs = bezier::cosSin(x0, y0, x1, y1);
        EXPECT_TRUE(same(cs[0], number((*c.find("cos_sin"))[0])) && same(cs[1], number((*c.find("cos_sin"))[1]))) << checked;
        EXPECT_EQ(bezier::checkIfParallel(x0, y0, x1, y1), int(c.find("parallel")->asNumber())) << checked;
        if (const Json* a = c.find("angles")) {
            double x = 0, y = 0;
            const bool ok = bezier::intersection(x0, y0, number((*a)[0]), number((*a)[1]), x1, y1, number((*a)[2]), number((*a)[3]), &x, &y);
            ASSERT_EQ(ok, c.find("intersection") != nullptr) << checked;
            if (ok) {
                EXPECT_TRUE(same(x, number((*c.find("intersection"))[0])) && same(y, number((*c.find("intersection"))[1]))) << checked;
            }
            const auto np = bezier::normalPoints(x0, y0, number((*a)[0]), number((*a)[1]), 3.5);
            for (int k = 0; k < 4; ++k) EXPECT_TRUE(same(np[std::size_t(k)], number((*c.find("normal"))[std::size_t(k)]))) << checked;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 51 + 46);
}
