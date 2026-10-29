// Derived from Matplotlib 3.11.2: lib/matplotlib/_constrained_layout.py, lib/matplotlib/_layoutgrid.py and
// lib/matplotlib/layout_engine.py (ConstrainedLayoutEngine.execute).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "engine/constrained_layout.hpp"

#include <string>

namespace calculate_plot::engine {
// kiwisolver's errors, which it would throw (KIWI_RAISE: see extern/kiwi/kiwi/solverimpl.h)
void kiwiRaise(const char* what);
}  // namespace calculate_plot::engine
#define KIWI_RAISE(error) ::calculate_plot::engine::kiwiRaise((error).what())
#include "kiwi/kiwi.h"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "engine/gridspec.hpp"
#include "engine/text.hpp"
#include "support/histogram.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace calculate_plot::engine {

namespace {

thread_local std::string kiwiError;

}  // namespace

void kiwiRaise(const char* what) {
    if (kiwiError.empty()) kiwiError = what;
}

namespace {

// --- kiwisolver's Python symbolics (py/src/symbolics.h): the order of terms and the constants as the
// Python module makes them from Matplotlib's expressions ------------------------------------------

struct Sym {
    enum class Kind { Variable, Term, Expression } kind;
    std::vector<kiwi::Term> terms;
    double constant = 0.0;
};

Sym var(const kiwi::Variable& v) { return {Sym::Kind::Variable, {kiwi::Term(v, 1.0)}, 0.0}; }

// BinaryMul: a Variable makes a Term of the coefficient; a Term multiplies its coefficient; an Expression
// each term and its constant
Sym mul(const Sym& a, double d) {
    Sym r = a;
    if (a.kind == Sym::Kind::Variable) {
        r.kind = Sym::Kind::Term;
        r.terms = {kiwi::Term(a.terms[0].variable(), d)};
        return r;
    }
    for (auto& t : r.terms) t = kiwi::Term(t.variable(), t.coefficient() * d);
    if (a.kind == Sym::Kind::Expression) r.constant = a.constant * d;
    return r;
}
Sym neg(const Sym& a) { return mul(a, -1.0); }                      // UnaryNeg
Sym div(const Sym& a, double d) { return mul(a, 1.0 / d); }        // BinaryDiv

// BinaryAdd
Sym add(const Sym& a, const Sym& b) {
    const bool ea = a.kind == Sym::Kind::Expression, eb = b.kind == Sym::Kind::Expression;
    Sym r{Sym::Kind::Expression, {}, 0.0};
    if (ea && eb) {  // the terms of both, the constants added
        r.terms = a.terms;
        r.terms.insert(r.terms.end(), b.terms.begin(), b.terms.end());
        r.constant = a.constant + b.constant;
    } else if (ea) {  // Expression + Term: the term appended
        r.terms = a.terms;
        r.terms.push_back(b.terms[0]);
        r.constant = a.constant;
    } else if (eb) {  // Term + Expression is Expression + Term
        r.terms = b.terms;
        r.terms.push_back(a.terms[0]);
        r.constant = b.constant;
    } else {  // two terms
        r.terms = {a.terms[0], b.terms[0]};
    }
    return r;
}
Sym add(const Sym& a, double d) {
    Sym r{Sym::Kind::Expression, a.terms, a.kind == Sym::Kind::Expression ? a.constant + d : d};
    return r;
}
Sym sub(const Sym& a, const Sym& b) { return add(a, neg(b)); }  // BinarySub
Sym sub(const Sym& a, double d) { return add(a, -d); }

// makecn(first, second, op) then `| strength`: the constraint first - second against zero
kiwi::Constraint constraint(const Sym& first, const Sym& second, kiwi::RelationalOperator op, double strength) {
    const Sym e = sub(first, second);
    const kiwi::Constraint required(kiwi::Expression(e.terms, e.constant), op, kiwi::strength::required);
    return kiwi::Constraint(required, strength);
}
kiwi::Constraint constraint(const Sym& first, double second, kiwi::RelationalOperator op, double strength) {
    const Sym e = sub(first, second);
    const kiwi::Constraint required(kiwi::Expression(e.terms, e.constant), op, kiwi::strength::required);
    return kiwi::Constraint(required, strength);
}

// --- _layoutgrid.LayoutGrid -----------------------------------------------------------------------

enum Side { Left, Right, LeftCb, RightCb, Bottom, Top, BottomCb, TopCb };

struct Span {
    int start, stop;  // range(start, stop)
};

class LayoutGrid {
public:
    // parent: none (the figure's grid in `rect`) or a LayoutGrid at (rows, cols), inside its margins
    LayoutGrid(kiwi::Solver* solver, LayoutGrid* parent, Span rows, Span cols, const std::array<double, 4>& rect, int nrows,
               int ncols, std::vector<double> widthRatios, std::vector<double> heightRatios)
        : solver_(solver), nrows_(nrows), ncols_(ncols), widthRatios_(std::move(widthRatios)), heightRatios_(std::move(heightRatios)) {
        if (widthRatios_.empty()) widthRatios_.assign(std::size_t(ncols), 1.0);
        if (heightRatios_.empty()) heightRatios_.assign(std::size_t(nrows), 1.0);
        for (Side s : {Left, Right, LeftCb, RightCb}) vals_[s].assign(std::size_t(ncols), 0.0);
        for (Side s : {Bottom, Top, BottomCb, TopCb}) vals_[s].assign(std::size_t(nrows), 0.0);
        const auto make = [](int n) {
            std::vector<kiwi::Variable> v;
            for (int i = 0; i < n; ++i) v.emplace_back();
            return v;
        };
        lefts_ = make(ncols);
        rights_ = make(ncols);
        for (Side s : {Left, Right, LeftCb, RightCb}) {
            margins_[s] = make(ncols);
            for (auto& v : margins_[s]) solver_->addEditVariable(v, kiwi::strength::strong);
        }
        bottoms_ = make(nrows);
        tops_ = make(nrows);
        for (Side s : {Bottom, Top, BottomCb, TopCb}) {
            margins_[s] = make(nrows);
            for (auto& v : margins_[s]) solver_->addEditVariable(v, kiwi::strength::strong);
        }
        resetMargins();
        hardConstraints();
        parentConstraints(parent, rows, cols, rect);
        gridConstraints();
    }

