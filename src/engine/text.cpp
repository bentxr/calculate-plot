// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (Text: layout, drawing, window extent).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/text.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"
#include "support/pynum.hpp"
#include "text/text_render.h"

#include <cmath>
#include <utility>

namespace calculate_plot::engine {

namespace {

constexpr double kPi = 3.141592653589793;

// Python's max(a, b) and min(a, b): the first argument unless the second is strictly beyond it.
double pyMax(double a, double b) { return b > a ? b : a; }
double pyMin(double a, double b) { return b < a ? b : a; }

// _rotate_point
std::pair<double, double> rotatePoint(double angle, double x, double y) {
    if (angle == 0) return {x, y};
    const double rad = angle * (kPi / 180.0);  // math.radians
    const double c = std::cos(rad), s = std::sin(rad);
    return {c * x - s * y, s * x + c * y};
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = s.find('\n', start);
        lines.push_back(s.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) return lines;
        start = end + 1;
    }
}

// cbook.is_math_text: an even, non-zero number of unescaped dollar signs.
bool isMathText(const std::string& s) {
    long dollars = 0, escaped = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '$') ++dollars;
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '$') ++escaped;
    }
    const long count = dollars - escaped;
    return count > 0 && count % 2 == 0;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
    return s;
}

}  // namespace

Text::Text(const rc::Params& rc, double x0, double y0, std::string s)
    : Artist(rc),
      x(x0),
      y(y0),
      text(std::move(s)),
      font(text::FontProperties::defaults(rc)),
      antialiased(rc.boolean("text.antialiased")),
      parseMath(rc.boolean("text.parse_math")) {
    zorder = 3;
    setColor(rc.text("text.color"));
}

void Text::setRotation(double degrees) { rotation_ = support::divmod(degrees, 360.0).second; }

void Text::setRotation(const std::string& name) { rotation_ = name == "vertical" ? 90.0 : 0.0; }

void Text::setColor(const std::string& c) { color = *colors::toRgba(c, std::nullopt, rc_, nullptr); }

