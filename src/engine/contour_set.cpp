// Derived from Matplotlib 3.11.2: lib/matplotlib/contour.py (ContourSet, QuadContourSet) and
// lib/matplotlib/cbook.py (_resize_sequence).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/contour_set.hpp"

#include "axes/log_ticker.hpp"
#include "axes/ticker.hpp"
#include "contour/contour.hpp"
#include "rc/rc_params.hpp"
#include "support/histogram.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::engine {

namespace {

using Grid = std::vector<std::vector<double>>;

std::string shapeText(int rows, int cols) { return "(" + std::to_string(rows) + ", " + std::to_string(cols) + ")"; }

// cbook._resize_sequence(seq, N): cut to N, or repeated to N
template <typename T>
std::vector<T> resizeSequence(const std::vector<T>& seq, std::size_t n) {
    if (seq.size() >= n) return std::vector<T>(seq.begin(), seq.begin() + std::ptrdiff_t(n));
    std::vector<T> out;
    for (std::size_t i = 0; i < n; ++i) out.push_back(seq[i % seq.size()]);
    return out;
}

// np.meshgrid(x, y): x repeated over the rows, y over the columns
void meshgrid(const std::vector<double>& x, const std::vector<double>& y, Grid* X, Grid* Y) {
    X->assign(y.size(), x);
    Y->assign(y.size(), std::vector<double>(x.size()));
    for (std::size_t i = 0; i < y.size(); ++i) std::fill((*Y)[i].begin(), (*Y)[i].end(), y[i]);
}

}  // namespace

ContourSet::ContourSet(const rc::Params& rc) : Collection(rc) { groupName = "QuadContourSet"; }