    void resetMargins() {
        for (Side s : {Left, Right, Bottom, Top, LeftCb, RightCb, BottomCb, TopCb}) editMargins(s, 0.0);
    }
    void editMargin(Side s, double size, int cell) {
        solver_->suggestValue(margins_[s][std::size_t(cell)], size);
        vals_[s][std::size_t(cell)] = size;
    }
    void editMarginMin(Side s, double size, int cell = 0) {
        if (size > vals_[s][std::size_t(cell)]) editMargin(s, size, cell);
    }
    void editMargins(Side s, double size) {
        for (std::size_t i = 0; i < vals_[s].size(); ++i) editMargin(s, size, int(i));
    }
    // edit_outer_margin_mins(margin, ss)
    void editOuterMarginMins(const std::map<Side, double>& m, Span rows, Span cols) {
        editMarginMin(Left, m.at(Left), cols.start);
        editMarginMin(LeftCb, m.at(LeftCb), cols.start);
        editMarginMin(Right, m.at(Right), cols.stop - 1);
        editMarginMin(RightCb, m.at(RightCb), cols.stop - 1);
        editMarginMin(Top, m.at(Top), rows.start);
        editMarginMin(TopCb, m.at(TopCb), rows.start);
        editMarginMin(Bottom, m.at(Bottom), rows.stop - 1);
        editMarginMin(BottomCb, m.at(BottomCb), rows.stop - 1);
    }
    double marginVal(Side s, int i) const { return vals_.at(s)[std::size_t(i)]; }

    // get_inner_bbox(rows, cols): (x0, y0, x1, y1)
    std::array<double, 4> innerBbox(Span rows, Span cols) const {
        const auto c0 = std::size_t(cols.start), c1 = std::size_t(cols.stop - 1), r0 = std::size_t(rows.start),
                   r1 = std::size_t(rows.stop - 1);
        return {lefts_[c0].value() + m(Left, c0) + m(LeftCb, c0), bottoms_[r1].value() + m(Bottom, r1) + m(BottomCb, r1),
                rights_[c1].value() - m(Right, c1) - m(RightCb, c1), tops_[r0].value() - m(Top, r0) - m(TopCb, r0)};
    }
    // get_bbox_for_cb(rows, cols): the cells less their colorbar margins only
    std::array<double, 4> bboxForCb(Span rows, Span cols) const {
        const auto c0 = std::size_t(cols.start), c1 = std::size_t(cols.stop - 1), r0 = std::size_t(rows.start),
                   r1 = std::size_t(rows.stop - 1);
        return {lefts_[c0].value() + m(LeftCb, c0), bottoms_[r1].value() + m(BottomCb, r1), rights_[c1].value() - m(RightCb, c1),
                tops_[r0].value() - m(TopCb, r0)};
    }
    int nrows() const { return nrows_; }
    int ncols() const { return ncols_; }

private:
    double m(Side s, std::size_t i) const { return margins_.at(s)[i].value(); }

