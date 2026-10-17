// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py, lib/matplotlib/axes/_base.py and
// lib/matplotlib/backends/backend_qt.py (see navigation.hpp).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "engine/navigation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace calculate_plot::engine {

namespace {

using Points = std::array<double, 4>;  // x0, y0, x1, y1

// Bbox.transformed: the corners ll, ul and lr through the transform, the box of ll and (lr.x, ul.y)
Points transformedBox(const transforms::Transform& t, const Points& p) {
    const auto ll = t.transform(p[0], p[1]);
    const auto ul = t.transform(p[0], p[3]);
    const auto lr = t.transform(p[2], p[1]);
    return {ll[0], ll[1], lr[0], ul[1]};
}

// _get_pan_points' format_deltas
void formatDeltas(const std::string& key, double& dx, double& dy) {
    if (key == "control") {
        if (std::fabs(dx) > std::fabs(dy)) dy = dx;
        else dx = dy;
    } else if (key == "x") {
        dy = 0;
    } else if (key == "y") {
        dx = 0;
    } else if (key == "shift") {
        if (2 * std::fabs(dx) < std::fabs(dy)) dx = 0;
        else if (2 * std::fabs(dy) < std::fabs(dx)) dy = 0;
        else if (std::fabs(dx) > std::fabs(dy)) dy = dy / std::fabs(dy) * std::fabs(dx);
        else dx = dx / std::fabs(dx) * std::fabs(dy);
    }
}

// _start_event_axes_interaction: the Axes under the event that navigate (in_axes: the patch contains the point),
// by zorder from the top, each with its twins, down to the first that captures events (its patch is visible:
// forward_navigation_events 'auto')
std::vector<Axes*> axesToTrigger(Figure& figure, double x, double y) {
    std::vector<Axes*> axes;
    for (auto& ax : figure.axes)
        if (ax->patch->containsPoint(x, y)) axes.push_back(ax.get());
    std::map<double, std::vector<Axes*>, std::greater<double>> groups;
    for (auto it = axes.rbegin(); it != axes.rend(); ++it) groups[(*it)->zorder].push_back(*it);
    std::vector<Axes*> out;
    bool captured = false;
    for (auto& [zorder, members] : groups) {
        for (Axes* ax : members) {
            out.push_back(ax);
            if (ax->twinned) out.insert(out.end(), ax->twinned->begin(), ax->twinned->end());
            else out.push_back(ax);
            if (ax->patch->visible) {
                captured = true;
                break;
            }
        }
        if (captured) break;
    }
    std::vector<Axes*> unique;  // dict.fromkeys: the first of each, in order
    for (Axes* ax : out)
        if (std::find(unique.begin(), unique.end(), ax) == unique.end()) unique.push_back(ax);
    return unique;
}

bool joined(const Axes& a, const Axes& b, bool x) {
    const auto& members = a.shared(x).members;
    return std::find(members.begin(), members.end(), &b) != members.end();
}

// get_xbound / get_ybound: the view interval in increasing order
std::array<double, 2> bound(const Axis& axis) {
    const auto v = axis.viewInterval();
    return v[0] < v[1] ? v : std::array<double, 2>{v[1], v[0]};
}

// sorted([a, b]) (Python: b goes first only when b < a)
std::array<double, 2> sorted2(double a, double b) { return b < a ? std::array<double, 2>{b, a} : std::array<double, 2>{a, b}; }

// np.clip(v, lo, hi): NaN stays NaN
double clip(double v, double lo, double hi) {
    if (std::isnan(v)) return v;
    return std::min(std::max(v, lo), hi);
}

// The scale's transform of one direction applied to values of that direction (and its inverse)
double scaled(const Axis& axis, bool x, double v, bool inverse) {
    auto identity = std::make_shared<transforms::IdentityTransform>();
    auto t = x ? transforms::blend(axis.scaleTransform(), identity) : transforms::blend(identity, axis.scaleTransform());
    if (inverse) t = t->inverted();
    const auto p = x ? t->transform(v, 0.0) : t->transform(0.0, v);
    return x ? p[0] : p[1];
}

std::vector<LimitChange> pan(const std::vector<Axes*>& axes, double ex, double ey, double tx, double ty,
                             const NavigationGesture& g) {
    std::vector<LimitChange> out;
    for (Axes* ax : axes) {
        // start_pan: the view, the transforms and the box as they are at the press
        const Points lim = ax->viewLim->points();
        const auto trans = ax->transData;
        const auto inverse = ax->transData->inverted();
        const Points bbox = ax->bbox->points();
        double dx = tx - ex, dy = ty - ey;
        if (dx == 0 && dy == 0) continue;
        Points result;
        if (g.button == 1) {
            formatDeltas(g.key, dx, dy);
            result = transformedBox(*inverse, {bbox[0] - dx, bbox[1] - dy, bbox[2] - dx, bbox[3] - dy});
        } else {
            dx = -dx / (bbox[2] - bbox[0]);
            dy = -dy / (bbox[3] - bbox[1]);
            formatDeltas(g.key, dx, dy);
            if (ax->aspect) dx = dy = 0.5 * (dx + dy);
            const double ax_ = std::pow(10.0, dx), ay = std::pow(10.0, dy);
            const Points old = transformedBox(*trans, lim);
            const Points moved = {ex + ax_ * (old[0] - ex), ey + ay * (old[1] - ey), ex + ax_ * (old[2] - ex), ey + ay * (old[3] - ey)};
            result = transformedBox(*inverse, moved);
        }
        // the limits whose pixels are not finite are None (an underflow on a log scale, say)
        const Points back = transformedBox(*trans, result);
        const auto valid = [&](int i) { return std::isfinite(back[std::size_t(i)]); };
        const auto value = [&](int i) { return valid(i) ? std::optional<double>(result[std::size_t(i)]) : std::nullopt; };
        out.push_back({ax, true, value(0), value(2)});
        out.push_back({ax, false, value(1), value(3)});
    }
    return out;
}

std::vector<LimitChange> zoom(const std::vector<Axes*>& axes, double sx, double sy, double ex, double ey, const NavigationGesture& g) {
    std::vector<LimitChange> out;
    const bool in = g.button == 1;
    const std::string& mode = g.key;
    for (std::size_t i = 0; i < axes.size(); ++i) {
        Axes* ax = axes[i];
        bool twinx = false, twiny = false;
        for (std::size_t k = 0; k < i; ++k) {
            twinx = twinx || joined(*ax, *axes[k], true);
            twiny = twiny || joined(*ax, *axes[k], false);
        }
        // _prepare_view_from_bbox((start_x, start_y, event.x, event.y), direction, key, twinx, twiny)
        const auto [xmin0, xmax0] = bound(*ax->xaxis);
        const auto [ymin0, ymax0] = bound(*ax->yaxis);
        const auto inverse = ax->transData->inverted();
        const auto start = inverse->transform(sx, sy);
        const auto stop = inverse->transform(ex, ey);
        auto xs = sorted2(start[0], stop[0]);
        auto ys = sorted2(start[1], stop[1]);
        double xmin = clip(xs[0], xmin0, xmax0), xmax = clip(xs[1], xmin0, xmax0);
        double ymin = clip(ys[0], ymin0, ymax0), ymax = clip(ys[1], ymin0, ymax0);
        if (twinx || mode == "y") xmin = xmin0, xmax = xmax0;
        if (twiny || mode == "x") ymin = ymin0, ymax = ymax0;
        std::array<double, 2> nx{xmin, xmax}, ny{ymin, ymax};
        if (!in) {
            const auto unzoom = [](const Axis& axis, bool x, double min0, double max0, double min, double max) {
                const double smin0 = scaled(axis, x, min0, false), smax0 = scaled(axis, x, max0, false);
                const double smin = scaled(axis, x, min, false), smax = scaled(axis, x, max, false);
                const double factor = (smax0 - smin0) / (smax - smin);
                const double smin1 = smin0 - factor * (smin - smin0);
                const double smax1 = smax0 + factor * (smax0 - smax);
                return std::array<double, 2>{scaled(axis, x, smin1, true), scaled(axis, x, smax1, true)};
            };
            nx = unzoom(*ax->xaxis, true, xmin0, xmax0, xmin, xmax);
            ny = unzoom(*ax->yaxis, false, ymin0, ymax0, ymin, ymax);
        }
        // set_xbound(lower, upper): set_xlim(sorted((lower, upper), reverse=xaxis_inverted()))
        const auto setBound = [&out, ax](bool x, std::array<double, 2> b) {
            const auto view = (x ? ax->xaxis : ax->yaxis)->viewInterval();
            auto s = sorted2(b[0], b[1]);
            if (view[1] < view[0]) std::swap(s[0], s[1]);
            out.push_back({ax, x, s[0], s[1]});
        };
        if (!twinx && mode != "y") setBound(true, nx);
        if (!twiny && mode != "x") setBound(false, ny);
    }
    return out;
}

}  // namespace

