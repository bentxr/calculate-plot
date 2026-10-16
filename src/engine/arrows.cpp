// Derived from Matplotlib 3.11.2: lib/matplotlib/patches.py (_Style, ConnectionStyle, ArrowStyle,
// FancyArrowPatch) and lib/matplotlib/path.py (make_compound_path).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/arrows.hpp"

#include "support/pyformat.hpp"
#include "support/pyparse.hpp"
#include "support/raised.hpp"
#include "svg/svg_renderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace calculate_plot::engine {

namespace {

using bezier::Point;
using bezier::Points;

constexpr double kPi = 3.141592653589793;
double radians(double degrees) { return degrees * (kPi / 180.0); }  // math.radians, np.deg2rad

// _Style.__new__: "name,key=value,..." without spaces; the name in lower case, the values floats
bool parseStyle(const std::string& stylename, std::string* name, StyleArgs* args, std::string* error) {
    std::string s;
    for (char c : stylename)
        if (c != ' ') s += c;
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = s.find(',', start);
        parts.push_back(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    *name = parts[0];
    for (char& c : *name) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const std::size_t eq = parts[i].find('=');
        const auto value = eq == std::string::npos || parts[i].find('=', eq + 1) != std::string::npos
                               ? std::nullopt
                               : support::pythonFloat(parts[i].substr(eq + 1));
        if (!value) return *error = "Incorrect style argument: " + svg::pythonStrRepr(stylename), false;
        (*args)[parts[i].substr(0, eq)] = *value;
    }
    return true;
}

// A style class's __init__(**{**args, **kwargs}): each parameter it has (its default unless given)
struct Params {
    const std::string& qualname;
    StyleArgs given;
    std::string* error;
    bool check(std::initializer_list<const char*> names) const {
        for (const auto& entry : given)
            if (std::none_of(names.begin(), names.end(), [&](const char* n) { return entry.first == n; }))
                return *error = qualname + ".__init__() got an unexpected keyword argument " + svg::pythonStrRepr(entry.first), false;
        return true;
    }
    // a number parameter (None only where the default is None)
    bool number(const char* key, double* out) const {
        auto it = given.find(key);
        if (it == given.end()) return true;
        if (!it->second) return *error = qualname + ": " + key + " must be a number", false;
        *out = *it->second;
        return true;
    }
    void optional(const char* key, std::optional<double>* out) const {
        if (auto it = given.find(key); it != given.end()) *out = it->second;
    }
};

}  // namespace

bool ConnectionStyle::make(const std::string& stylename, const StyleArgs& kwargs, ConnectionStyle* out, std::string* error) {
    std::string name;
    StyleArgs args;
    if (!parseStyle(stylename, &name, &args, error)) return false;
    for (const auto& [k, v] : kwargs) args[k] = v;
    static const std::map<std::string, std::string> qualnames = {
        {"arc3", "ConnectionStyle.Arc3"}, {"angle3", "ConnectionStyle.Angle3"}, {"angle", "ConnectionStyle.Angle"},
        {"arc", "ConnectionStyle.Arc"}, {"bar", "ConnectionStyle.Bar"}};
    const auto it = qualnames.find(name);
    if (it == qualnames.end()) return *error = "Unknown style: " + svg::pythonStrRepr(stylename), false;
    const Params p{it->second, args, error};
    ConnectionStyle c;
    if (name == "arc3") {
        c.kind = Kind::Arc3;
        if (!p.check({"rad"}) || !p.number("rad", &c.rad)) return false;
    } else if (name == "angle3") {
        c.kind = Kind::Angle3, c.angleA = 90, c.angleB = 0;
        if (!p.check({"angleA", "angleB"}) || !p.number("angleA", &c.angleA) || !p.number("angleB", &c.angleB)) return false;
    } else if (name == "angle") {
        c.kind = Kind::Angle, c.angleA = 90, c.angleB = 0;
        if (!p.check({"angleA", "angleB", "rad"}) || !p.number("angleA", &c.angleA) || !p.number("angleB", &c.angleB) ||
            !p.number("rad", &c.rad))
            return false;
    } else if (name == "arc") {
        c.kind = Kind::Arc, c.angleA = 0, c.angleB = 0;
        if (!p.check({"angleA", "angleB", "armA", "armB", "rad"}) || !p.number("angleA", &c.angleA) ||
            !p.number("angleB", &c.angleB) || !p.number("rad", &c.rad))
            return false;
        p.optional("armA", &c.armA), p.optional("armB", &c.armB);
    } else {
        c.kind = Kind::Bar;
        double armA = 0., armB = 0.;
        if (!p.check({"armA", "armB", "fraction", "angle"}) || !p.number("armA", &armA) || !p.number("armB", &armB) ||
            !p.number("fraction", &c.fraction))
            return false;
        c.armA = armA, c.armB = armB;
        p.optional("angle", &c.angle);
    }
    *out = c;
    return true;
}