    void require(const kiwi::Constraint& c) { solver_->addConstraint(c); }

    void hardConstraints() {
        for (int i = 0; i < ncols_; ++i) {
            const auto k = std::size_t(i);
            require(constraint(var(rights_[k]), var(lefts_[k]), kiwi::OP_GE, kiwi::strength::required));
            require(constraint(sub(sub(var(rights_[k]), var(margins_[Right][k])), var(margins_[RightCb][k])),
                           sub(sub(var(lefts_[k]), var(margins_[Left][k])), var(margins_[LeftCb][k])), kiwi::OP_GE,
                           kiwi::strength::required));
        }
        for (int i = 0; i < nrows_; ++i) {
            const auto k = std::size_t(i);
            require(constraint(var(tops_[k]), var(bottoms_[k]), kiwi::OP_GE, kiwi::strength::required));
            require(constraint(sub(sub(var(tops_[k]), var(margins_[Top][k])), var(margins_[TopCb][k])),
                           sub(sub(var(bottoms_[k]), var(margins_[Bottom][k])), var(margins_[BottomCb][k])), kiwi::OP_GE,
                           kiwi::strength::required));
        }
    }

    void parentConstraints(LayoutGrid* parent, Span rows, Span cols, const std::array<double, 4>& rect) {
        if (!parent) {
            for (const kiwi::Constraint& c :
                 {constraint(var(lefts_[0]), rect[0], kiwi::OP_EQ, kiwi::strength::required),
                  constraint(var(rights_.back()), rect[0] + rect[2], kiwi::OP_EQ, kiwi::strength::required),
                  constraint(var(tops_[0]), rect[1] + rect[3], kiwi::OP_EQ, kiwi::strength::required),
                  constraint(var(bottoms_.back()), rect[1], kiwi::OP_EQ, kiwi::strength::required)})
                require(c);
            return;
        }
        // (inside the parent's margins: parent_inner)
        const auto c0 = std::size_t(cols.start), c1 = std::size_t(cols.stop - 1), r0 = std::size_t(rows.start),
                   r1 = std::size_t(rows.stop - 1);
        Sym left = var(parent->lefts_[c0]), right = var(parent->rights_[c1]), top = var(parent->tops_[r0]),
            bottom = var(parent->bottoms_[r1]);
        left = add(left, var(parent->margins_[Left][c0]));
        left = add(left, var(parent->margins_[LeftCb][c0]));
        right = sub(right, var(parent->margins_[Right][c1]));
        right = sub(right, var(parent->margins_[RightCb][c1]));
        top = sub(top, var(parent->margins_[Top][r0]));
        top = sub(top, var(parent->margins_[TopCb][r0]));
        bottom = add(bottom, var(parent->margins_[Bottom][r1]));
        bottom = add(bottom, var(parent->margins_[BottomCb][r1]));
        for (const kiwi::Constraint& c : {constraint(var(lefts_[0]), left, kiwi::OP_EQ, kiwi::strength::required),
                                          constraint(var(rights_.back()), right, kiwi::OP_EQ, kiwi::strength::required),
                                          constraint(var(tops_[0]), top, kiwi::OP_EQ, kiwi::strength::required),
                                          constraint(var(bottoms_.back()), bottom, kiwi::OP_EQ, kiwi::strength::required)})
            require(c);
    }

    void gridConstraints() {
        const auto width = [&](std::size_t i) {
            Sym w = sub(sub(var(rights_[i]), var(margins_[Right][i])), var(margins_[RightCb][i]));
            return sub(sub(sub(w, var(lefts_[i])), var(margins_[Left][i])), var(margins_[LeftCb][i]));
        };
        const Sym w0 = div(width(0), widthRatios_[0]);
        for (int i = 1; i < ncols_; ++i) {
            const auto k = std::size_t(i);
            require(constraint(width(k), mul(w0, widthRatios_[k]), kiwi::OP_EQ, kiwi::strength::strong));
            require(constraint(var(rights_[k - 1]), var(lefts_[k]), kiwi::OP_EQ, kiwi::strength::strong));
        }
        const auto height = [&](std::size_t i) {
            Sym h = sub(sub(var(tops_[i]), var(margins_[Top][i])), var(margins_[TopCb][i]));
            return sub(sub(sub(h, var(bottoms_[i])), var(margins_[Bottom][i])), var(margins_[BottomCb][i]));
        };
        const Sym h0 = div(height(0), heightRatios_[0]);
        for (int i = 1; i < nrows_; ++i) {
            const auto k = std::size_t(i);
            require(constraint(height(k), mul(h0, heightRatios_[k]), kiwi::OP_EQ, kiwi::strength::strong));
            require(constraint(var(bottoms_[k - 1]), var(tops_[k]), kiwi::OP_EQ, kiwi::strength::strong));
        }
    }

