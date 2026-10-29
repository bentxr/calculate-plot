// Derived from Matplotlib 3.11.2: lib/matplotlib/streamplot.py (Grid, StreamMask, DomainMap, the integrator,
// interpgrid, _gen_starting_points).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/streamplot.hpp"

#include "support/pyformat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace calculate_plot::engine {

namespace {

using XY = std::array<double, 2>;

// How a step of the integration ends, as streamplot's exceptions do
enum class Stop { None, OutOfBounds, Terminate, InvalidIndex };

// Python's min(a, b): a unless b is smaller
double pyMin(double a, double b) { return b < a ? b : a; }

// StreamMask and DomainMap together: the cells streamlines have crossed, and the coordinate conversions
class Domain {
public:
    Domain(const StreamGrid& g, int maskNx, int maskNy)
        : grid(g), mnx(maskNx), mny(maskNy), mask(std::size_t(maskNx) * std::size_t(maskNy), 0) {
        xGrid2mask = double(mnx - 1) / double(grid.nx - 1);
        yGrid2mask = double(mny - 1) / double(grid.ny - 1);
        xMask2grid = 1. / xGrid2mask;
        yMask2grid = 1. / yGrid2mask;
        xData2grid = 1. / grid.dx;
        yData2grid = 1. / grid.dy;
    }
    bool withinGrid(double xi, double yi) const { return 0 <= xi && xi <= grid.nx - 1 && 0 <= yi && yi <= grid.ny - 1; }
    std::pair<int, int> grid2mask(double xi, double yi) const {
        return {int(std::nearbyint(xi * xGrid2mask)), int(std::nearbyint(yi * yGrid2mask))};  // round (half to even)
    }
    char& at(int xm, int ym) { return mask[std::size_t(ym) * std::size_t(mnx) + std::size_t(xm)]; }
    bool startTrajectory(double xg, double yg, bool broken) {
        traj_.clear();
        const auto m = grid2mask(xg, yg);
        return updateMask(m.first, m.second, broken);
    }
    void resetStartPoint(double xg, double yg) { current_ = grid2mask(xg, yg), hasCurrent_ = true; }
    Stop updateTrajectory(double xg, double yg, bool broken) {
        if (!withinGrid(xg, yg)) return Stop::InvalidIndex;
        const auto m = grid2mask(xg, yg);
        return updateMask(m.first, m.second, broken) ? Stop::None : Stop::InvalidIndex;
    }
    void undoTrajectory() {
        for (const auto& t : traj_) at(t.first, t.second) = 0;
    }

    const StreamGrid& grid;
    const int mnx, mny;
    std::vector<char> mask;
    double xGrid2mask, yGrid2mask, xMask2grid, yMask2grid, xData2grid, yData2grid;

private:
    // StreamMask._update_trajectory: false (InvalidIndexError) entering a filled cell, when streamlines break
    bool updateMask(int xm, int ym, bool broken) {
        if (hasCurrent_ && current_ == std::make_pair(xm, ym)) return true;
        if (at(xm, ym) == 0) {
            traj_.push_back({xm, ym});
            at(xm, ym) = 1;
            current_ = {xm, ym}, hasCurrent_ = true;
            return true;
        }
        return !broken;
    }
    std::vector<std::pair<int, int>> traj_;
    std::pair<int, int> current_{0, 0};
    bool hasCurrent_ = false;  // (_current_xy None at first; kept from one trajectory to the next)
};

}  // namespace

bool streamGrid(const std::vector<double>& x, const std::vector<double>& y, StreamGrid* g, std::string* error) {
    if (x.size() < 2 || y.size() < 2) return *error = "grids of fewer than 2 points a side are not supported yet", false;
    for (std::size_t i = 1; i < x.size(); ++i)
        if (!(x[i] - x[i - 1] > 0)) return *error = "'x' must be strictly increasing", false;
    for (std::size_t i = 1; i < y.size(); ++i)
        if (!(y[i] - y[i - 1] > 0)) return *error = "'y' must be strictly increasing", false;
    g->nx = int(x.size()), g->ny = int(y.size());
    g->dx = x[1] - x[0], g->dy = y[1] - y[0];
    g->xOrigin = x[0], g->yOrigin = y[0];
    g->width = x.back() - x[0], g->height = y.back() - y[0];
    // np.allclose(np.diff(x), width / (nx - 1))
    const auto equal = [](const std::vector<double>& v, double step) {
        for (std::size_t i = 1; i < v.size(); ++i)
            if (!(std::fabs((v[i] - v[i - 1]) - step) <= 1e-8 + 1e-5 * std::fabs(step))) return false;
        return true;
    };
    if (!equal(x, g->width / double(g->nx - 1))) return *error = "'x' values must be equally spaced", false;
    if (!equal(y, g->height / double(g->ny - 1))) return *error = "'y' values must be equally spaced", false;
    return true;
}