bool ConnectionStyle::connect(const Point& a, const Point& b, const rc::Params& rc, Path* out, std::string* error) const {
    const double x1 = a[0], y1 = a[1];
    double x2 = b[0], y2 = b[1];
    std::vector<double> v;
    std::vector<unsigned char> codes;
    const auto add = [&](double x, double y, unsigned char code) { v.push_back(x), v.push_back(y), codes.push_back(code); };
    switch (kind) {
    case Kind::Arc3: {
        const double x12 = (x1 + x2) / 2., y12 = (y1 + y2) / 2.;
        const double dx = x2 - x1, dy = y2 - y1;
        const double f = rad;
        const double cx = x12 + f * dy, cy = y12 - f * dx;
        add(x1, y1, MOVETO), add(cx, cy, CURVE3), add(x2, y2, CURVE3);
        break;
    }
    case Kind::Angle3:
    case Kind::Angle: {
        const double cosA = std::cos(radians(angleA)), sinA = std::sin(radians(angleA));
        const double cosB = std::cos(radians(angleB)), sinB = std::sin(radians(angleB));
        double cx = 0, cy = 0;
        if (!bezier::intersection(x1, y1, cosA, sinA, x2, y2, cosB, sinB, &cx, &cy))
            return *error = "Given lines do not intersect. Please verify that the angles are not equal or differ by 180 degrees.", false;
        add(x1, y1, MOVETO);
        if (kind == Kind::Angle3) {
            add(cx, cy, CURVE3), add(x2, y2, CURVE3);
            break;
        }
        if (rad == 0.) {
            add(cx, cy, LINETO);
        } else {
            const double dx1 = x1 - cx, dy1 = y1 - cy;
            const double d1 = std::hypot(dx1, dy1);
            const double f1 = rad / d1;
            const double dx2 = x2 - cx, dy2 = y2 - cy;
            const double d2 = std::hypot(dx2, dy2);
            const double f2 = rad / d2;
            add(cx + dx1 * f1, cy + dy1 * f1, LINETO), add(cx, cy, CURVE3), add(cx + dx2 * f2, cy + dy2 * f2, CURVE3);
        }
        add(x2, y2, LINETO);
        break;
    }
    case Kind::Arc: {
        add(x1, y1, MOVETO);
        std::vector<Point> rounded;
        const bool hasA = armA && *armA != 0, hasB = armB && *armB != 0;  // (truth values)
        if (hasA) {
            const double cosA = std::cos(radians(angleA)), sinA = std::sin(radians(angleA));
            double d = *armA - rad;
            rounded.push_back({x1 + d * cosA, y1 + d * sinA});
            d = *armA;
            rounded.push_back({x1 + d * cosA, y1 + d * sinA});
        }
        if (hasB) {
            const double cosB = std::cos(radians(angleB)), sinB = std::sin(radians(angleB));
            const double xArmB = x2 + *armB * cosB, yArmB = y2 + *armB * sinB;
            double xp, yp, dx, dy, dd;
            if (!rounded.empty()) {
                xp = rounded.back()[0], yp = rounded.back()[1];
                dx = xArmB - xp, dy = yArmB - yp;
                dd = std::pow(dx * dx + dy * dy, .5);
                rounded.push_back({xp + rad * dx / dd, yp + rad * dy / dd});
                add(rounded[0][0], rounded[0][1], LINETO), add(rounded[1][0], rounded[1][1], CURVE3),
                    add(rounded[2][0], rounded[2][1], CURVE3);
            } else {
                xp = x1, yp = y1;  // vertices[-1]
                dx = xArmB - xp, dy = yArmB - yp;
                dd = std::pow(dx * dx + dy * dy, .5);
            }
            const double d = dd - rad;
            rounded = {{xp + d * dx / dd, yp + d * dy / dd}, {xArmB, yArmB}};
        }
        if (!rounded.empty()) {
            const double xp = rounded.back()[0], yp = rounded.back()[1];
            const double dx = x2 - xp, dy = y2 - yp;
            const double dd = std::pow(dx * dx + dy * dy, .5);
            rounded.push_back({xp + rad * dx / dd, yp + rad * dy / dd});
            add(rounded[0][0], rounded[0][1], LINETO), add(rounded[1][0], rounded[1][1], CURVE3),
                add(rounded[2][0], rounded[2][1], CURVE3);
        }
        add(x2, y2, LINETO);
        break;
    }
    case Kind::Bar: {
        const double x20 = x2, y20 = y2;
        const double theta1 = std::atan2(y2 - y1, x2 - x1);
        double dx = x2 - x1, dy = y2 - y1;
        const double dd = std::pow(dx * dx + dy * dy, .5);
        double ddx = dx / dd, ddy = dy / dd;
        const double a0 = *armA;
        double b0 = *armB;
        if (angle) {
            const double theta0 = radians(*angle);
            const double dtheta = theta1 - theta0;
            const double dl = dd * std::sin(dtheta);
            const double dL = dd * std::cos(dtheta);
            x2 = x1 + dL * std::cos(theta0), y2 = y1 + dL * std::sin(theta0);
            b0 = b0 - dl;
            dx = x2 - x1, dy = y2 - y1;
            const double dd2 = std::pow(dx * dx + dy * dy, .5);
            ddx = dx / dd2, ddy = dy / dd2;
        }
        const double arm = b0 > a0 ? b0 : a0;  // max(armA, armB)
        const double f = fraction * dd + arm;
        add(x1, y1, MOVETO), add(x1 + f * ddy, y1 - f * ddx, LINETO), add(x2 + f * ddy, y2 - f * ddx, LINETO),
            add(x20, y20, LINETO);
        break;
    }
    }
    *out = Path::make(std::move(v), std::move(codes), rc);
    return true;
}

