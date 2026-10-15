// Derived from Matplotlib 3.11.2: lib/matplotlib/hatch.py (get_path and the hatch pattern classes).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "engine/hatch.hpp"

#include "engine/lines.hpp"

#include <algorithm>

namespace calculate_plot::engine {

namespace {

int count(const std::string& s, char c) { return int(std::count(s.begin(), s.end(), c)); }

// np.linspace(start, stop, num, endpoint): arange(num) * step + start, the end itself when it is included
std::vector<double> linspace(double start, double stop, int num, bool endpoint) {
    std::vector<double> y;
    const int div = endpoint ? num - 1 : num;
    const double step = div > 0 ? (stop - start) / div : 0;
    for (int i = 0; i < num; ++i) y.push_back(div > 0 ? double(i) * step + start : start);
    if (endpoint && num > 1) y.back() = stop;
    return y;
}

struct Pattern {
    std::vector<double> vertices;  // x0, y0, x1, y1 ...
    std::vector<unsigned char> codes;
};

// HorizontalHatch ('-', '+') and VerticalHatch ('|', '+'): lines across the square, half a step in
Pattern straightLines(int lines, bool horizontal) {
    Pattern p;
    std::vector<double> steps = linspace(0.0, 1.0, lines, false);
    const double stepsize = lines > 0 ? 1.0 / lines : 0;
    for (double& s : steps) s += stepsize / 2.;
    for (double s : steps) {
        if (horizontal) p.vertices.insert(p.vertices.end(), {0.0, s, 1.0, s});
        else p.vertices.insert(p.vertices.end(), {s, 0.0, s, 1.0});
        p.codes.insert(p.codes.end(), {MOVETO, LINETO});
    }
    return p;
}

// NorthEastHatch ('/', 'x', 'X') and SouthEastHatch ('\', 'x', 'X'): diagonals
Pattern diagonals(int lines, bool northEast) {
    Pattern p;
    if (!lines) return p;
    for (double s : linspace(-0.5, 0.5, lines + 1, true)) {
        if (northEast) p.vertices.insert(p.vertices.end(), {0.0 + s, 0.0 - s, 1.0 + s, 1.0 - s});
        else p.vertices.insert(p.vertices.end(), {0.0 + s, 1.0 + s, 1.0 + s, 0.0 + s});
        p.codes.insert(p.codes.end(), {MOVETO, LINETO});
    }
    return p;
}

// Shapes: a shape at each point of rows that alternate between num_rows + 1 and num_rows columns;
// an unfilled one is drawn forward, then backward at 0.9 of its size
Pattern shapes(int rows, const Path& shape, std::vector<unsigned char> shapeCodes, double size, bool filled) {
    Pattern p;
    if (rows == 0) return p;
    const double offset = 1.0 / rows;
    std::vector<double> v = *shape.vertices;
    for (double& x : v) x = x * offset * size;
    std::vector<unsigned char> codes = shapeCodes;
    if (!filled) {
        const std::size_t n = v.size() / 2;
        for (std::size_t k = n; k-- > 0;) v.push_back(v[2 * k] * 0.9), v.push_back(v[2 * k + 1] * 0.9);
        codes.insert(codes.end(), shapeCodes.begin(), shapeCodes.end());
    }
    for (int row = 0; row < rows + 1; ++row) {
        const std::vector<double> cols =
            row % 2 == 0 ? linspace(0, 1, rows + 1, true) : linspace(offset / 2, 1 - offset / 2, rows, true);
        const double rowPos = row * offset;
        for (double col : cols) {
            for (std::size_t k = 0; k + 1 < v.size(); k += 2) p.vertices.push_back(v[k] + col), p.vertices.push_back(v[k + 1] + rowPos);
            p.codes.insert(p.codes.end(), codes.begin(), codes.end());
        }
    }
    return p;
}

}  // namespace

std::optional<Path> hatchPath(const std::string& hatch, const rc::Params& rc, int density) {
    for (char shape : {'o', 'O', '.', '*'})
        if ((count(hatch, shape) * density) % 2 != 0) return std::nullopt;  // "Output array is the wrong shape"
    const Path& circle = unitCirclePath();
    const std::vector<unsigned char> circleCodes = *circle.codes;
    const Path& star = unitStarPath();
    std::vector<unsigned char> starCodes(star.size(), LINETO);
    starCodes.front() = MOVETO;
    starCodes.back() = CLOSEPOLY;
    const Pattern patterns[] = {
        straightLines(int((count(hatch, '-') + count(hatch, '+')) * density), true),
        straightLines(int((count(hatch, '|') + count(hatch, '+')) * density), false),
        diagonals(int((count(hatch, '/') + count(hatch, 'x') + count(hatch, 'X')) * density), true),
        diagonals(int((count(hatch, '\\') + count(hatch, 'x') + count(hatch, 'X')) * density), false),
        shapes(count(hatch, 'o') * density, circle, circleCodes, 0.2, false),     // SmallCircles
        shapes(count(hatch, 'O') * density, circle, circleCodes, 0.35, false),    // LargeCircles
        shapes(count(hatch, '.') * density, circle, circleCodes, 0.1, true),      // SmallFilledCircles
        shapes(count(hatch, '*') * density, star, starCodes, 1.0 / 3.0, true)};   // Stars
    std::vector<double> vertices;
    std::vector<unsigned char> codes;
    for (const Pattern& p : patterns) {
        vertices.insert(vertices.end(), p.vertices.begin(), p.vertices.end());
        codes.insert(codes.end(), p.codes.begin(), p.codes.end());
    }
    if (vertices.empty()) return Path::make({}, {}, rc);  // Path(np.empty((0, 2)))
    return Path::make(std::move(vertices), std::move(codes), rc);
}

}  // namespace calculate_plot::engine
