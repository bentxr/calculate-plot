// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (Colormap, ListedColormap, Normalize)
// and lib/matplotlib/cm.py (ScalarMappable.to_rgba).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colors.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::colors {

// A listed colormap by name (data/colormaps.json); nullopt for names not available yet.
class ListedColormap {
public:
    static std::optional<ListedColormap> named(const std::string& name);
    // Colormap.__call__ of normalised floats (NaN is 'bad').
    Rgba operator()(double x, std::optional<double> alpha) const;
    std::size_t size() const { return lut_.size() - 3; }

private:
    std::vector<Rgba> lut_;  // the colours, then under, over, bad
};

// Normalize with vmin and vmax autoscaled from the data when unset.
struct Normalize {
    std::optional<double> vmin, vmax;
    bool clip = false;
    void autoscaleNone(const std::vector<double>& data);
    std::vector<double> operator()(const std::vector<double>& data);
};

// ScalarMappable.to_rgba for one-dimensional float data.
std::vector<Rgba> mapColors(const std::vector<double>& data, Normalize& norm, const ListedColormap& cmap,
                            std::optional<double> alpha);

}  // namespace calculate_plot::colors
