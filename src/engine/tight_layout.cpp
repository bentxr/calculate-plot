// Derived from Matplotlib 3.11.2: lib/matplotlib/_tight_layout.py (_auto_adjust_subplotpars,
// get_subplotspec_list, get_tight_layout_figure), lib/matplotlib/layout_engine.py
// (TightLayoutEngine.execute) and lib/matplotlib/figure.py (Figure.tight_layout, subplots_adjust).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/font_manager.hpp"
#include "engine/tight_layout.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace calculate_plot::engine {

namespace {

using Rect = std::array<std::optional<double>, 4>;

// ndarray.max(): NaN wins
double npMax(const std::vector<double>& v) {
    double m = -std::numeric_limits<double>::infinity();
    for (double x : v) {
        if (std::isnan(x)) return x;
        if (x > m) m = x;
    }
    return m;
}

// Python's max(a, 0): the first unless 0 is greater
double pyMaxZero(double a) { return 0.0 > a ? 0.0 : a; }

// Python's truth of an optional number: None and zero are false
bool truthy(const std::optional<double>& v) { return v && *v != 0; }

struct Span {
    int start, stop;
};

struct Group {
    SubplotSpec spec;
    std::vector<Axes*> axes;
};

// The box of the figure-coordinates points of a window-coordinates box (transform_bbox).
std::array<double, 4> toFigure(const Figure& figure, const transforms::BboxBase& box) {
    const auto inv = figure.transFigure->inverted();
    const auto p = box.points();
    const auto a = inv->transform(p[0], p[1]), b = inv->transform(p[2], p[3]);
    return {a[0], a[1], b[0], b[1]};
}

double xmin(const std::array<double, 4>& p) { return std::min(p[0], p[2]); }
double xmax(const std::array<double, 4>& p) { return std::max(p[0], p[2]); }
double ymin(const std::array<double, 4>& p) { return std::min(p[1], p[3]); }
double ymax(const std::array<double, 4>& p) { return std::max(p[1], p[3]); }

// _auto_adjust_subplotpars
std::optional<SubplotsAdjustment> autoAdjust(Figure& figure, Renderer& renderer, int rows, int cols,
                                             const std::vector<std::pair<Span, Span>>& spanPairs,
                                             const std::vector<Group>& groups,
                                             const std::vector<std::array<double, 4>>& axBboxes,
                                             const TightLayoutOptions& o, const std::optional<Rect>& rect) {
    // FontProperties(size=rcParams["font.size"]).get_size_in_points() / 72
    const double fontSizeInch = text::atLeastOnePoint(figure.rc().number("font.size")) / 72;
    const double padInch = o.pad * fontSizeInch;
    const double vpadInch = o.hPad ? *o.hPad * fontSizeInch : padInch;
    const double hpadInch = o.wPad ? *o.wPad * fontSizeInch : padInch;
    std::optional<double> marginLeft, marginBottom, marginRight, marginTop;
    if (rect) {
        marginLeft = (*rect)[0];
        marginBottom = (*rect)[1];
        if (truthy((*rect)[2])) marginRight = 1 - *(*rect)[2];
        if (truthy((*rect)[3])) marginTop = 1 - *(*rect)[3];
    }
    std::vector<std::vector<double>> vspaces(std::size_t(rows + 1), std::vector<double>(std::size_t(cols), 0.0));
    std::vector<std::vector<double>> hspaces(std::size_t(rows), std::vector<double>(std::size_t(cols + 1), 0.0));
    for (std::size_t g = 0; g < groups.size(); ++g) {
        std::vector<transforms::Bbox> bb;
        for (Axes* ax : groups[g].axes)
            if (ax->visible)
                if (auto b = ax->tightbbox(renderer, true)) bb.push_back(*b);
        if (bb.empty()) continue;  // all invisible
        const auto tight = toFigure(figure, transforms::unionOf(bb));
        const auto& ax = axBboxes[g];
        const auto [rowspan, colspan] = spanPairs[g];
        for (int r = rowspan.start; r < rowspan.stop; ++r) {
            hspaces[std::size_t(r)][std::size_t(colspan.start)] += xmin(ax) - xmin(tight);  // l
            hspaces[std::size_t(r)][std::size_t(colspan.stop)] += xmax(tight) - xmax(ax);   // r
        }
        for (int c = colspan.start; c < colspan.stop; ++c) {
            vspaces[std::size_t(rowspan.start)][std::size_t(c)] += ymax(tight) - ymax(ax);  // t
            vspaces[std::size_t(rowspan.stop)][std::size_t(c)] += ymin(ax) - ymin(tight);   // b
        }
    }
    const auto size = figure.bboxInches->points();
    const double figWidthInch = size[2], figHeightInch = size[3];
    const auto column = [&](std::size_t c) {
        std::vector<double> v;
        for (const auto& row : hspaces) v.push_back(row[c]);
        return v;
    };
    if (!truthy(marginLeft)) marginLeft = pyMaxZero(npMax(column(0))) + padInch / figWidthInch;  // (no supylabel yet)
    if (!truthy(marginRight)) marginRight = pyMaxZero(npMax(column(std::size_t(cols)))) + padInch / figWidthInch;
    if (!truthy(marginTop)) {
        marginTop = pyMaxZero(npMax(vspaces.front())) + padInch / figHeightInch;
        if (figure.suptitle) {
            const auto b = toFigure(figure, static_cast<Text&>(*figure.suptitle).windowExtent(renderer));
            const double relHeight = b[3] - b[1];
            *marginTop += relHeight + padInch / figHeightInch;
        }
    }
    if (!truthy(marginBottom)) marginBottom = pyMaxZero(npMax(vspaces.back())) + padInch / figHeightInch;  // (no supxlabel yet)
    if (*marginLeft + *marginRight >= 1) return std::nullopt;  // the left and right margins cannot fit
    if (*marginBottom + *marginTop >= 1) return std::nullopt;  // the bottom and top margins cannot fit
    SubplotsAdjustment out;
    out.left = *marginLeft;
    out.right = 1 - *marginRight;
    out.bottom = *marginBottom;
    out.top = 1 - *marginTop;
    if (cols > 1) {
        std::vector<double> inner;
        for (const auto& row : hspaces) inner.insert(inner.end(), row.begin() + 1, row.end() - 1);
        const double hspace = npMax(inner) + hpadInch / figWidthInch;
        const double hAxes = (1 - *marginRight - *marginLeft - hspace * (cols - 1)) / cols;
        if (hAxes < 0) return std::nullopt;  // the Axes cannot be made narrow enough
        out.wspace = hspace / hAxes;
    }
    if (rows > 1) {
        std::vector<double> inner;
        for (std::size_t r = 1; r + 1 < vspaces.size(); ++r) inner.insert(inner.end(), vspaces[r].begin(), vspaces[r].end());
        const double vspace = npMax(inner) + vpadInch / figHeightInch;
        const double vAxes = (1 - *marginTop - *marginBottom - vspace * (rows - 1)) / rows;
        if (vAxes < 0) return std::nullopt;  // the Axes cannot be made short enough
        out.hspace = vspace / vAxes;
    }
    return out;
}

// GridSpec.locally_modified_subplot_params: any of its own parameters set (and not zero)
bool locallyModified(const GridSpec& gs) {
    for (const auto& v : {gs.left, gs.bottom, gs.right, gs.top, gs.wspace, gs.hspace})
        if (truthy(v)) return true;
    return false;
}

}  // namespace