bool ConnectionStyle::operator()(const Point& a, const Point& b, double shrinkA, double shrinkB, const bezier::Inside& inA,
                                 const bezier::Inside& inB, const rc::Params& rc, Path* out, std::string* error) const {
    Path path;
    if (!connect(a, b, rc, &path, error)) return false;
    // _clip: from where it leaves the start, to where it enters the stop (a ValueError keeps the path)
    const auto clip = [&](const bezier::Inside& start, const bezier::Inside& stop) {
        Path in, outside;
        if (start && bezier::splitPathInout(path, start, rc, &in, &outside)) path = outside;
        if (stop && bezier::splitPathInout(path, stop, rc, &in, &outside)) path = in;
    };
    clip(inA, inB);
    const auto& v = *path.vertices;
    const std::size_t n = path.size();
    clip(shrinkA != 0 ? bezier::insideCircle(v[0], v[1], shrinkA) : bezier::Inside{},
         shrinkB != 0 ? bezier::insideCircle(v[2 * n - 2], v[2 * n - 1], shrinkB) : bezier::Inside{});
    *out = path;
    return true;
}

bool ArrowStyle::make(const std::string& stylename, const StyleArgs& kwargs, ArrowStyle* out, std::string* error) {
    std::string name;
    StyleArgs args;
    if (!parseStyle(stylename, &name, &args, error)) return false;
    for (const auto& [k, v] : kwargs) args[k] = v;
    static const std::map<std::string, std::string> qualnames = {
        // (the qualified name of the __init__ each style class has: the plain curves take _Curve's)
        {"-", "ArrowStyle.Curve"}, {"<-", "ArrowStyle._Curve"}, {"->", "ArrowStyle._Curve"}, {"<->", "ArrowStyle._Curve"},
        {"<|-", "ArrowStyle._Curve"}, {"-|>", "ArrowStyle._Curve"}, {"<|-|>", "ArrowStyle._Curve"},
        {"]-", "ArrowStyle.BracketA"}, {"-[", "ArrowStyle.BracketB"}, {"]-[", "ArrowStyle.BracketAB"},
        {"|-|", "ArrowStyle.BarAB"}, {"]->", "ArrowStyle.BracketCurve"}, {"<-[", "ArrowStyle.CurveBracket"},
        {"simple", "ArrowStyle.Simple"}, {"fancy", "ArrowStyle.Fancy"}, {"wedge", "ArrowStyle.Wedge"}};
    const auto it = qualnames.find(name);
    if (it == qualnames.end()) return *error = "Unknown style: " + svg::pythonStrRepr(stylename), false;
    const Params p{it->second, args, error};
    ArrowStyle a;
    a.arrow = name;
    const auto numbers = [&](std::initializer_list<std::pair<const char*, double*>> list) {
        for (const auto& [key, target] : list)
            if (!p.number(key, target)) return false;
        return true;
    };
    if (name == "simple" || name == "fancy") {
        a.kind = name == "simple" ? Kind::Simple : Kind::Fancy;
        a.headLength = name == "simple" ? .5 : .4, a.headWidth = name == "simple" ? .5 : .4, a.tailWidth = name == "simple" ? .2 : .4;
        if (!p.check({"head_length", "head_width", "tail_width"}) ||
            !numbers({{"head_length", &a.headLength}, {"head_width", &a.headWidth}, {"tail_width", &a.tailWidth}}))
            return false;
    } else if (name == "wedge") {
        a.kind = Kind::Wedge, a.tailWidth = .3, a.shrinkFactor = 0.5;
        if (!p.check({"tail_width", "shrink_factor"}) || !numbers({{"tail_width", &a.tailWidth}, {"shrink_factor", &a.shrinkFactor}}))
            return false;
    } else {
        a.kind = Kind::Curve;
        if (name == "-") {
            a.headLength = .2, a.headWidth = .1;
            if (!p.check({})) return false;
        } else if (name == "]-" || name == "]->") {
            if (name == "]->") a.angleA.reset();
            if (!p.check({"widthA", "lengthA", "angleA"}) || !numbers({{"widthA", &a.widthA}, {"lengthA", &a.lengthA}})) return false;
            p.optional("angleA", &a.angleA);
        } else if (name == "-[" || name == "<-[") {
            if (name == "<-[") a.angleB.reset();
            if (!p.check({"widthB", "lengthB", "angleB"}) || !numbers({{"widthB", &a.widthB}, {"lengthB", &a.lengthB}})) return false;
            p.optional("angleB", &a.angleB);
        } else if (name == "]-[") {
            if (!p.check({"widthA", "lengthA", "angleA", "widthB", "lengthB", "angleB"}) ||
                !numbers({{"widthA", &a.widthA}, {"lengthA", &a.lengthA}, {"widthB", &a.widthB}, {"lengthB", &a.lengthB}}))
                return false;
            p.optional("angleA", &a.angleA), p.optional("angleB", &a.angleB);
        } else if (name == "|-|") {
            a.lengthA = a.lengthB = 0;
            if (!p.check({"widthA", "angleA", "widthB", "angleB"}) || !numbers({{"widthA", &a.widthA}, {"widthB", &a.widthB}}))
                return false;
            p.optional("angleA", &a.angleA), p.optional("angleB", &a.angleB);
        } else {
            if (!p.check({"head_length", "head_width", "widthA", "widthB", "lengthA", "lengthB", "angleA", "angleB", "scaleA",
                          "scaleB"}) ||
                !numbers({{"head_length", &a.headLength}, {"head_width", &a.headWidth}, {"widthA", &a.widthA}, {"widthB", &a.widthB},
                          {"lengthA", &a.lengthA}, {"lengthB", &a.lengthB}}))
                return false;
            p.optional("angleA", &a.angleA), p.optional("angleB", &a.angleB);
            p.optional("scaleA", &a.scaleA), p.optional("scaleB", &a.scaleB);
        }
        // _Curve.__init__: the heads and brackets from the arrow's two sides
        const std::size_t dash = name.find('-');
        const std::string begin = name.substr(0, dash), end = name.substr(dash + 1);
        if (begin == "<") a.beginHead = true;
        else if (begin == "<|") a.beginHead = true, a.fillBegin = true;
        else if (begin == "]" || begin == "|") a.beginBracket = true;
        if (end == ">") a.endHead = true;
        else if (end == "|>") a.endHead = true, a.fillEnd = true;
        else if (end == "[" || end == "|") a.endBracket = true;
    }
    *out = a;
    return true;
}

