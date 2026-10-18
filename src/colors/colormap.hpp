// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (Colormap, LinearSegmentedColormap with
// _create_lookup_table, ListedColormap), lib/matplotlib/cm.py (the registry with its reversed colormaps) and
// lib/matplotlib/colorizer.py (to_rgba of one-dimensional data).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colors.hpp"
#include "colors/norms.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::colors {

// A colormap: a lookup table of N colours, then the under, over and bad colours.
class Colormap {
public:
    // A registered colormap by name (data/colormaps.json, names ending in _r reversed); nullopt for other names.
    static std::optional<Colormap> named(const std::string& name);
    static std::vector<std::string> names();  // every registered name

    const std::string& name() const { return name_; }
    std::size_t size() const { return std::size_t(n_); }  // N
    const std::vector<Rgba>& lut() const { return lut_; }  // N + 3 rows
    bool hasFunctionChannels() const;
    // resampled(lutsize); refused for a colormap whose channels matplotlib computes with functions
    bool resampled(long long lutsize, Colormap* out, std::string* error) const;
    Colormap reversed() const;

    // __call__(X, alpha, bytes): floats in [0, 1] (scaled by N), integers as indices; alpha None (nullptr), a
    // number (one value) or one per value. Bytes give integers 0-255 (as doubles).
    bool operator()(const MaskedArray& x, const std::vector<double>* alpha, bool bytes, std::vector<Rgba>* out,
                    std::string* error) const;

private:
    struct Channel {
        bool present = false;
        std::vector<std::array<double, 3>> segments;  // x, y0, y1
        std::vector<double> lut;                      // a function's values, at N = 256 only
    };
    void init();  // _init: the table from the colours or the segments

    std::string name_;
    long long n_ = 0;
    bool listed_ = false;
    std::vector<Rgba> colors_;          // ListedColormap
    std::array<Channel, 4> channels_;   // LinearSegmentedColormap: red, green, blue, alpha
    std::vector<Rgba> lut_;
};

// ScalarMappable.to_rgba of one-dimensional data: the norm, then the colormap (alpha None or a number).
bool mapColors(const MaskedArray& data, Norm& norm, const Colormap& cmap, std::optional<double> alpha,
               std::vector<Rgba>* out, std::string* error);

}  // namespace calculate_plot::colors