    kiwi::Solver* solver_;
    int nrows_, ncols_;
    std::vector<double> widthRatios_, heightRatios_;
    std::vector<kiwi::Variable> lefts_, rights_, bottoms_, tops_;
    std::map<Side, std::vector<kiwi::Variable>> margins_;
    std::map<Side, std::vector<double>> vals_;
};

// --- _constrained_layout --------------------------------------------------------------------------

Span rowspan(const SubplotSpec& ss) {
    const int ncols = ss.gridspec->ncols;
    return {ss.num1 / ncols, ss.num2 / ncols + 1};
}
Span colspan(const SubplotSpec& ss) {
    const int ncols = ss.gridspec->ncols;
    const int a = ss.num1 % ncols, b = ss.num2 % ncols;
    return {std::min(a, b), std::max(a, b) + 1};
}

// Bbox.transformed(transform): three corners through the transform
std::array<double, 4> transformed(const std::array<double, 4>& p, const transforms::Transform& t) {
    const auto ll = t.transform(p[0], p[1]), ul = t.transform(p[0], p[3]), lr = t.transform(p[2], p[1]);
    return {ll[0], ll[1], lr[0], ul[1]};
}

class Layout {
public:
    Layout(Figure& figure, Renderer& renderer) : figure_(figure), renderer_(renderer) {}

    // do_constrained_layout(fig, h_pad, w_pad, hspace, wspace, rect, compress); false where kiwisolver raises
    bool run(double hPad, double wPad, double hspace, double wspace, const std::array<double, 4>& rect, bool compress,
             std::string* error) {
        kiwiError.clear();
        // make_layoutgrids: the figure's grid, then one for each gridspec of the figure's Axes
        figureGrid_ = std::make_unique<LayoutGrid>(&solver_, nullptr, Span{0, 1}, Span{0, 1}, rect, 1, 1, std::vector<double>{},
                                                   std::vector<double>{});
        for (auto& ax : figure_.axes) {
            if (!ax->subplotSpec) continue;
            const GridSpec* gs = ax->subplotSpec->gridspec;
            if (grids_.count(gs)) continue;
            grids_[gs] = std::make_unique<LayoutGrid>(&solver_, figureGrid_.get(), Span{0, 1}, Span{0, 1}, rect, gs->nrows, gs->ncols,
                                                      gs->widthRatios, gs->heightRatios);
        }
        if (!kiwiError.empty()) return *error = kiwiError, false;
        if (grids_.empty()) return true;  // (a warning: there are no gridspecs with layoutgrids)
        for (int pass = 0; pass < 2; ++pass) {
            makeLayoutMargins(hPad, wPad, hspace, wspace);
            makeMarginSuptitles(hPad);
            matchSubmergedMargins();
            solver_.updateVariables();
            if (!kiwiError.empty()) return *error = kiwiError, false;
            if (checkNoCollapsedAxes()) {
                repositionAxes();
                if (compress) {
                    if (!compressFixedAspect()) return true;  // (a ValueError, which Figure.draw ignores)
                    solver_.updateVariables();
                    if (checkNoCollapsedAxes()) repositionAxes(true);
                    if (figure_.suptitle && figure_.suptitleAutopos) {
                        auto& t = static_cast<Text&>(*figure_.suptitle);
                        t.y = figureGrid_->innerBbox(Span{0, 1}, Span{0, 1})[3] + hPad;
                        t.verticalAlignment = "bottom";
                    }
                }
            }
            resetMargins();
            if (!kiwiError.empty()) return *error = kiwiError, false;
        }
        return true;
    }

private:
    LayoutGrid& gridOf(const Axes& ax) { return *grids_.at(ax.subplotSpec->gridspec); }