namespace {

Path pathOf(const Points& points, std::vector<unsigned char> codes, const rc::Params& rc) {
    std::vector<double> v;
    for (const auto& p : points) v.push_back(p[0]), v.push_back(p[1]);
    return Path::make(std::move(v), std::move(codes), rc);
}

Points pointsOf(const Path& path) {
    Points out;
    for (std::size_t k = 0; k < path.size(); ++k) out.push_back({(*path.vertices)[2 * k], (*path.vertices)[2 * k + 1]});
    return out;
}

// _point_along_a_line(x0, y0, x1, y1, d): the point at d from (x0, y0) towards (x1, y1)
Point pointAlongALine(double x0, double y0, double x1, double y1, double d) {
    const double dx = x0 - x1, dy = y0 - y1;
    const double ff = d / std::pow(dx * dx + dy * dy, .5);
    return {x0 - ff * dx, y0 - ff * dy};
}

const char* const kNotQuadratic = "'path' is not a valid quadratic Bezier curve";

// ensure_quadratic_bezier: a MOVETO and one CURVE3 segment
bool quadratic(const Path& path, Points* out) {
    if (path.size() != 3 || !path.codes || (*path.codes)[0] != MOVETO || (*path.codes)[1] != CURVE3 || (*path.codes)[2] != CURVE3)
        return false;
    *out = pointsOf(path);
    return true;
}

}  // namespace