bool subplotsAdjust(Figure& figure, const SubplotsAdjustment& a, std::string* error) {
    if (!figure.subplotpars.update(a.left, a.bottom, a.right, a.top, a.wspace, a.hspace, error)) return false;
    for (auto& ax : figure.axes)
        if (ax->subplotSpec) ax->setPosition(*ax->subplotSpec->position(figure.subplotpars));
    return true;
}

std::optional<SubplotsAdjustment> tightLayoutParameters(Figure& figure, Renderer& renderer,
                                                        const TightLayoutOptions& options) {
    // get_subplotspec_list, then the Axes grouped by their subplot spec (in order of appearance);
    // the Axes without one are left out (matplotlib warns).
    std::vector<Group> groups;
    for (auto& ax : figure.axes) {
        if (!ax->subplotSpec) continue;
        const SubplotSpec ss = ax->subplotSpec->topmost();  // (a colorbar made beside an Axes shares its cell)
        if (locallyModified(*ss.gridspec)) continue;
        auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) {
            return x.spec.gridspec == ss.gridspec && x.spec.num1 == ss.num1 && x.spec.num2 == ss.num2;
        });
        if (g == groups.end()) {
            groups.push_back({ss, {}});
            g = groups.end() - 1;
        }
        g->axes.push_back(ax.get());
    }
    if (groups.empty()) return std::nullopt;  // ({} in matplotlib: nothing to adjust)
    std::vector<std::array<double, 4>> axBboxes;
    int maxRows = 0, maxCols = 0;
    for (const Group& g : groups) {
        axBboxes.push_back(g.spec.position(figure.subplotpars)->points());
        maxRows = std::max(maxRows, g.spec.gridspec->nrows);
        maxCols = std::max(maxCols, g.spec.gridspec->ncols);
    }
    std::vector<std::pair<Span, Span>> spanPairs;
    for (const Group& g : groups) {
        const int rows = g.spec.gridspec->nrows, cols = g.spec.gridspec->ncols;
        if (maxRows % rows != 0 || maxCols % cols != 0) return std::nullopt;  // grids that are not multiples
        const int divRow = maxRows / rows, divCol = maxCols / cols;
        // SubplotSpec.rowspan and colspan
        const int c1 = std::min(g.spec.num1 % cols, g.spec.num2 % cols), c2 = std::max(g.spec.num1 % cols, g.spec.num2 % cols);
        spanPairs.push_back({{g.spec.num1 / cols * divRow, (g.spec.num2 / cols + 1) * divRow},
                             {c1 * divCol, (c2 + 1) * divCol}});
    }
    auto kwargs = autoAdjust(figure, renderer, maxRows, maxCols, spanPairs, groups, axBboxes, options, std::nullopt);
    if (options.rect && kwargs) {
        Rect r{(*options.rect)[0], (*options.rect)[1], (*options.rect)[2], (*options.rect)[3]};
        *r[0] += *kwargs->left;
        *r[1] += *kwargs->bottom;
        *r[2] -= (1 - *kwargs->right);
        *r[3] -= (1 - *kwargs->top);
        kwargs = autoAdjust(figure, renderer, maxRows, maxCols, spanPairs, groups, axBboxes, options, r);
    }
    return kwargs;
}

bool tightLayout(Figure& figure, const TightLayoutOptions& options, const std::string& locale,
                 const std::string& fontDirectory, std::string* error) {
    // TightLayoutEngine.execute with fig._get_renderer(): the Agg canvas' renderer at the figure's dpi
    const auto size = figure.canvasSize();
    AggTraceRenderer renderer(size[0], size[1], figure.dpi, figure.rc(), locale, fontDirectory);
    const auto kwargs = tightLayoutParameters(figure, renderer, options);
    if (!renderer.error().empty()) {
        *error = renderer.error();
        return false;
    }
    if (!kwargs) return true;
    return subplotsAdjust(figure, *kwargs, error);
}

}  // namespace calculate_plot::engine