    // get_margin_from_padding(ax, w_pad=, h_pad=, hspace=, wspace=)
    std::map<Side, double> marginFromPadding(const Axes& ax, double wPad, double hPad, double hspace, double wspace) {
        const SubplotSpec& ss = *ax.subplotSpec;
        const GridSpec& gs = *ss.gridspec;
        double hs = gs.hspace ? *gs.hspace : hspace, ws = gs.wspace ? *gs.wspace : wspace;
        ws = ws / 2;
        hs = hs / 2;
        const int nrows = gs.nrows, ncols = gs.ncols;
        std::map<Side, double> margin{{LeftCb, wPad}, {RightCb, wPad}, {BottomCb, hPad}, {TopCb, hPad},
                                      {Left, 0.0},    {Right, 0.0},    {Top, 0.0},       {Bottom, 0.0}};
        const Span rows = rowspan(ss), cols = colspan(ss);
        if (ws / ncols > wPad) {
            if (cols.start > 0) margin[LeftCb] = ws / ncols;
            if (cols.stop < ncols) margin[RightCb] = ws / ncols;
        }
        if (hs / nrows > hPad) {
            if (rows.stop < nrows) margin[BottomCb] = hs / nrows;
            if (rows.start > 0) margin[TopCb] = hs / nrows;
        }
        return margin;
    }

    void makeLayoutMargins(double hPad, double wPad, double hspace, double wspace) {
        for (auto& ax : figure_.axes) {
            if (!ax->subplotSpec) continue;
            auto margin = marginFromPadding(*ax, wPad, hPad, hspace, wspace);
            // get_pos_and_bbox: the original position, and the tight box for the layout in figure coordinates
            const auto pos = ax->originalPosition->points();
            std::array<double, 4> bbox = pos;
            if (const auto tight = ax->tightbbox(renderer_, true))
                bbox = transformed(tight->points(), *figure_.transFigure->inverted());
            margin[Left] += pos[0] - bbox[0];
            margin[Right] += bbox[2] - pos[2];
            margin[Bottom] += pos[1] - bbox[1];
            margin[Top] += bbox[3] - pos[3];
            // margins for the colorbars (in the padding margins)
            const Span rows = rowspan(*ax->subplotSpec), cols = colspan(*ax->subplotSpec);
            for (Axes* cbax : ax->colorbars) {
                const double pad = colorbarPad(*cbax);
                const auto [cbRows, cbCols] = cbParentSpans(*cbax);
                const std::string& loc = cbax->colorbarInfo->location;
                const auto pb = posAndBbox(*cbax);
                const auto& cbbbox = pb.second;
                if (loc == "right") {
                    if (cbCols.stop == cols.stop) margin[RightCb] += (cbbbox[2] - cbbbox[0]) + pad;
                } else if (loc == "left") {
                    if (cbCols.start == cols.start) margin[LeftCb] += (cbbbox[2] - cbbbox[0]) + pad;
                } else if (loc == "top") {
                    if (cbRows.start == rows.start) margin[TopCb] += (cbbbox[3] - cbbbox[1]) + pad;
                } else {
                    if (cbRows.stop == rows.stop) margin[BottomCb] += (cbbbox[3] - cbbbox[1]) + pad;
                }
                // colorbars wider (taller) than the parent box in the cross direction
                if (loc == "top" || loc == "bottom") {
                    if (cbCols.start == cols.start && cbbbox[0] < bbox[0]) margin[Left] += bbox[0] - cbbbox[0];
                    if (cbCols.stop == cols.stop && cbbbox[2] > bbox[2]) margin[Right] += cbbbox[2] - bbox[2];
                } else {
                    if (cbRows.stop == rows.stop && cbbbox[1] < bbox[1]) margin[Bottom] += bbox[1] - cbbbox[1];
                    if (cbRows.start == rows.start && cbbbox[3] > bbox[3]) margin[Top] += cbbbox[3] - bbox[3];
                }
            }
            gridOf(*ax).editOuterMarginMins(margin, rowspan(*ax->subplotSpec), colspan(*ax->subplotSpec));
        }
    }

    void makeMarginSuptitles(double hPad) {
        if (!figure_.suptitle || !figure_.suptitleAutopos) return;
        auto& t = static_cast<Text&>(*figure_.suptitle);
        t.y = 1 - hPad;  // (h_pad_local: the pad through transFigure - transSubfigure, the identity)
        const auto box = t.windowExtent(renderer_).points();
        const auto inv = figure_.transFigure->inverted();
        const auto p0 = inv->transform(box[0], box[1]), p1 = inv->transform(box[2], box[3]);  // transform_bbox
        figureGrid_->editMarginMin(Top, (p1[1] - p0[1]) + 2 * hPad);
    }