bool interpgrid(const std::vector<double>& a, int Nx, int Ny, double xi, double yi, double* out) {
    const int x = int(xi), y = int(yi);
    const int xn = x == Nx - 1 ? x : x + 1, yn = y == Ny - 1 ? y : y + 1;
    const auto at = [&](int r, int c) { return a[std::size_t(r) * std::size_t(Nx) + std::size_t(c)]; };
    const double a00 = at(y, x), a01 = at(y, xn), a10 = at(yn, x), a11 = at(yn, xn);
    if (std::isnan(a00) || std::isnan(a01) || std::isnan(a10) || std::isnan(a11)) return false;  // np.ma.is_masked
    const double xt = xi - x, yt = yi - y;
    const double a0 = a00 * (1 - xt) + a01 * xt;
    const double a1 = a10 * (1 - xt) + a11 * xt;
    *out = a0 * (1 - yt) + a1 * yt;
    return true;
}

bool streamTrajectories(const StreamGrid& grid, const std::vector<double>& uIn, const std::vector<double>& vIn,
                        const StreamOptions& o, std::vector<std::vector<XY>>* out, std::string* error) {
    // StreamMask(density): 30 cells per unit of density
    const int mnx = int(30 * o.densityX), mny = int(30 * o.densityY);
    if (mnx < 0 || mny < 0) return *error = "'density' must be positive", false;
    if (mnx < 2 || mny < 2) return *error = "densities below 2/30 are not supported yet", false;
    Domain dmap(grid, mnx, mny);
    const int nx = grid.nx, ny = grid.ny;
    // _get_integrator: the velocity in grid coordinates per unit of path length in axes coordinates
    std::vector<double> u(uIn.size()), v(vIn.size()), speed(uIn.size());
    for (std::size_t i = 0; i < uIn.size(); ++i) {
        u[i] = uIn[i] * dmap.xData2grid;
        v[i] = vIn[i] * dmap.yData2grid;
        const double uax = u[i] / double(nx - 1), vax = v[i] / double(ny - 1);
        speed[i] = std::sqrt(uax * uax + vax * vax);
    }
    const auto forwardTime = [&](double xi, double yi, double* dx, double* dy) {
        if (!dmap.withinGrid(xi, yi)) return Stop::OutOfBounds;
        double dsdt = 0, ui = 0, vi = 0;
        if (!interpgrid(speed, nx, ny, xi, yi, &dsdt) || dsdt == 0) return Stop::Terminate;
        const double dtds = 1. / dsdt;
        if (!interpgrid(u, nx, ny, xi, yi, &ui) || !interpgrid(v, nx, ny, xi, yi, &vi)) return Stop::Terminate;
        *dx = ui * dtds, *dy = vi * dtds;
        return Stop::None;
    };
    double maxlength = o.maxlength;
    if (o.direction == "both") maxlength /= 2.;
    bool raised = false;  // (TerminateTrajectory out of _euler_step goes through streamplot itself)
    // _integrate_rk12: Heun's method with an adaptive step, ended by the boundary (an Euler step onto it), a zero
    // or masked speed, an occupied cell, or the length
    const auto rk12 = [&](double x0, double y0, bool backward, std::vector<XY>* traj) {
        const auto f = [&](double xi, double yi, double* dx, double* dy) {
            const Stop s = forwardTime(xi, yi, dx, dy);
            if (s == Stop::None && backward) *dx = -*dx, *dy = -*dy;
            return s;
        };
        const double maxerror = 0.003 * o.maxErrorScale;
        double maxds = pyMin(pyMin(1. / mnx, 1. / mny), 0.1);
        maxds *= o.maxStepScale;
        double ds = maxds, stotal = 0, xi = x0, yi = y0;
        while (true) {
            double k1x = 0, k1y = 0, k2x = 0, k2y = 0;
            Stop s = Stop::None;
            if (dmap.withinGrid(xi, yi)) traj->push_back({xi, yi});
            else s = Stop::OutOfBounds;
            if (s == Stop::None) s = f(xi, yi, &k1x, &k1y);
            if (s == Stop::None) s = f(xi + ds * k1x, yi + ds * k1y, &k2x, &k2y);
            if (s == Stop::OutOfBounds) {
                if (!traj->empty()) {
                    // _euler_step: straight on to the boundary
                    const XY last = traj->back();
                    double cx = 0, cy = 0;
                    if (f(last[0], last[1], &cx, &cy) != Stop::None) {
                        raised = true;
                        return stotal;
                    }
                    const double inf = std::numeric_limits<double>::infinity();
                    const double dsx = cx == 0 ? inf : cx < 0 ? last[0] / -cx : (nx - 1 - last[0]) / cx;
                    const double dsy = cy == 0 ? inf : cy < 0 ? last[1] / -cy : (ny - 1 - last[1]) / cy;
                    const double step = pyMin(dsx, dsy);
                    traj->push_back({last[0] + cx * step, last[1] + cy * step});
                    stotal += step;
                }
                break;
            }
            if (s == Stop::Terminate) break;
            const double dx1 = ds * k1x, dy1 = ds * k1y;
            const double dx2 = ds * 0.5 * (k1x + k2x), dy2 = ds * 0.5 * (k1y + k2y);
            // the error in axes coordinates
            const double err = std::hypot((dx2 - dx1) / (nx - 1), (dy2 - dy1) / (ny - 1));
            if (err < maxerror) {
                xi += dx2;
                yi += dy2;
                if (dmap.updateTrajectory(xi, yi, o.broken) != Stop::None) break;
                if (stotal + ds > maxlength) break;
                stotal += ds;
            }
            ds = err == 0 ? maxds : pyMin(maxds, 0.85 * ds * std::pow(maxerror / err, 0.5));
        }
        return stotal;
    };
    // integrate: backward and forward from the start, kept when longer than minlength
    const auto integrate = [&](double x0, double y0, std::vector<XY>* traj) {
        if (!dmap.startTrajectory(x0, y0, o.broken)) return false;
        double stotal = 0.;
        traj->clear();
        if (o.direction == "both" || o.direction == "backward") {
            std::vector<XY> t;
            stotal += rk12(x0, y0, true, &t);
            traj->insert(traj->end(), t.rbegin(), t.rend());
        }
        if (raised) return false;
        if (o.direction == "both" || o.direction == "forward") {
            dmap.resetStartPoint(x0, y0);
            std::vector<XY> t;
            stotal += rk12(x0, y0, false, &t);
            if (t.size() > 1) traj->insert(traj->end(), t.begin() + 1, t.end());
        }
        if (raised) return false;
        if (stotal > o.minlength) return true;
        dmap.undoTrajectory();
        return false;
    };
    out->clear();
    std::vector<XY> t;
    if (!o.startPoints) {
        // _gen_starting_points: the mask's cells from a corner, spiralling inwards
        int xfirst = 0, yfirst = 1, xlast = mnx - 1, ylast = mny - 1, x = 0, y = 0;
        char direction = 'r';
        for (long i = 0; i < long(mnx) * long(mny); ++i) {
            if (dmap.at(x, y) == 0 && integrate(x * dmap.xMask2grid, y * dmap.yMask2grid, &t)) out->push_back(t);
            if (raised) return *error = "TerminateTrajectory", false;
            if (direction == 'r') {
                if (++x >= xlast) --xlast, direction = 'u';
            } else if (direction == 'u') {
                if (++y >= ylast) --ylast, direction = 'l';
            } else if (direction == 'l') {
                if (--x <= xfirst) ++xfirst, direction = 'd';
            } else {
                if (--y <= yfirst) ++yfirst, direction = 'r';
            }
        }
    } else {
        for (const XY& p : *o.startPoints)
            if (!(grid.xOrigin <= p[0] && p[0] <= grid.xOrigin + grid.width && grid.yOrigin <= p[1] && p[1] <= grid.yOrigin + grid.height))
                return *error = "Starting point (" + support::pythonRepr(p[0]) + ", " + support::pythonRepr(p[1]) + ") outside of data boundaries", false;
        for (const XY& p : *o.startPoints) {
            double xg = (p[0] - grid.xOrigin) * dmap.xData2grid, yg = (p[1] - grid.yOrigin) * dmap.yData2grid;
            // np.clip (NumPy's max then min: -0.0 clipped at 0 is 0.0)
            xg = xg > 0.0 ? xg : 0.0, xg = xg < double(nx - 1) ? xg : double(nx - 1);
            yg = yg > 0.0 ? yg : 0.0, yg = yg < double(ny - 1) ? yg : double(ny - 1);
            if (integrate(xg, yg, &t)) out->push_back(t);
            if (raised) return *error = "TerminateTrajectory", false;
        }
    }
    return true;
}

}  // namespace calculate_plot::engine