bool ArrowStyle::operator()(const Path& path, double mutationSize, double linewidth, double aspectRatio, const rc::Params& rc,
                            std::vector<Path>* paths, std::vector<bool>* fills, bool* list, std::string* error) const {
    // the path shrunk by the aspect ratio, made, and (when a list of paths) stretched back
    std::vector<double> v = *path.vertices;
    for (std::size_t k = 1; k < v.size(); k += 2) v[k] = v[k] / aspectRatio;
    const Path shrunk =
        Path::make(std::move(v), path.codes ? *path.codes : std::vector<unsigned char>{}, rc);
    if (!transmute(shrunk, mutationSize, linewidth, rc, paths, fills, error)) return false;
    *list = kind == Kind::Curve;
    if (*list)
        for (Path& p : *paths) {
            std::vector<double> w = *p.vertices;
            for (std::size_t k = 1; k < w.size(); k += 2) w[k] = w[k] * aspectRatio;
            p = Path::make(std::move(w), p.codes ? *p.codes : std::vector<unsigned char>{}, rc);
        }
    return true;
}

bool ArrowStyle::transmute(const Path& path, double mutationSize, double linewidth, const rc::Params& rc,
                           std::vector<Path>* paths, std::vector<bool>* fills, std::string* error) const {
    paths->clear(), fills->clear();
    if (kind == Kind::Curve) {
        double headDist = 0, cosT = 0, sinT = 0;
        if (beginHead || endHead) {
            const double hl = headLength * mutationSize, hw = headWidth * mutationSize;
            headDist = std::hypot(hl, hw);
            cosT = hl / headDist, sinT = hw / headDist;
        }
        const double sA = scaleA ? *scaleA : mutationSize, sB = scaleB ? *scaleB : mutationSize;
        const Points pts = pointsOf(path);
        // _get_arrow_wedge: the head's three points, and the line's end pulled back by half the line width
        const auto wedge = [&](double x0, double y0, double x1, double y1, Points* vertices, double* ddx, double* ddy) {
            double dx = x0 - x1, dy = y0 - y1;
            double cpDistance = std::hypot(dx, dy);
            const double padProjected = .5 * linewidth / sinT;
            if (cpDistance == 0) cpDistance = 1;
            *ddx = padProjected * dx / cpDistance;
            *ddy = padProjected * dy / cpDistance;
            dx = dx / cpDistance * headDist;
            dy = dy / cpDistance * headDist;
            const double dx1 = cosT * dx + sinT * dy, dy1 = -sinT * dx + cosT * dy;
            const double dx2 = cosT * dx - sinT * dy, dy2 = sinT * dx + cosT * dy;
            *vertices = {{x1 + *ddx + dx1, y1 + *ddy + dy1}, {x1 + *ddx, y1 + *ddy}, {x1 + *ddx + dx2, y1 + *ddy + dy2}};
        };
        // _get_bracket
        const auto bracket = [&](double x0, double y0, double x1, double y1, double width, double length,
                                 const std::optional<double>& angle) {
            const auto cs = bezier::cosSin(x1, y1, x0, y0);
            const auto np = bezier::normalPoints(x0, y0, cs[0], cs[1], width);
            const double dx = length * cs[0], dy = length * cs[1];
            Points vertices = {{np[0] + dx, np[1] + dy}, {np[0], np[1]}, {np[2], np[3]}, {np[2] + dx, np[3] + dy}};
            if (angle && *angle != 0) {  // Affine2D().rotate_deg_around(x0, y0, angle)
                transforms::Affine2D t;
                t.translate(-x0, -y0).rotateDeg(*angle).translate(x0, y0);
                const auto m = t.matrix();
                for (auto& p : vertices) p = transforms::apply(m, p[0], p[1]);
            }
            return pathOf(vertices, {MOVETO, LINETO, LINETO, LINETO}, rc);
        };
        const double x0 = pts[0][0], y0 = pts[0][1], x1 = pts[1][0], y1 = pts[1][1];
        const bool hasBeginArrow = beginHead && (x0 != x1 || y0 != y1);
        Points verticesA, verticesB;
        double ddxA = 0, ddyA = 0, ddxB = 0, ddyB = 0;
        if (hasBeginArrow) wedge(x1, y1, x0, y0, &verticesA, &ddxA, &ddyA);
        const std::size_t n = pts.size();
        const double x2 = pts[n - 2][0], y2 = pts[n - 2][1], x3 = pts[n - 1][0], y3 = pts[n - 1][1];
        const bool hasEndArrow = endHead && (x2 != x3 || y2 != y3);
        if (hasEndArrow) wedge(x2, y2, x3, y3, &verticesB, &ddxB, &ddyB);
        Points line{{x0 + ddxA, y0 + ddyA}};
        for (std::size_t k = 1; k + 1 < n; ++k) line.push_back(pts[k]);
        line.push_back({x3 + ddxB, y3 + ddyB});
        paths->push_back(pathOf(line, path.codes ? *path.codes : std::vector<unsigned char>{}, rc));
        fills->push_back(false);
        if (hasBeginArrow) {
            if (fillBegin) {
                Points closed = verticesA;
                closed.push_back({0, 0});
                paths->push_back(pathOf(closed, {MOVETO, LINETO, LINETO, CLOSEPOLY}, rc));
                fills->push_back(true);
            } else {
                paths->push_back(pathOf(verticesA, {MOVETO, LINETO, LINETO}, rc));
                fills->push_back(false);
            }
        } else if (beginBracket) {
            paths->push_back(bracket(pts[0][0], pts[0][1], pts[1][0], pts[1][1], widthA * sA, lengthA * sA, angleA));
            fills->push_back(false);
        }
        if (hasEndArrow) {
            if (fillEnd) {
                Points closed = verticesB;
                closed.push_back({0, 0});
                fills->push_back(true);
                paths->push_back(pathOf(closed, {MOVETO, LINETO, LINETO, CLOSEPOLY}, rc));
            } else {
                fills->push_back(false);
                paths->push_back(pathOf(verticesB, {MOVETO, LINETO, LINETO}, rc));
            }
        } else if (endBracket) {
            paths->push_back(bracket(pts[n - 1][0], pts[n - 1][1], pts[n - 2][0], pts[n - 2][1], widthB * sB, lengthB * sB, angleB));
            fills->push_back(false);
        }
        return true;
    }
    Points q;
    if (!quadratic(path, &q)) return *error = kNotQuadratic, false;
    double x0 = q[0][0], y0 = q[0][1];
    const double x1 = q[1][0], y1 = q[1][1], x2 = q[2][0], y2 = q[2][1];
    std::vector<std::pair<unsigned char, Point>> patch;
    if (kind == Kind::Simple) {
        const double hl = headLength * mutationSize;
        const auto inF = bezier::insideCircle(x2, y2, hl);
        Points arrowPath{{x0, y0}, {x1, y1}, {x2, y2}};
        Points arrowOut, arrowIn;
        bool hasOut = bezier::splitIntersecting(arrowPath, inF, &arrowOut, &arrowIn);
        if (!hasOut) {
            const Point p0 = pointAlongALine(x2, y2, x1, y1, hl);
            arrowIn = {p0, {0.5 * (p0[0] + x2), 0.5 * (p0[1] + y2)}, {x2, y2}};
        }
        const double hw = headWidth * mutationSize;
        const auto head = bezier::wedged(arrowIn, hw / 2., 1., .5);
        const Points& headLeft = head.first;
        const Points& headRight = head.second;
        if (hasOut) {
            const double tw = tailWidth * mutationSize;
            const auto tail = bezier::parallels(arrowOut, tw / 2.);
            const Points& tailLeft = tail.first;
            const Points& tailRight = tail.second;
            patch = {{MOVETO, tailRight[0]}, {CURVE3, tailRight[1]}, {CURVE3, tailRight[2]}, {LINETO, headRight[0]},
                     {CURVE3, headRight[1]}, {CURVE3, headRight[2]}, {CURVE3, headLeft[1]}, {CURVE3, headLeft[0]},
                     {LINETO, tailLeft[2]}, {CURVE3, tailLeft[1]}, {CURVE3, tailLeft[0]}, {LINETO, tailRight[0]},
                     {CLOSEPOLY, tailRight[0]}};
        } else {
            patch = {{MOVETO, headRight[0]}, {CURVE3, headRight[1]}, {CURVE3, headRight[2]},
                     {CURVE3, headLeft[1]},  {CURVE3, headLeft[0]},  {CLOSEPOLY, headLeft[0]}};
        }
    } else if (kind == Kind::Fancy) {
        const double hl = headLength * mutationSize;
        Points arrowPath{{x0, y0}, {x1, y1}, {x2, y2}};
        Points pathOut, pathIn, pathHead;
        if (!bezier::splitIntersecting(arrowPath, bezier::insideCircle(x2, y2, hl), &pathOut, &pathIn)) {
            const Point p0 = pointAlongALine(x2, y2, x1, y1, hl);
            x0 = p0[0], y0 = p0[1];
            arrowPath = {p0, {0.5 * (x0 + x2), 0.5 * (y0 + y2)}, {x2, y2}};
            pathHead = arrowPath;
        } else {
            pathHead = pathIn;
        }
        if (!bezier::splitIntersecting(arrowPath, bezier::insideCircle(x2, y2, hl * .8), &pathOut, &pathIn))
            return *error = "Both points are on the same side of the closed path", false;
        const Points pathTail = pathOut;
        const double hw = headWidth * mutationSize;
        const auto head = bezier::wedged(pathHead, hw / 2., 1., .6);
        const double tw = tailWidth * mutationSize;
        const auto tail = bezier::wedged(pathTail, tw * .5, 1., 0.6, 0.3);
        if (!bezier::splitIntersecting(arrowPath, bezier::insideCircle(x0, y0, tw * .3), &pathIn, &pathOut))
            return *error = "Both points are on the same side of the closed path", false;
        const Point tailStart = pathIn.back();
        const Points& headLeft = head.first;
        const Points& headRight = head.second;
        const Points& tailLeft = tail.first;
        const Points& tailRight = tail.second;
        patch = {{MOVETO, tailStart},     {LINETO, tailRight[0]}, {CURVE3, tailRight[1]}, {CURVE3, tailRight[2]},
                 {LINETO, headRight[0]},  {CURVE3, headRight[1]}, {CURVE3, headRight[2]}, {CURVE3, headLeft[1]},
                 {CURVE3, headLeft[0]},   {LINETO, tailLeft[2]},  {CURVE3, tailLeft[1]},  {CURVE3, tailLeft[0]},
                 {LINETO, tailStart},     {CLOSEPOLY, tailStart}};
    } else {  // Wedge
        const Points arrowPath{{x0, y0}, {x1, y1}, {x2, y2}};
        const auto w = bezier::wedged(arrowPath, tailWidth * mutationSize / 2., 1., shrinkFactor);
        const Points& bPlus = w.first;
        const Points& bMinus = w.second;
        patch = {{MOVETO, bPlus[0]},  {CURVE3, bPlus[1]},  {CURVE3, bPlus[2]}, {LINETO, bMinus[2]},
                 {CURVE3, bMinus[1]}, {CURVE3, bMinus[0]}, {CLOSEPOLY, bMinus[0]}};
    }
    Points vertices;
    std::vector<unsigned char> codes;
    for (const auto& [c, p] : patch) codes.push_back(c), vertices.push_back(p);
    paths->push_back(pathOf(vertices, codes, rc));
    fills->push_back(true);
    return true;
}