    // match_submerged_margins: Axes spanning several columns (rows) take the largest inner margins of them
    void matchSubmergedMargins() {
        std::vector<Axes*> axs;
        for (auto& ax : figure_.axes)
            if (ax->subplotSpec) axs.push_back(ax.get());
        const auto values = [](const LayoutGrid& lg, Side s, int from, int to) {  // margin_vals[s][from:to]
            std::vector<double> v;
            for (int i = from; i < to; ++i) v.push_back(lg.marginVal(s, i));
            return v;
        };
        const auto plus = [](std::vector<double> a, const std::vector<double>& b) {
            for (std::size_t i = 0; i < a.size(); ++i) a[i] = a[i] + b[i];
            return a;
        };
        for (Axes* ax1 : axs) {
            const SubplotSpec& ss1 = *ax1->subplotSpec;
            LayoutGrid& lg1 = gridOf(*ax1);
            const Span cols1 = colspan(ss1), rows1 = rowspan(ss1);
            if (cols1.stop - cols1.start > 1) {
                const auto leftcb = values(lg1, LeftCb, cols1.start + 1, cols1.stop);
                const auto rightcb = values(lg1, RightCb, cols1.start, cols1.stop - 1);
                double maxsubl = support::npMax(plus(values(lg1, Left, cols1.start + 1, cols1.stop), leftcb));
                double maxsubr = support::npMax(plus(values(lg1, Right, cols1.start, cols1.stop - 1), rightcb));
                for (Axes* ax2 : axs) {
                    const Span cols2 = colspan(*ax2->subplotSpec);
                    const LayoutGrid& lg2 = gridOf(*ax2);
                    if (cols2.stop - cols2.start > 1) {
                        const double l2 = support::npMax(plus(values(lg2, Left, cols2.start + 1, cols2.stop),
                                                              values(lg2, LeftCb, cols2.start + 1, cols2.stop)));
                        if (l2 > maxsubl) maxsubl = l2;
                        const double r2 = support::npMax(plus(values(lg2, Right, cols2.start, cols2.stop - 1),
                                                              values(lg2, RightCb, cols2.start, cols2.stop - 1)));
                        if (r2 > maxsubr) maxsubr = r2;
                    }
                }
                for (int i = cols1.start + 1, k = 0; i < cols1.stop; ++i, ++k) lg1.editMarginMin(Left, maxsubl - leftcb[std::size_t(k)], i);
                for (int i = cols1.start, k = 0; i < cols1.stop - 1; ++i, ++k)
                    lg1.editMarginMin(Right, maxsubr - rightcb[std::size_t(k)], i);
            }
            if (rows1.stop - rows1.start > 1) {
                const auto topcb = values(lg1, TopCb, rows1.start + 1, rows1.stop);
                const auto bottomcb = values(lg1, BottomCb, rows1.start, rows1.stop - 1);
                double maxsubt = support::npMax(plus(values(lg1, Top, rows1.start + 1, rows1.stop), topcb));
                double maxsubb = support::npMax(plus(values(lg1, Bottom, rows1.start, rows1.stop - 1), bottomcb));
                for (Axes* ax2 : axs) {
                    const Span rows2 = rowspan(*ax2->subplotSpec);
                    const LayoutGrid& lg2 = gridOf(*ax2);
                    if (rows2.stop - rows2.start > 1) {  // np.max([np.max(...), maxsub])
                        maxsubt = support::npMax({support::npMax(plus(values(lg2, Top, rows2.start + 1, rows2.stop),
                                                                      values(lg2, TopCb, rows2.start + 1, rows2.stop))),
                                                  maxsubt});
                        maxsubb = support::npMax({support::npMax(plus(values(lg2, Bottom, rows2.start, rows2.stop - 1),
                                                                      values(lg2, BottomCb, rows2.start, rows2.stop - 1))),
                                                  maxsubb});
                    }
                }
                for (int i = rows1.start + 1, k = 0; i < rows1.stop; ++i, ++k) lg1.editMarginMin(Top, maxsubt - topcb[std::size_t(k)], i);
                for (int i = rows1.start, k = 0; i < rows1.stop - 1; ++i, ++k)
                    lg1.editMarginMin(Bottom, maxsubb - bottomcb[std::size_t(k)], i);
            }
        }
    }

    bool checkNoCollapsedAxes() {
        for (auto& ax : figure_.axes) {
            if (!ax->subplotSpec) continue;
            const LayoutGrid& lg = gridOf(*ax);
            for (int i = 0; i < lg.nrows(); ++i)
                for (int j = 0; j < lg.ncols(); ++j) {
                    const auto bb = lg.innerBbox(Span{i, i + 1}, Span{j, j + 1});
                    if (bb[2] - bb[0] <= 0 || bb[3] - bb[1] <= 0) return false;
                }
        }
        return true;
    }

