// Derived from Matplotlib 3.11.2: lib/matplotlib/gridspec.py (SubplotParams, GridSpec, SubplotSpec).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "transforms/transforms.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

// The subplots' margins and spacing, as fractions of the figure (rcParams figure.subplot.*).
struct SubplotParams {
    double left, bottom, right, top, wspace, hspace;
    static SubplotParams defaults(const rc::Params& rc);
    // SubplotParams.update: unset values stay; false (with *error) where matplotlib raises.
    bool update(std::optional<double> left, std::optional<double> bottom, std::optional<double> right,
                std::optional<double> top, std::optional<double> wspace, std::optional<double> hspace,
                std::string* error);
};

class GridSpec;

// A cell or a block of cells of a grid: num1, num2 are its first and last cells (row-major).
struct SubplotSpec {
    const GridSpec* gridspec;
    int num1, num2;
    // SubplotSpec.get_position: the block's box in figure coordinates.
    std::shared_ptr<transforms::Bbox> position(const SubplotParams& figureParams) const;
};

class GridSpec {
public:
    GridSpec(int rows, int cols) : nrows(rows), ncols(cols) {}
    SubplotSpec cell(int row, int col) const { return {this, row * ncols + col, row * ncols + col}; }
    // gs[r0:r1, c0:c1]
    SubplotSpec span(int r0, int r1, int c0, int c1) const { return {this, r0 * ncols + c0, (r1 - 1) * ncols + (c1 - 1)}; }

    struct Positions {
        std::vector<double> bottoms, tops, lefts, rights;
    };
    // GridSpec.get_grid_positions with the figure's subplot parameters and this grid's own overrides.
    Positions gridPositions(const SubplotParams& figureParams) const;

    int nrows, ncols;
    std::optional<double> left, bottom, right, top, wspace, hspace;  // overrides of the figure's
    // Empty: all 1. Python sums them (sum()), whose result depends on which are ints.
    std::vector<double> widthRatios, heightRatios;
    std::vector<bool> widthRatiosInt, heightRatiosInt;
};

}  // namespace calculate_plot::engine
