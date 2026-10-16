// Derived from Matplotlib 3.11.2: lib/matplotlib/bezier.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/bezier.hpp"

#include "support/blas_dot.hpp"

#include <cmath>

namespace calculate_plot::engine::bezier {

Point at(const Points& points, double t) {
    // _px = control points * comb(N - 1, i); (power.outer(1 - t, orders[::-1]) * power.outer(t, orders)) @ _px,
    // a vector times a matrix: NumPy calls BLAS's dgemv
    const std::size_t N = points.size(), n = N - 1;
    std::vector<std::array<double, 2>> px(N);
    std::vector<double> w(N);
    double c = 1;
    for (std::size_t k = 0; k < N; ++k) {
        px[k] = {points[k][0] * c, points[k][1] * c};
        c = c * double(n - k) / double(k + 1);
        w[k] = std::pow(1 - t, double(n - k)) * std::pow(t, double(k));
    }
    return support::blasGemvN2(px, w);
}

std::pair<Points, Points> splitDeCasteljau(const Points& beta, double t) {
    std::vector<Points> list{beta};
    Points b = beta;
    while (true) {
        Points next;  // _de_casteljau1: beta[:-1] * (1 - t) + beta[1:] * t
        for (std::size_t i = 0; i + 1 < b.size(); ++i)
            next.push_back({b[i][0] * (1 - t) + b[i + 1][0] * t, b[i][1] * (1 - t) + b[i + 1][1] * t});
        b = next;
        list.push_back(b);
        if (b.size() == 1) break;
    }
    Points left, right;
    for (const auto& p : list) left.push_back(p.front());
    for (auto it = list.rbegin(); it != list.rend(); ++it) right.push_back(it->back());
    return {left, right};
}

Inside insideCircle(double cx, double cy, double r) {
    const double r2 = std::pow(r, 2.0);
    return [cx, cy, r2](const Point& p) { return std::pow(p[0] - cx, 2.0) + std::pow(p[1] - cy, 2.0) < r2; };
}

bool findTIntersecting(const std::function<Point(double)>& pointAt, const Inside& inside, double* t0, double* t1,
                       double tolerance) {
    Point start = pointAt(*t0), end = pointAt(*t1);
    bool startInside = inside(start);
    const bool endInside = inside(end);
    if (startInside == endInside && start != end) return false;
    while (true) {
        if (std::hypot(start[0] - end[0], start[1] - end[1]) < tolerance) return true;
        const double middleT = 0.5 * (*t0 + *t1);
        const Point middle = pointAt(middleT);
        const bool middleInside = inside(middle);
        if (startInside ^ middleInside) {
            *t1 = middleT;
            if (end == middle) return true;  // (no endless loop with large numbers)
            end = middle;
        } else {
            *t0 = middleT;
            if (start == middle) return true;
            start = middle;
            startInside = middleInside;
        }
    }
}

bool splitIntersecting(const Points& points, const Inside& inside, Points* left, Points* right, double tolerance) {
    double t0 = 0., t1 = 1.;
    if (!findTIntersecting([&](double t) { return at(points, t); }, inside, &t0, &t1, tolerance)) return false;
    auto halves = splitDeCasteljau(points, (t0 + t1) / 2.);
    *left = std::move(halves.first), *right = std::move(halves.second);
    return true;
}

bool splitPathInout(const Path& path, const Inside& inside, const rc::Params& rc, Path* in, Path* out, double tolerance,
                    bool reorderInout) {
    // path.iter_segments(): each code with its vertices (codes None: a MOVETO, then LINETOs)
    const auto& v = *path.vertices;
    const std::size_t n = path.size();
    const auto code = [&](std::size_t k) -> unsigned { return path.codes ? (*path.codes)[k] : k == 0 ? unsigned(MOVETO) : unsigned(LINETO); };
    const auto vertex = [&](std::size_t k) { return Point{v[2 * k], v[2 * k + 1]}; };
    struct Segment {
        std::size_t first, count;
    };
    std::vector<Segment> segments;
    for (std::size_t k = 0; k < n;) {
        const unsigned c = code(k);
        const std::size_t count = c == CURVE3 ? 2 : c == CURVE4 ? 3 : 1;
        segments.push_back({k, count});
        k += count;
    }
    const auto last = [&](const Segment& s) { return vertex(s.first + s.count - 1); };
    const bool beginInside = inside(last(segments[0]));
    Segment old = segments[0];
    std::size_t iold = 0, i = 1;
    Points bp;
    bool found = false;
    for (std::size_t s = 1; s < segments.size(); ++s) {
        iold = i;
        i += segments[s].count;
        if (inside(last(segments[s])) != beginInside) {
            bp.push_back(last(old));
            for (std::size_t k = 0; k < segments[s].count; ++k) bp.push_back(vertex(segments[s].first + k));
            found = true;
            break;
        }
        old = segments[s];
    }
    if (!found) return false;
    Points left, right;
    if (!splitIntersecting(bp, inside, &left, &right, tolerance)) {
        return false;  // (NonIntersectingPathException is a ValueError: the callers take it as no intersection)
    }
    std::vector<unsigned char> codesLeft, codesRight;
    if (left.size() == 2) codesLeft = {LINETO}, codesRight = {MOVETO, LINETO};
    else if (left.size() == 3) codesLeft = {CURVE3, CURVE3}, codesRight = {MOVETO, CURVE3, CURVE3};
    else codesLeft = {CURVE4, CURVE4, CURVE4}, codesRight = {MOVETO, CURVE4, CURVE4, CURVE4};
    std::vector<double> vin, vout;
    std::vector<unsigned char> cin, cout;
    const std::size_t cut = path.codes ? iold : i;
    for (std::size_t k = 0; k < cut; ++k) vin.push_back(v[2 * k]), vin.push_back(v[2 * k + 1]);
    for (std::size_t k = 1; k < left.size(); ++k) vin.push_back(left[k][0]), vin.push_back(left[k][1]);
    for (const auto& p : right) vout.push_back(p[0]), vout.push_back(p[1]);
    for (std::size_t k = i; k < n; ++k) vout.push_back(v[2 * k]), vout.push_back(v[2 * k + 1]);
    if (path.codes) {
        cin.assign(path.codes->begin(), path.codes->begin() + std::ptrdiff_t(iold));
        cin.insert(cin.end(), codesLeft.begin(), codesLeft.end());
        cout = codesRight;
        cout.insert(cout.end(), path.codes->begin() + std::ptrdiff_t(i), path.codes->end());
    }
    *in = Path::make(std::move(vin), std::move(cin), rc);
    *out = Path::make(std::move(vout), std::move(cout), rc);
    if (reorderInout && !beginInside) std::swap(*in, *out);
    return true;
}

std::array<double, 2> cosSin(double x0, double y0, double x1, double y1) {
    const double dx = x1 - x0, dy = y1 - y0;
    const double d = std::pow(dx * dx + dy * dy, .5);
    if (d == 0) return {0.0, 0.0};
    return {dx / d, dy / d};
}

int checkIfParallel(double dx1, double dy1, double dx2, double dy2, double tolerance) {
    const double theta1 = std::atan2(dx1, dy1), theta2 = std::atan2(dx2, dy2);
    const double dtheta = std::fabs(theta1 - theta2);
    if (dtheta < tolerance) return 1;
    if (std::fabs(dtheta - 3.141592653589793) < tolerance) return -1;  // (np.pi)
    return 0;
}

bool intersection(double cx1, double cy1, double cosT1, double sinT1, double cx2, double cy2, double cosT2,
                  double sinT2, double* x, double* y) {
    const double line1Rhs = sinT1 * cx1 - cosT1 * cy1;
    const double line2Rhs = sinT2 * cx2 - cosT2 * cy2;
    const double a = sinT1, b = -cosT1, c = sinT2, d = -cosT2;
    const double adBc = a * d - b * c;
    if (std::fabs(adBc) < 1e-12) return false;
    const double a_ = d / adBc, b_ = -b / adBc, c_ = -c / adBc, d_ = a / adBc;
    *x = a_ * line1Rhs + b_ * line2Rhs;
    *y = c_ * line1Rhs + d_ * line2Rhs;
    return true;
}

std::array<double, 4> normalPoints(double cx, double cy, double cosT, double sinT, double length) {
    if (length == 0.) return {cx, cy, cx, cy};
    const double cosT1 = sinT, sinT1 = -cosT, cosT2 = -sinT, sinT2 = cosT;
    return {length * cosT1 + cx, length * sinT1 + cy, length * cosT2 + cx, length * sinT2 + cy};
}

std::pair<Points, Points> parallels(const Points& bezier2, double width) {
    const double c1x = bezier2[0][0], c1y = bezier2[0][1], cmx = bezier2[1][0], cmy = bezier2[1][1];
    const double c2x = bezier2[2][0], c2y = bezier2[2][1];
    std::array<double, 2> t1, t2;
    if (checkIfParallel(c1x - cmx, c1y - cmy, cmx - c2x, cmy - c2y) == -1) {
        t1 = t2 = cosSin(c1x, c1y, c2x, c2y);  // (matplotlib warns: a straight line instead)
    } else {
        t1 = cosSin(c1x, c1y, cmx, cmy);
        t2 = cosSin(cmx, cmy, c2x, c2y);
    }
    const auto p1 = normalPoints(c1x, c1y, t1[0], t1[1], width);
    const auto p2 = normalPoints(c2x, c2y, t2[0], t2[1], width);
    double cmxLeft, cmyLeft, cmxRight, cmyRight;
    if (!intersection(p1[0], p1[1], t1[0], t1[1], p2[0], p2[1], t2[0], t2[1], &cmxLeft, &cmyLeft) ||
        !intersection(p1[2], p1[3], t1[0], t1[1], p2[2], p2[3], t2[0], t2[1], &cmxRight, &cmyRight)) {
        // straight lines: the middle points
        cmxLeft = 0.5 * (p1[0] + p2[0]), cmyLeft = 0.5 * (p1[1] + p2[1]);
        cmxRight = 0.5 * (p1[2] + p2[2]), cmyRight = 0.5 * (p1[3] + p2[3]);
    }
    return {{{p1[0], p1[1]}, {cmxLeft, cmyLeft}, {p2[0], p2[1]}}, {{p1[2], p1[3]}, {cmxRight, cmyRight}, {p2[2], p2[3]}}};
}

namespace {

// find_control_points: the quadratic curve from c1 to c2 through mm at t = 0.5
Points controlPoints(double c1x, double c1y, double mmx, double mmy, double c2x, double c2y) {
    const double cmx = .5 * (4 * mmx - (c1x + c2x));
    const double cmy = .5 * (4 * mmy - (c1y + c2y));
    return {{c1x, c1y}, {cmx, cmy}, {c2x, c2y}};
}

}  // namespace

std::pair<Points, Points> wedged(const Points& bezier2, double width, double w1, double wm, double w2) {
    const double c1x = bezier2[0][0], c1y = bezier2[0][1], cmx = bezier2[1][0], cmy = bezier2[1][1];
    const double c3x = bezier2[2][0], c3y = bezier2[2][1];
    const auto t1 = cosSin(c1x, c1y, cmx, cmy), t2 = cosSin(cmx, cmy, c3x, c3y);
    const auto p1 = normalPoints(c1x, c1y, t1[0], t1[1], width * w1);
    const auto p3 = normalPoints(c3x, c3y, t2[0], t2[1], width * w2);
    const double c12x = (c1x + cmx) * .5, c12y = (c1y + cmy) * .5;
    const double c23x = (cmx + c3x) * .5, c23y = (cmy + c3y) * .5;
    const double c123x = (c12x + c23x) * .5, c123y = (c12y + c23y) * .5;
    const auto t123 = cosSin(c12x, c12y, c23x, c23y);
    const auto p123 = normalPoints(c123x, c123y, t123[0], t123[1], width * wm);
    return {controlPoints(p1[0], p1[1], p123[0], p123[1], p3[0], p3[1]),
            controlPoints(p1[2], p1[3], p123[2], p123[3], p3[2], p3[3])};
}

}  // namespace calculate_plot::engine::bezier