    void repositionAxes(bool compress = false) {
        for (auto& ax : figure_.axes) {
            if (!ax->subplotSpec) continue;
            const auto b = gridOf(*ax).innerBbox(rowspan(*ax->subplotSpec), colspan(*ax->subplotSpec));
            const transforms::Bbox box(b);  // (through transFigure - transSubfigure, the identity)
            ax->setPosition(box);
            // the colorbars of which this Axes is the first parent, the last made first
            std::map<std::string, double> offset{{"left", 0}, {"right", 0}, {"bottom", 0}, {"top", 0}};
            for (auto it = ax->colorbars.rbegin(); it != ax->colorbars.rend(); ++it)
                if ((*it)->colorbarInfo->parents.front() == ax.get()) repositionColorbar(**it, &offset, compress);
        }
    }

    // get_cb_parent_spans: the rows and columns the colorbar's parents cover
    std::pair<Span, Span> cbParentSpans(const Axes& cbax) {
        Span rows{1 << 30, -(1 << 30)}, cols{1 << 30, -(1 << 30)};
        for (const Axes* p : cbax.colorbarInfo->parents) {
            const Span r = rowspan(*p->subplotSpec), c = colspan(*p->subplotSpec);
            rows.start = std::min(rows.start, r.start), rows.stop = std::max(rows.stop, r.stop);
            cols.start = std::min(cols.start, c.start), cols.stop = std::max(cols.stop, c.stop);
        }
        return {rows, cols};
    }

    // colorbar_get_pad: the pad as a fraction of the parents' inner box across the colorbar
    double colorbarPad(const Axes& cbax) {
        const auto [rows, cols] = cbParentSpans(cbax);
        const auto b = gridOf(*cbax.colorbarInfo->parents.front()).innerBbox(rows, cols);
        const std::string& loc = cbax.colorbarInfo->location;
        const double size = loc == "right" || loc == "left" ? b[2] - b[0] : b[3] - b[1];
        return cbax.colorbarInfo->pad * size;
    }

    // get_pos_and_bbox: the original position, and the tight box for the layout, in figure coordinates
    std::pair<std::array<double, 4>, std::array<double, 4>> posAndBbox(Axes& ax) {
        const auto pos = ax.originalPosition->points();
        std::array<double, 4> bbox = pos;
        if (const auto tight = ax.tightbbox(renderer_, true)) bbox = transformed(tight->points(), *figure_.transFigure->inverted());
        return {pos, bbox};
    }

    // reposition_colorbar: beside the parents' cells, past their colorbar margin, the pad, and the colorbars put there
    // already
    void repositionColorbar(Axes& cbax, std::map<std::string, double>* offset, bool compress) {
        const Axes::ColorbarInfo info = *cbax.colorbarInfo;
        LayoutGrid& lg = gridOf(*info.parents.front());
        const auto [rows, cols] = cbParentSpans(cbax);
        const auto bboxparent = lg.bboxForCb(rows, cols);
        auto pb = lg.innerBbox(rows, cols);
        if (compress && info.parents.size() == 1) {
            // the parent's actual position (after apply_aspect) across the colorbar
            Axes& parent = *info.parents.front();
            parent.applyAspect();
            const auto actual = parent.position->points();
            if (info.location == "left" || info.location == "right") pb = {pb[0], actual[1], pb[2], actual[3]};
            else pb = {actual[0], pb[1], actual[2], pb[3]};
        }
        const auto pbb = posAndBbox(cbax);
        const auto& cbpos = pbb.first;
        const auto& cbbbox = pbb.second;
        const double cbpad = colorbarPad(cbax);
        const auto shrunk = [](const std::array<double, 4>& b, double mx, double my) {
            return std::array<double, 4>{b[0], b[1], b[0] + mx * (b[2] - b[0]), b[1] + my * (b[3] - b[1])};
        };
        const auto anchored = [](const std::array<double, 4>& b, const std::array<double, 2>& c, const std::array<double, 4>& container) {
            const double dx = (container[0] + c[0] * ((container[2] - container[0]) - (b[2] - b[0]))) - b[0];
            const double dy = (container[1] + c[1] * ((container[3] - container[1]) - (b[3] - b[1]))) - b[1];
            return std::array<double, 4>{b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy};
        };
        const auto translated = [](const std::array<double, 4>& b, double tx, double ty) {
            return std::array<double, 4>{b[0] + tx, b[1] + ty, b[2] + tx, b[3] + ty};
        };
        std::array<double, 4> pbcb;
        auto& off = *offset;
        const double cbw = cbbbox[2] - cbbbox[0], cbh = cbbbox[3] - cbbbox[1];
        if (info.location == "left" || info.location == "right") {
            pbcb = anchored(shrunk(pb, info.fraction, info.shrink), info.anchor, pb);
            const double lmargin = cbpos[0] - cbbbox[0];
            if (info.location == "right") {
                double dx = bboxparent[2] - pbcb[0] + off["right"];
                dx += cbpad + lmargin;
                off["right"] += cbw + cbpad;
                pbcb = translated(pbcb, dx, 0);
            } else {
                double dx = bboxparent[0] - pbcb[0];
                dx += -cbw - cbpad + lmargin - off["left"];
                off["left"] += cbw + cbpad;
                pbcb = translated(pbcb, dx, 0);
            }
        } else {
            pbcb = anchored(shrunk(pb, info.shrink, info.fraction), info.anchor, pb);
            const double bmargin = cbpos[1] - cbbbox[1];
            if (info.location == "top") {
                double dy = bboxparent[3] - pbcb[1] + off["top"];
                dy += cbpad + bmargin;
                off["top"] += cbh + cbpad;
                pbcb = translated(pbcb, 0, dy);
            } else {
                double dy = bboxparent[1] - pbcb[1];
                dy += -cbh - cbpad + bmargin - off["bottom"];
                off["bottom"] += cbh + cbpad;
                pbcb = translated(pbcb, 0, dy);
            }
        }
        cbax.setPosition(transforms::Bbox(pbcb));
        cbax.anchor = info.anchor;
        cbax.boxAspect = info.location == "bottom" || info.location == "top" ? 1 / info.aspect : info.aspect;
        cbax.adjustable = "datalim";  // (set_box_aspect)
        cbax.aspect.reset();
    }