namespace {

// The event's position in device pixels from the bottom-left as mouseEventCoords gives it (LocationEvent finds
// inaxes and xdata, ydata from it before it keeps int() of it as x, y)
std::array<double, 2> eventPosition(const Figure& figure, double ratio, double x, double y) {
    const auto p = figure.bbox->points();
    const double height = double(static_cast<long long>(std::max(p[1], p[3]) / ratio));
    return {x * ratio, (height - y) * ratio};
}

}  // namespace

std::string pointerText(Figure& figure, double ratio, double x, double y) {
    const auto e = eventPosition(figure, ratio, x, y);
    // FigureCanvasBase.inaxes: of the visible Axes whose patch holds the point, the topmost (the last of the
    // highest zorder)
    Axes* inaxes = nullptr;
    for (auto& ax : figure.axes)
        if (ax->visible && ax->patch->containsPoint(e[0], e[1]) && (!inaxes || ax->zorder >= inaxes->zorder)) inaxes = ax.get();
    if (!inaxes) return std::string();
    const auto data = inaxes->transData->inverted()->transform(e[0], e[1]);
    // format_coord(xdata, ydata): of twins, each one's coordinates of the same pixel, by zorder
    std::vector<Axes*> twins = inaxes->twinned ? *inaxes->twinned : std::vector<Axes*>{inaxes};
    std::string s = "(x, y) = ";
    const auto pair = [](Axes& ax, double vx, double vy) -> std::optional<std::string> {
        const auto fx = ax.xaxis->formatDataShort(vx);
        const auto fy = ax.yaxis->formatDataShort(vy);
        if (!fx || !fy) return std::nullopt;
        return "(" + *fx + ", " + *fy + ")";
    };
    if (twins.size() == 1) {
        const auto p = pair(*inaxes, data[0], data[1]);
        if (!p) return std::string();
        s += *p;
    } else {
        const auto screen = inaxes->transData->transform(data[0], data[1]);
        std::stable_sort(twins.begin(), twins.end(), [](const Axes* a, const Axes* b) { return a->zorder < b->zorder; });
        for (std::size_t i = 0; i < twins.size(); ++i) {
            const auto d = twins[i]->transData->inverted()->transform(screen[0], screen[1]);
            const auto p = pair(*twins[i], d[0], d[1]);
            if (!p) return std::string();
            s += (i ? " | " : "") + *p;
        }
    }
    s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);  // rstrip
    return s;
}

