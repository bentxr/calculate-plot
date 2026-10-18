// Derived from Matplotlib 3.11.2: lib/matplotlib/colorbar.py (Colorbar, _ColorbarAxesLocator, _ColorbarSpine,
// make_axes, make_axes_gridspec) and lib/matplotlib/figure.py (Figure.colorbar).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colormap.hpp"
#include "colors/norms.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Artist;
class Axes;
class Figure;

// What a colorbar reads of its mappable (an image, a mesh, a scatter): its colormap and norm (the same objects:
// the colorbar sets the norm's limits), the data the norm scales to (get_array), and its alpha as a number.
struct Mappable {
    Artist* artist = nullptr;
    Axes* axes = nullptr;
    const colors::Colormap* cmap = nullptr;
    std::shared_ptr<colors::Norm> norm;
    std::optional<colors::MaskedArray> array;
    std::optional<double> alpha;
};

// Figure.colorbar's keywords (the Colorbar's and make_axes')
struct ColorbarOptions {
    std::optional<std::string> location, orientation, extend;
    std::string ticklocation = "auto", spacing = "uniform", label;
    bool extendrect = false, drawedges = false;
    bool extendfracAuto = false;                       // extendfrac='auto'
    std::optional<std::array<double, 2>> extendfrac;  // a number (twice) or a pair
    std::optional<std::vector<double>> ticks;
    double fraction = 0.15, shrink = 1.0, aspect = 20;
    std::optional<double> pad;
    std::optional<std::array<double, 2>> anchor, panchor;
    bool panchorFalse = false;  // panchor=False: the parents keep their anchor
};

// Figure.colorbar(mappable, cax=None, ax=None, use_gridspec=True, **kwargs): the colorbar's Axes made beside the
// parents (make_axes_gridspec for one subplot, else make_axes), then the colorbar drawn in it. False and why.
bool makeColorbar(Figure& fig, const Mappable& mappable, Axes* cax, const std::vector<Axes*>& parents, bool useGridspec,
                  const ColorbarOptions& options, std::string* error);

}  // namespace calculate_plot::engine