Text::Layout Text::layout(Renderer& renderer) const {
    // _preprocess_math
    const auto preprocess = [this](const std::string& line) -> std::pair<std::string, bool> {
        if (!parseMath) return {line, false};
        if (isMathText(line)) return {line, true};
        return {replaceAll(line, "\\$", "$"), false};
    };
    double thisx = 0.0, thisy = 0.0;
    const std::vector<std::string> lines = splitLines(text);
    struct Wad {
        double w, a, d;
    };
    std::vector<Wad> wads;
    std::vector<double> xs, ys;
    const double dpi = figure ? figure->dpi : renderer.dpi();
    const FontSpec spec{text::findFontsByProps(font, rc_), font.size, font};

    // The font's own line metrics (OS/2, else hhea), scaled to the size at the figure's dpi.
    double minAscent = 0, minDescent = 0, lineGap = 0;
    std::string error;
    const auto metrics = text::fontHeightMetrics(renderer.fontCache(), spec.files, font.size, dpi, error);
    if (metrics) {
        lineGap = metrics->lineGap;
        minAscent = metrics->ascent;
        minDescent = metrics->descent;
    } else {
        const TextMetrics lp = renderer.textWidthHeightDescent("lp", spec, false);
        minDescent = lp.descent;
        minAscent = lp.height - lp.descent;
        lineGap = 0;
    }
    if (lines.size() == 1) lineGap = 0;

    double baseline = 0, d = 0;
    for (const std::string& line : lines) {
        const auto [clean, ismath] = preprocess(line);
        double w = 0, h = 0;
        d = 0;
        if (!clean.empty()) {
            const TextMetrics m = renderer.textWidthHeightDescent(clean, spec, ismath);
            w = m.width, h = m.height, d = m.descent;
        }
        double a = h - d;
        if (!linespacing) {
            a = pyMax(a, minAscent) + lineGap / 2;
            d = pyMax(d, minDescent) + lineGap / 2;
        } else {
            const double lineHeight = *linespacing * (minAscent + minDescent);
            const double leading = lineHeight - (a + d);
            a += leading / 2;
            d += leading / 2;
        }
        baseline = a - thisy;
        thisy -= a;
        wads.push_back({w, a, d});
        xs.push_back(thisx);
        ys.push_back(thisy);
        thisy -= d;
    }
    const double descent = d;

    double width = wads[0].w;
    for (const Wad& wad : wads) width = pyMax(width, wad.w);
    double xmin = 0, xmax = width, ymax = 0;
    double ymin = ys.back() - descent;

    const std::string malign = multialignment ? *multialignment : horizontalAlignment;
    std::vector<std::pair<double, double>> offsetLayout;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (malign == "center") offsetLayout.push_back({xs[i] + width / 2 - wads[i].w / 2, ys[i]});
        else if (malign == "right") offsetLayout.push_back({xs[i] + width - wads[i].w, ys[i]});
        else offsetLayout.push_back({xs[i], ys[i]});
    }

    const std::pair<double, double> cornersHoriz[4] = {{xmin, ymin}, {xmin, ymax}, {xmax, ymax}, {xmax, ymin}};
    const double sizeW = xmax - xmin, sizeH = ymax - ymin;
    const double angle = rotation();
    std::pair<double, double> cornersRotated[4];
    for (int i = 0; i < 4; ++i) cornersRotated[i] = rotatePoint(angle, cornersHoriz[i].first, cornersHoriz[i].second);
    xmin = xmax = cornersRotated[0].first;
    ymin = ymax = cornersRotated[0].second;
    for (int i = 1; i < 4; ++i) {
        xmin = pyMin(xmin, cornersRotated[i].first), xmax = pyMax(xmax, cornersRotated[i].first);
        ymin = pyMin(ymin, cornersRotated[i].second), ymax = pyMax(ymax, cornersRotated[i].second);
    }
    const double widthRot = xmax - xmin, heightRot = ymax - ymin;

    std::string halign = horizontalAlignment, valign = verticalAlignment;
    double offsetx, offsety;
    if (rotationMode != "anchor") {
        if (rotationMode == "xtick") {  // _ha_for_angle
            const bool anchorAtBottom = verticalAlignment == "bottom";
            if (angle <= 10 || (85 <= angle && angle <= 95) || 350 <= angle || (170 <= angle && angle <= 190) ||
                (265 <= angle && angle <= 275))
                halign = "center";
            else if ((10 < angle && angle < 85) || (190 < angle && angle < 265))
                halign = anchorAtBottom ? "left" : "right";
            else
                halign = anchorAtBottom ? "right" : "left";
        } else if (rotationMode == "ytick") {  // _va_for_angle
            const bool anchorAtLeft = horizontalAlignment == "left";
            if (angle <= 10 || 350 <= angle || (170 <= angle && angle <= 190) || (80 <= angle && angle <= 100) ||
                (260 <= angle && angle <= 280))
                valign = "center";
            else if ((190 < angle && angle < 260) || (10 < angle && angle < 80))
                valign = anchorAtLeft ? "baseline" : "top";
            else
                valign = anchorAtLeft ? "top" : "baseline";
        }
        offsetx = halign == "left" ? xmin : halign == "right" ? xmax : (xmin + xmax) / 2;
        offsety = valign == "bottom"     ? ymin
                  : valign == "top"      ? ymax
                  : valign == "center"   ? (ymin + ymax) / 2
                  : valign == "baseline" ? ymin + descent
                                         : ymin + heightRot - baseline / 2;  // center_baseline
    } else {
        const double xmin1 = cornersHoriz[0].first, ymin1 = cornersHoriz[0].second;
        const double xmax1 = cornersHoriz[2].first, ymax1 = cornersHoriz[2].second;
        offsetx = halign == "left" ? xmin1 : halign == "right" ? xmax1 : (xmin1 + xmax1) / 2;
        offsety = valign == "bottom"     ? ymin1
                  : valign == "top"      ? ymax1
                  : valign == "center"   ? (ymin1 + ymax1) / 2
                  : valign == "baseline" ? ymax1 - baseline
                                         : ymax1 - baseline / 2;  // center_baseline
        std::tie(offsetx, offsety) = rotatePoint(angle, offsetx, offsety);
    }
    xmin -= offsetx;
    ymin -= offsety;

    Layout out;
    out.bbox = *transforms::Bbox::fromBounds(xmin, ymin, widthRot, heightRot);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto [rx, ry] = rotatePoint(angle, offsetLayout[i].first, offsetLayout[i].second);
        out.lines.push_back(Line{lines[i], wads[i].w, wads[i].a, wads[i].d, rx - offsetx, ry - offsety});
    }
    out.cornerX = cornersRotated[0].first - offsetx;
    out.cornerY = cornersRotated[0].second - offsety;
    out.boxWidth = sizeW;
    out.boxHeight = sizeH;
    return out;
}

transforms::Bbox Text::windowExtent(Renderer& renderer) const {
    if (!visible) return transforms::Bbox({0, 0, 1, 1});  // Bbox.unit()
    const auto [tx, ty] = transform->transform(x, y);
    if (text.empty()) return transforms::Bbox({tx, ty, tx + 0.0, ty + 0.0});
    const auto b = layout(renderer).bbox.points();
    // Bbox.translated
    return transforms::Bbox({b[0] + tx, b[1] + ty, b[2] + tx, b[3] + ty});
}

void Text::draw(Renderer& renderer) {
    if (!visible || text.empty()) return;
    renderer.openGroup("text", gid);
    const Layout l = layout(renderer);
    const auto [posx, posy] = transform->transform(x, y);
    // (matplotlib returns here with the group it has opened left open: in an SVG it closes with
    // the enclosing one)
    if (!std::isfinite(posx) || !std::isfinite(posy)) return;
    const double canvash = renderer.height();
    GC gc = renderer.newGc();
    gc.setForeground(std::vector<double>(color.begin(), color.end()));
    gc.setAlpha(alpha);
    gc.url = url;
    gc.antialiased = antialiased;
    gc.snap = snap;
    setGcClip(gc);
    const double angle = rotation();
    const FontSpec spec{text::findFontsByProps(font, rc_), font.size, font};
    for (const Line& line : l.lines) {
        const bool single = l.lines.size() == 1;  // mtext
        double lx = line.x + posx;
        double ly = line.y + posy;
        if (renderer.flipy()) ly = canvash - ly;
        std::string clean = line.text;
        bool ismath = false;
        if (parseMath) {
            if (isMathText(clean)) ismath = true;
            else clean = replaceAll(clean, "\\$", "$");
        }
        TextOptions options;
        if (single) {
            options.features = features;
            options.language = language;
        }
        renderer.drawText(gc, lx, ly, clean, spec, angle, ismath, options);
    }
    renderer.closeGroup("text");
}

}  // namespace calculate_plot::engine