    // compress_fixed_aspect: false where it raises a ValueError
    bool compressFixedAspect() {
        const GridSpec* gs = nullptr;
        std::vector<double> extraw, extrah;
        for (auto& ax : figure_.axes) {
            if (!ax->subplotSpec) continue;
            ax->applyAspect();
            const SubplotSpec& sub = *ax->subplotSpec;
            if (!gs) {
                gs = sub.gridspec;
                extraw.assign(std::size_t(gs->ncols), 0.0);
                extrah.assign(std::size_t(gs->nrows), 0.0);
            } else if (sub.gridspec != gs) {
                return false;  // Cannot do compressed layout if Axes are not all from the same gridspec
            }
            const auto orig = ax->originalPosition->points(), actual = ax->position->points();
            const double dw = (orig[2] - orig[0]) - (actual[2] - actual[0]);
            const auto maximum = [](double a, double b) { return (a >= b || std::isnan(a)) ? a : b;  // np.maximum
            };
            if (dw > 0)
                for (int i = colspan(sub).start; i < colspan(sub).stop; ++i) extraw[std::size_t(i)] = maximum(extraw[std::size_t(i)], dw);
            const double dh = (orig[3] - orig[1]) - (actual[3] - actual[1]);
            if (dh > 0)
                for (int i = rowspan(sub).start; i < rowspan(sub).stop; ++i) extrah[std::size_t(i)] = maximum(extrah[std::size_t(i)], dh);
        }
        if (!gs) return false;  // Cannot do compressed layout if no Axes are part of a gridspec
        const double w = support::npSum(extraw) / 2;
        figureGrid_->editMarginMin(Left, w);
        figureGrid_->editMarginMin(Right, w);
        const double h = support::npSum(extrah) / 2;
        figureGrid_->editMarginMin(Top, h);
        figureGrid_->editMarginMin(Bottom, h);
        return true;
    }

    void resetMargins() {
        for (auto& ax : figure_.axes)
            if (ax->subplotSpec) gridOf(*ax).resetMargins();
        figureGrid_->resetMargins();
    }

    Figure& figure_;
    Renderer& renderer_;
    kiwi::Solver solver_;
    std::unique_ptr<LayoutGrid> figureGrid_;
    std::map<const GridSpec*, std::unique_ptr<LayoutGrid>> grids_;
};

}  // namespace

bool executeConstrainedLayout(Figure& figure, Renderer& renderer, const ConstrainedLayoutParams& params, std::string* error) {
    // the pads relative to the figure's size in inches (get_size_inches: the upper corner of bbox_inches)
    const auto inches = figure.bboxInches->points();
    const double width = inches[2], height = inches[3];
    Layout layout(figure, renderer);
    if (!layout.run(params.hPad / height, params.wPad / width, params.hspace, params.wspace, params.rect, params.compress, error)) {
        *error = "kiwisolver: " + *error;
        return false;
    }
    return true;
}

}  // namespace calculate_plot::engine
