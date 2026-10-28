// Derived from Matplotlib 3.11.2: lib/matplotlib/contour.py (ContourSet, QuadContourSet) and
// lib/matplotlib/cbook.py (_resize_sequence).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/collections.hpp"
#include "engine/image.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

// What QuadContourSet takes from Axes.contour and Axes.contourf: the grid (x and y given, 1-D as one row or
// 2-D, or made from z's shape), z, and the keywords that shape the set.
struct ContourArgs {
    bool filled = false;
    std::optional<std::vector<std::vector<double>>> x, y;
    int xDims = 0, yDims = 0;  // 1: x (or y) given in one dimension, as one row
    ImageArray z;
    std::optional<std::vector<double>> levels;  // levels as a sequence
    std::optional<long long> nlevels;           // levels as an integer: how many a locator picks (None: 7)
    std::optional<std::vector<double>> linewidths;          // one is repeated, several cycled
    std::optional<std::vector<std::string>> linestyles;     // likewise
    std::optional<std::string> negativeLinestyles;          // None: rcParams["contour.negative_linestyle"]
    std::optional<double> alpha;
    std::optional<std::string> origin;  // None, 'lower', 'upper' or 'image'
    std::optional<std::array<double, 4>> extent;
    colors::Colormap cmap;  // the colormap (rcParams["image.cmap"] for None)
    bool cmapGiven = false;
    std::optional<std::vector<colors::Rgba>> colors;  // colors=: one colour or several
    std::shared_ptr<colors::Norm> norm;                // the norm (a Normalize for None)
    bool logNorm = false;                              // the norm is a LogNorm
    std::optional<double> vmin, vmax;
    std::string extend = "neither";
    std::vector<std::optional<std::string>> hatches{std::nullopt};  // cycled over the filled regions
    std::optional<bool> antialiased, cornerMask;
    long long nchunk = 0;
    std::optional<std::string> algorithm;  // None: rcParams["contour.algorithm"]
};

// A QuadContourSet: one path of lines (or of filled regions) per level, drawn as a collection.
class ContourSet : public Collection {
public:
    // QuadContourSet(ax, *args, **kwargs) up to adding it to the Axes: the levels, the colours and the paths.
    // False and matplotlib's message when it raises, or a "not supported yet" message.
    static std::unique_ptr<ContourSet> make(const rc::Params& rc, const ContourArgs& args, std::string* error);
    // ContourSet.draw: filled regions with hatches are drawn one at a time, each with its own hatch
    void draw(Renderer& renderer) override;

    bool filled = false;
    bool logscale = false;
    std::string extend = "neither";
    std::vector<std::optional<std::string>> hatches{std::nullopt};
    std::vector<double> levels;          // levels
    std::vector<double> extendedLevels;  // _levels: with the extensions' -1e250 and 1e250 (1e-250 on a log scale)
    std::vector<double> layers;          // the levels, or the middle of each pair of them for filled contours
    double zmin = 0, zmax = 0;
    std::array<double, 2> mins{}, maxs{};  // _mins, _maxs: the grid's extent

private:
    explicit ContourSet(const rc::Params& rc);
};

}  // namespace calculate_plot::engine