std::unique_ptr<ContourSet> ContourSet::make(const rc::Params& rc, const ContourArgs& a, std::string* error) {
    if (a.filled) return *error = "filled contours are not supported yet", nullptr;
    std::unique_ptr<ContourSet> cs(new ContourSet(rc));
    ContourSet& c = *cs;
    // Collection(antialiaseds=antialiased, alpha=alpha): not antialiased when filled, unless asked
    if (a.antialiased) c.setAntialiaseds({*a.antialiased});
    else if (a.filled) c.setAntialiaseds({false});
    if (a.alpha) c.setAlpha(a.alpha);
    c.filled = a.filled;
    c.extend = a.extend;
    c.logscale = a.logNorm;  // (a LogLocator is not taken yet)
    std::optional<std::string> origin = a.origin;
    if (origin && *origin != "lower" && *origin != "upper" && *origin != "image")
        return *error = "'" + *origin + "' is not a valid value for origin. Supported values are None, 'lower', 'upper', 'image'", nullptr;
    if (a.colors && a.cmapGiven) return *error = "Either colors or cmap must be None", nullptr;
    if (origin && *origin == "image") origin = rc.text("image.origin");
    std::string negativeLinestyles;
    if (a.negativeLinestyles) {
        negativeLinestyles = *a.negativeLinestyles;
    } else {
        const support::Json& v = rc.value("contour.negative_linestyle");
        if (!v.isString()) return *error = "contour.negative_linestyle as an on-off ink sequence is not supported yet", nullptr;
        negativeLinestyles = v.asString();
    }

    // QuadContourSet._process_args: the generator of mpl2014 (the only one taken yet)
    const std::string algorithm = a.algorithm ? *a.algorithm : rc.text("contour.algorithm");
    if (algorithm != "mpl2005" && algorithm != "mpl2014" && algorithm != "serial" && algorithm != "threaded")
        return *error = "'" + algorithm + "' is not a valid value for contour.algorithm; supported values are ['mpl2005', 'mpl2014', 'serial', 'threaded']", nullptr;
    if (algorithm != "mpl2014") return *error = "the " + algorithm + " contour algorithm is not supported yet", nullptr;
    const bool cornerMask = a.cornerMask ? *a.cornerMask : rc.boolean("contour.corner_mask");

    // _contour_args: the grid from x and y (_check_xyz) or from z's shape (_initialize_x_y)
    const ImageArray& z = a.z;
    if (z.channels != 0) return *error = "Input z must be 2D, not 3D", nullptr;
    if (z.rows < 2 || z.cols < 2)
        return *error = "Input z must be at least a (2, 2) shaped array, but has shape " + shapeText(z.rows, z.cols), nullptr;
    const int Ny = z.rows, Nx = z.cols;
    Grid X, Y;
    if (a.x) {
        const Grid& x = *a.x;
        const Grid& y = *a.y;
        const bool x1 = a.xDims == 1, y1 = a.yDims == 1;
        if (x1 != y1)
            return *error = std::string("Number of dimensions of x (") + (x1 ? "1" : "2") + ") and y (" + (y1 ? "1" : "2") + ") do not match", nullptr;
        if (x1) {
            if (int(x[0].size()) != Nx)
                return *error = "Length of x (" + std::to_string(x[0].size()) + ") must match number of columns in z (" + std::to_string(Nx) + ")", nullptr;
            if (int(y[0].size()) != Ny)
                return *error = "Length of y (" + std::to_string(y[0].size()) + ") must match number of rows in z (" + std::to_string(Ny) + ")", nullptr;
            meshgrid(x[0], y[0], &X, &Y);
        } else {
            if (int(x.size()) != Ny || int(x[0].size()) != Nx)
                return *error = "Shapes of x " + shapeText(int(x.size()), int(x[0].size())) + " and z " + shapeText(Ny, Nx) + " do not match", nullptr;
            if (int(y.size()) != Ny || int(y[0].size()) != Nx)
                return *error = "Shapes of y " + shapeText(int(y.size()), int(y[0].size())) + " and z " + shapeText(Ny, Nx) + " do not match", nullptr;
            X = x, Y = y;
        }
    } else {
        std::vector<double> xs(std::size_t(Nx), 0.0), ys(std::size_t(Ny), 0.0);
        if (!origin) {  // (not for image-matching)
            if (!a.extent) {
                for (int j = 0; j < Nx; ++j) xs[std::size_t(j)] = j;
                for (int i = 0; i < Ny; ++i) ys[std::size_t(i)] = i;
            } else {
                xs = support::npLinspace((*a.extent)[0], (*a.extent)[1], Nx);
                ys = support::npLinspace((*a.extent)[2], (*a.extent)[3], Ny);
            }
        } else {
            const std::array<double, 4> e = a.extent ? *a.extent : std::array<double, 4>{0, double(Nx), 0, double(Ny)};
            const double dx = (e[1] - e[0]) / Nx, dy = (e[3] - e[2]) / Ny;
            for (int j = 0; j < Nx; ++j) xs[std::size_t(j)] = e[0] + (j + 0.5) * dx;
            for (int i = 0; i < Ny; ++i) ys[std::size_t(i)] = e[2] + (i + 0.5) * dy;
            if (*origin == "upper") std::reverse(ys.begin(), ys.end());
        }
        meshgrid(xs, ys, &X, &Y);
    }
    // z masked where not finite; its limits from the rest (on a log scale, values <= 0 masked too)
    std::vector<bool> mask(z.size());
    for (std::size_t i = 0; i < z.size(); ++i) mask[i] = z.masked(i) || !std::isfinite(z.data[i]);
    const auto limits = [&]() {
        double lo = std::numeric_limits<double>::infinity(), hi = -lo;
        for (std::size_t i = 0; i < z.size(); ++i)
            if (!mask[i]) lo = std::min(lo, z.data[i]), hi = std::max(hi, z.data[i]);
        c.zmin = lo, c.zmax = hi;
        return lo <= hi;
    };
    if (!limits()) return *error = "contours of a z without a valid value are not supported yet", nullptr;
    if (c.logscale && c.zmin <= 0) {
        for (std::size_t i = 0; i < z.size(); ++i) mask[i] = mask[i] || z.data[i] <= 0;  // (a warning)
        if (!limits()) return *error = "contours of a z without a valid value are not supported yet", nullptr;
    }

    // _process_contour_level_args: the levels given, or picked by a locator (_autolev); bool z has its own
    std::optional<long long> n = a.nlevels;
    if (a.levels) {
        c.levels = *a.levels;
    } else if (!n && z.dtype == colors::DType::Bool) {
        c.levels = a.filled ? std::vector<double>{0, .5, 1} : std::vector<double>{.5};
    } else {
        std::vector<double> lev;
        if (c.logscale) {
            axes::LogLocator locator;  // LogLocator(numticks=N)
            if (n && (*n < 0 || (*n < 2 && rc.boolean("_internal.classic_mode"))))
                return *error = "levels as an integer below " + std::string(rc.boolean("_internal.classic_mode") ? "2" : "0") +
                                " on a log scale are not supported yet", nullptr;
            if (n) locator.numticks = int(*n);
            locator.classic = rc.boolean("_internal.classic_mode");
            lev = locator.tickValues(c.zmin, c.zmax);
        } else {
            if (n && (*n < 0 || *n > 1000)) return *error = "levels as an integer must be from 0 to 1000 (others are not supported yet)", nullptr;
            axes::MaxNLocator locator;  // MaxNLocator(N + 1, min_n_ticks=1), N 7 for None
            locator.nbins = int(n ? *n : 7) + 1;
            locator.minNTicks = 1;
            lev = locator.tickValues(c.zmin, c.zmax);
        }
        // the ticks that bound the data, one more on each side unless extended there; all of them when fewer
        // than three are left
        std::ptrdiff_t i0 = 0, i1 = std::ptrdiff_t(lev.size());
        for (std::size_t i = 0; i < lev.size(); ++i)
            if (lev[i] < c.zmin) i0 = std::ptrdiff_t(i);
        for (std::size_t i = lev.size(); i-- > 0;)
            if (lev[i] > c.zmax) i1 = std::ptrdiff_t(i) + 1;
        if (a.extend == "min" || a.extend == "both") i0 += 1;
        if (a.extend == "max" || a.extend == "both") i1 -= 1;
        if (i1 - i0 < 3) i0 = 0, i1 = std::ptrdiff_t(lev.size());
        c.levels.assign(lev.begin() + i0, lev.begin() + i1);
    }
    if (c.levels.empty()) return *error = "contours without levels are not supported yet", nullptr;
    if (a.filled && c.levels.size() < 2) return *error = "Filled contours require at least 2 levels.", nullptr;
    for (std::size_t i = 1; i < c.levels.size(); ++i)
        if (!(c.levels[i] - c.levels[i - 1] > 0.0)) return *error = "Contour levels must be increasing", nullptr;

    // the generator, then the grid's extent
    std::vector<double> xv, yv, zv;
    for (int i = 0; i < Ny; ++i)
        for (int j = 0; j < Nx; ++j) {
            const double xx = X[std::size_t(i)][std::size_t(j)], yy = Y[std::size_t(i)][std::size_t(j)];
            if (!std::isfinite(xx) || !std::isfinite(yy)) return *error = "contours of x and y that are not finite are not supported yet", nullptr;
            xv.push_back(xx), yv.push_back(yy);
            const std::size_t k = std::size_t(i) * std::size_t(Nx) + std::size_t(j);
            zv.push_back(mask[k] ? std::numeric_limits<double>::quiet_NaN() : z.data[k]);
        }
    auto generator = contour::Generator::make(xv, yv, zv, Ny, Nx, cornerMask, a.nchunk, error);
    if (!generator) return nullptr;
    c.mins = {*std::min_element(xv.begin(), xv.end()), *std::min_element(yv.begin(), yv.end())};
    c.maxs = {*std::max_element(xv.begin(), xv.end()), *std::max_element(yv.begin(), yv.end())};

    // _process_levels: the extended levels and the layers
    c.extendedLevels = c.levels;
    const double lower = c.logscale ? 1e-250 : -1e250, upper = 1e250;
    if (a.extend == "both" || a.extend == "min") c.extendedLevels.insert(c.extendedLevels.begin(), lower);
    if (a.extend == "both" || a.extend == "max") c.extendedLevels.push_back(upper);
    c.layers = c.levels;  // (the lines' layers are the levels)
    const bool extendMin = a.extend == "min" || a.extend == "both", extendMax = a.extend == "max" || a.extend == "both";

    // colors=: a ListedColormap of them, resized to the levels (the first and last under and over the ends
    // when there is one for each extension)
    colors::Colormap cmap = a.cmap;
    if (a.colors) {
        const std::vector<colors::Rgba>& seq = *a.colors;
        if (seq.empty()) return *error = "an empty list of colors is not supported yet", nullptr;
        const std::size_t ncolors = c.levels.size() - (a.filled ? 1 : 0);
        const std::size_t total = ncolors + (extendMin ? 1 : 0) + (extendMax ? 1 : 0);
        const bool underOver = seq.size() == total && (extendMin || extendMax);
        const std::size_t i0 = underOver && extendMin ? 1 : 0;
        cmap = colors::Colormap::listed(resizeSequence(std::vector<colors::Rgba>(seq.begin() + std::ptrdiff_t(i0), seq.end()), ncolors),
                                        "from_list", underOver && extendMin ? std::optional<colors::Rgba>(seq.front()) : std::nullopt,
                                        underOver && extendMax ? std::optional<colors::Rgba>(seq.back()) : std::nullopt);
    }
    c.cmap = cmap;
    if (a.norm) c.norm = a.norm;
    if (a.vmin) c.norm->setVmin(a.vmin);
    if (a.vmax) c.norm->setVmax(a.vmax);

    // _process_colors: with colors, the indices of the colormap's colours (NoNorm); else the layers
    const bool monochrome = cmap.monochrome();
    colors::MaskedArray cvalues;
    if (a.colors) {
        long long i0 = 0, i1 = (long long)c.levels.size();
        if (a.filled) {
            i1 -= 1;
            if (extendMin) i0 -= 1;
            if (extendMax) i1 += 1;
        }
        for (long long i = i0; i < i1; ++i) cvalues.data.push_back(double(i));
        cvalues.dtype = colors::DType::Int64;
        colors::NormSpec spec;
        spec.kind = "NoNorm";
        c.norm = std::make_shared<colors::Norm>();
        if (!colors::Norm::make(spec, c.norm.get(), error)) return nullptr;
    } else {
        cvalues.data = c.layers;
    }
    colors::MaskedArray levelValues;
    levelValues.data = c.levels;
    c.norm->autoscaleNone(colors::processValue(levelValues));
    c.array = cvalues;
    if (extendMin || extendMax) c.norm->setClip(false);

    // the paths: one compound path per level
    for (double level : c.levels) {
        const contour::Paths p = generator->lines(level);
        std::vector<double> vertices;
        std::vector<unsigned char> codes;
        for (std::size_t k = 0; k < p.vertices.size(); ++k) {
            vertices.insert(vertices.end(), p.vertices[k].begin(), p.vertices[k].end());
            codes.insert(codes.end(), p.codes[k].begin(), p.codes[k].end());
        }
        c.paths.push_back(Path::make(std::move(vertices), std::move(codes), rc));
    }
    c.offsets = {{0.0, 0.0}};  // get_offsets() without offsets: one at the origin

    // lines: no face, the widths and styles of the levels (negative ones in their own style when monochrome)
    const std::size_t nlev = c.levels.size();
    c.setFacecolor(ColorArg{ColorArg::None, {}});
    std::vector<double> widths;
    if (!a.linewidths) {
        const support::Json& w = rc.value("contour.linewidth");
        widths.assign(nlev, w.isNull() ? rc.number("lines.linewidth") : rc.number("contour.linewidth"));
    } else {
        if (a.linewidths->empty()) return *error = "division by zero", nullptr;
        widths = resizeSequence(*a.linewidths, nlev);
    }
    c.setLinewidths(widths);
    std::vector<std::string> styles;
    if (!a.linestyles) {
        styles.assign(nlev, "solid");
        if (monochrome) {
            const double eps = -(c.zmax - c.zmin) * 1e-15;
            for (std::size_t i = 0; i < nlev; ++i)
                if (c.levels[i] < eps) styles[i] = negativeLinestyles;
        }
    } else {
        if (a.linestyles->empty()) return *error = "division by zero", nullptr;
        styles = resizeSequence(*a.linestyles, nlev);
    }
    if (!c.setLinestyles(styles)) return *error = "linestyles other than 'solid', 'dashed', 'dashdot' and 'dotted' are not supported yet", nullptr;
    c.label = "_nolegend_";
    c.zorder = 2;
    c.stickyX = {c.mins[0], c.maxs[0]};
    c.stickyY = {c.mins[1], c.maxs[1]};
    return cs;
}

}  // namespace calculate_plot::engine
