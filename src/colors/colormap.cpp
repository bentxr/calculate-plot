// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (Colormap, ListedColormap, Normalize)
// and lib/matplotlib/cm.py (ScalarMappable.to_rgba).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "colors/colormap.hpp"

#include "support/embedded.hpp"
#include "support/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace calculate_plot::colors {

std::optional<ListedColormap> ListedColormap::named(const std::string& name) {
    const auto doc = support::parseJson(embedded::file("data/colormaps.json"), nullptr);
    if (!doc || !doc->find("colormaps")) return std::nullopt;
    const support::Json* list = doc->find("colormaps")->find(name);
    if (!list) return std::nullopt;
    ListedColormap m;
    for (const auto& c : list->items()) {  // to_rgba_array of RGB rows: alpha 1
        Rgba rgba{0, 0, 0, 1};
        for (std::size_t k = 0; k < c.size() && k < 4; ++k) rgba[k] = std::strtod(c[k].asString().c_str(), nullptr);
        m.lut_.push_back(rgba);
    }
    // _update_lut_extremes: under and over default to the ends, bad to transparent black
    const Rgba first = m.lut_.front(), last = m.lut_.back();
    m.lut_.push_back(first);
    m.lut_.push_back(last);
    m.lut_.push_back(Rgba{0.0, 0.0, 0.0, 0.0});
    return m;
}

Rgba ListedColormap::operator()(double x, std::optional<double> alpha) const {
    const long long n = (long long)size();
    double xa = x * double(n);
    if (xa == double(n)) xa = double(n - 1);
    long long index;
    if (std::isnan(xa)) index = n + 2;   // bad
    else if (xa < 0) index = n;          // under
    else if (xa >= double(n)) index = n + 1;  // over
    else index = (long long)xa;          // astype(int): truncation
    Rgba rgba = lut_[std::size_t(std::clamp(index, 0LL, n + 2))];
    if (alpha) {
        rgba[3] = std::clamp(*alpha, 0.0, 1.0);
        if (lut_.back()[3] == 0 && lut_.back()[0] == 0 && lut_.back()[1] == 0 && lut_.back()[2] == 0 && std::isnan(xa))
            rgba = Rgba{0, 0, 0, 0};  // (lut[-1] == 0).all(): bad stays transparent
    }
    return rgba;
}

void Normalize::autoscaleNone(const std::vector<double>& data) {
    if (data.empty()) return;
    // A.min() / A.max() (NaN propagates)
    double mn = data[0], mx = data[0];
    for (double v : data) {
        if (std::isnan(v) || std::isnan(mn)) mn = std::nan("");
        else if (v < mn) mn = v;
        if (std::isnan(v) || std::isnan(mx)) mx = std::nan("");
        else if (v > mx) mx = v;
    }
    if (!vmin) vmin = mn;
    if (!vmax) vmax = mx;
}

std::vector<double> Normalize::operator()(const std::vector<double>& data) {
    std::vector<double> result = data;
    if (!vmin || !vmax) autoscaleNone(result);
    const double lo = *vmin, hi = *vmax;
    if (lo == hi) {
        std::fill(result.begin(), result.end(), 0.0);
    } else {
        for (double& v : result) {
            if (clip) v = std::clamp(v, lo, hi);
            v -= lo;
            v /= (hi - lo);
        }
    }
    return result;
}

std::vector<Rgba> mapColors(const std::vector<double>& data, Normalize& norm, const ListedColormap& cmap,
                            std::optional<double> alpha) {
    std::vector<Rgba> out;
    for (double v : norm(data)) out.push_back(cmap(v, alpha));
    return out;
}

}  // namespace calculate_plot::colors