Path compoundPath(const std::vector<Path>& paths, const rc::Params& rc) {
    std::vector<double> v;
    std::vector<unsigned char> codes;
    for (const Path& p : paths)
        for (std::size_t k = 0; k < p.size(); ++k) {
            const unsigned char c = p.codes ? (*p.codes)[k] : (k == 0 ? (unsigned char)MOVETO : (unsigned char)LINETO);
            if (c == STOP) continue;
            v.push_back((*p.vertices)[2 * k]), v.push_back((*p.vertices)[2 * k + 1]);
            codes.push_back(c);
        }
    return Path::make(std::move(v), std::move(codes), rc);
}

FancyArrowPatch::FancyArrowPatch(const rc::Params& rc, bezier::Point posA, bezier::Point posB)
    : Patch(rc), posA_(posA), posB_(posB) {
    edgeDefault_ = true;  // (an edge without force_edgecolor)
    setEdgecolor(std::nullopt);
    joinstyle = "round";  // kwargs.setdefault('joinstyle', JoinStyle.round) and capstyle
    capstyle = "round";
    ArrowStyle::make("simple", {}, &arrowStyle, nullptr);
}

bool FancyArrowPatch::displayPaths(std::vector<Path>* paths, std::vector<bool>* fills, bool* list, std::string* error) const {
    // posA and posB through the transform, connected, then made into the arrow
    const auto m = fullTransform();
    const auto a = m->transform(posA_[0], posA_[1]), b = m->transform(posB_[0], posB_[1]);
    Path connection;
    if (!connectionStyle(a, b, shrinkA * dpiCor_, shrinkB * dpiCor_, patchA, patchB, rc_, &connection, error)) return false;
    return arrowStyle(connection, mutationScale * dpiCor_, linewidth() * dpiCor_, mutationAspect, rc_, paths, fills, list, error);
}

Path FancyArrowPatch::path() const {
    std::vector<Path> paths;
    std::vector<bool> fills;
    bool list = false;
    std::string error;
    if (!displayPaths(&paths, &fills, &list, &error)) {
        support::noteRaised(error);
        return Path::make({}, {}, rc_);
    }
    // get_path: the display path (compound when a list) back through the transform (the identity here)
    return list ? compoundPath(paths, rc_) : paths[0];
}

void FancyArrowPatch::draw(Renderer& renderer) {
    if (!visible) return;
    dpiCor_ = renderer.pointsToPixels(1.);
    std::vector<Path> paths;
    std::vector<bool> fills;
    bool list = false;
    std::string error;
    if (!displayPaths(&paths, &fills, &list, &error)) {
        support::noteRaised(error);
        return;
    }
    std::vector<DrawPathArgs> args;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        std::optional<std::vector<double>> face;
        if (fills[i] && facecolorRgba()[3] != 0) face = std::vector<double>(facecolorRgba().begin(), facecolorRgba().end());
        args.push_back({paths[i], transforms::Matrix{}, face});
    }
    drawPaths(renderer, args);
}

}  // namespace calculate_plot::engine