std::vector<LimitChange> navigate(Figure& figure, double ratio, const NavigationGesture& g) {
    if (g.button != 1 && g.button != 3) return {};
    // FigureCanvasQT.mouseEventCoords: y from the bottom of get_width_height() (logical: int(bbox.max / ratio)),
    // then LocationEvent
    const auto p = figure.bbox->points();
    const double height = double(static_cast<long long>(std::max(p[1], p[3]) / ratio));
    // LocationEvent keeps int(x) and int(y): whole device pixels, truncated
    const auto whole = [](double v) { return std::trunc(v); };
    const double ex = whole(g.fromX * ratio), ey = whole((height - g.fromY) * ratio);
    const double tx = whole(g.toX * ratio), ty = whole((height - g.toY) * ratio);
    const std::vector<Axes*> axes = axesToTrigger(figure, ex, ey);
    if (axes.empty()) return {};
    if (!g.zoom) return pan(axes, ex, ey, tx, ty, g);
    // release_zoom: a drag of less than 5 pixels (in a direction that counts) cancels
    if ((std::fabs(tx - ex) < 5 && g.key != "y") || (std::fabs(ty - ey) < 5 && g.key != "x")) return {};
    return zoom(axes, ex, ey, tx, ty, g);
}

}  // namespace calculate_plot::engine
