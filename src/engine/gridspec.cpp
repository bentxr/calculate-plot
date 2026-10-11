// Derived from Matplotlib 3.11.2: lib/matplotlib/gridspec.py (SubplotParams, GridSpec, SubplotSpec).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/gridspec.hpp"

#include "rc/rc_params.hpp"
#include "support/pynum.hpp"

#include <algorithm>

namespace calculate_plot::engine {

SubplotParams SubplotParams::defaults(const rc::Params& rc) {
    return {rc.number("figure.subplot.left"),  rc.number("figure.subplot.bottom"),
            rc.number("figure.subplot.right"), rc.number("figure.subplot.top"),
            rc.number("figure.subplot.wspace"), rc.number("figure.subplot.hspace")};
}

bool SubplotParams::valid(std::string* error) const {
    SubplotParams copy = *this;
    return copy.update({}, {}, {}, {}, {}, {}, error);
}

bool SubplotParams::update(std::optional<double> l, std::optional<double> b, std::optional<double> r,
                           std::optional<double> t, std::optional<double> ws, std::optional<double> hs,
                           std::string* error) {
    if ((l ? *l : left) >= (r ? *r : right)) {
        *error = "left cannot be >= right";
        return false;
    }
    if ((b ? *b : bottom) >= (t ? *t : top)) {
        *error = "bottom cannot be >= top";
        return false;
    }
    if (l) left = *l;
    if (r) right = *r;
    if (b) bottom = *b;
    if (t) top = *t;
    if (ws) wspace = *ws;
    if (hs) hspace = *hs;
    return true;
}

namespace {

// The cells' accumulated extents along one direction: np.cumsum over the interleaved separators
// and cells [0, cell0, sep, cell1, sep, cell2, ...].
std::vector<double> accumulated(int n, double total, double space, const std::vector<double>& ratios,
                                const std::vector<bool>& ratiosInt) {
    const double cell = total / (n + space * (n - 1));
    const double sep = space * cell;
    const double sum = ratios.empty() ? double(n) : support::pythonSum(ratios, ratiosInt);
    const double norm = cell * n / sum;
    std::vector<double> out;
    double acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += i == 0 ? 0.0 : sep;
        out.push_back(acc);
        acc += (ratios.empty() ? 1.0 : ratios[std::size_t(i)]) * norm;
        out.push_back(acc);
    }
    return out;
}

}  // namespace

GridSpec::Positions GridSpec::gridPositions(const SubplotParams& figureParams) const {
    SubplotParams p = figureParams;  // GridSpec.get_subplot_params: the figure's, updated with ours
    std::string ignored;
    p.update(left, bottom, right, top, wspace, hspace, &ignored);
    const double totWidth = p.right - p.left;
    const double totHeight = p.top - p.bottom;
    const std::vector<double> cellHs = accumulated(nrows, totHeight, p.hspace, heightRatios, heightRatiosInt);
    const std::vector<double> cellWs = accumulated(ncols, totWidth, p.wspace, widthRatios, widthRatiosInt);
    Positions out;
    for (int i = 0; i < nrows; ++i) {
        out.tops.push_back(p.top - cellHs[std::size_t(2 * i)]);
        out.bottoms.push_back(p.top - cellHs[std::size_t(2 * i + 1)]);
    }
    for (int i = 0; i < ncols; ++i) {
        out.lefts.push_back(p.left + cellWs[std::size_t(2 * i)]);
        out.rights.push_back(p.left + cellWs[std::size_t(2 * i + 1)]);
    }
    return out;
}

std::shared_ptr<transforms::Bbox> SubplotSpec::position(const SubplotParams& figureParams) const {
    const GridSpec::Positions g = gridspec->gridPositions(figureParams);
    const int ncols = gridspec->ncols;
    const int rows[2] = {num1 / ncols, num2 / ncols}, cols[2] = {num1 % ncols, num2 % ncols};
    // fig_bottoms[rows].min() etc. over the two corner cells
    const double bottom = std::min(g.bottoms[std::size_t(rows[0])], g.bottoms[std::size_t(rows[1])]);
    const double top = std::max(g.tops[std::size_t(rows[0])], g.tops[std::size_t(rows[1])]);
    const double left = std::min(g.lefts[std::size_t(cols[0])], g.lefts[std::size_t(cols[1])]);
    const double right = std::max(g.rights[std::size_t(cols[0])], g.rights[std::size_t(cols[1])]);
    return transforms::Bbox::fromExtents(left, bottom, right, top);
}

}  // namespace calculate_plot::engine
