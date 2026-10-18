// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (Colormap, LinearSegmentedColormap with
// _create_lookup_table, ListedColormap), lib/matplotlib/cm.py (the registry with its reversed colormaps) and
// lib/matplotlib/colorizer.py (to_rgba of one-dimensional data).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "colors/colormap.hpp"

#include "support/embedded.hpp"
#include "support/histogram.hpp"
#include "support/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace calculate_plot::colors {

namespace {

const char* const kChannels[] = {"red", "green", "blue", "alpha"};

// data/colormaps.json, parsed once
const support::Json& registry() {
    static const std::unique_ptr<support::Json> doc = [] {
        auto parsed = support::parseJson(embedded::file("data/colormaps.json"), nullptr);
        return std::make_unique<support::Json>(parsed ? *parsed : support::Json());
    }();
    return *doc;
}

double f32(double v) { return double(float(v)); }

// _create_lookup_table(N, data, gamma=1.0) of segment data: linear interpolation between the segments
std::vector<double> lookupTable(long long n, const std::vector<std::array<double, 3>>& segments) {
    std::vector<double> lut;
    if (n == 1) {
        lut = {segments.back()[1]};  // the y = f(x=1) value
    } else {
        std::vector<double> x;
        for (const auto& s : segments) x.push_back(s[0] * double(n - 1));
        // xind = (N - 1) * np.linspace(0, 1, N) ** gamma (gamma 1: the values themselves)
        std::vector<double> xind = support::npLinspace(0, 1, n);
        for (double& v : xind) v = double(n - 1) * v;
        lut.push_back(segments.front()[2]);
        for (long long i = 1; i < n - 1; ++i) {
            const double xi = xind[std::size_t(i)];
            // np.searchsorted(x, xind) (side 'left')
            const auto ind = std::size_t(std::lower_bound(x.begin(), x.end(), xi) - x.begin());
            const double distance = (xi - x[ind - 1]) / (x[ind] - x[ind - 1]);
            lut.push_back(distance * (segments[ind][1] - segments[ind - 1][2]) + segments[ind - 1][2]);
        }
        lut.push_back(segments.back()[1]);
    }
    for (double& v : lut) v = std::clamp(v, 0.0, 1.0);  // np.clip(lut, 0.0, 1.0)
    return lut;
}

}  // namespace

std::vector<std::string> Colormap::names() {
    // every registered name: those in the data file, then the reversed ones made from them
    std::vector<std::string> out;
    for (const char* kind : {"segmented", "listed"}) {
        const support::Json* maps = registry().find(kind);
        if (!maps) continue;
        for (const auto& member : maps->members()) out.push_back(member.first);
    }
    std::vector<std::string> all = out;
    for (const auto& name : out) {
        const std::string reversedName = name + "_r";
        if (std::find(out.begin(), out.end(), reversedName) == out.end() &&
            (name.size() < 2 || name.compare(name.size() - 2, 2, "_r") != 0))
            all.push_back(reversedName);
    }
    return all;
}

std::optional<Colormap> Colormap::named(const std::string& name) {
    const support::Json& doc = registry();
    Colormap m;
    m.name_ = name;
    if (const support::Json* listed = doc.find("listed") ? doc.find("listed")->find(name) : nullptr) {
        m.listed_ = true;
        for (const auto& c : listed->items()) {  // to_rgba_array of the colours
            Rgba rgba{0, 0, 0, 1};
            for (std::size_t k = 0; k < c.size() && k < 4; ++k) rgba[k] = c[k].asNumber();
            m.colors_.push_back(rgba);
        }
        m.n_ = (long long)m.colors_.size();
        m.init();
        return m;
    }
    if (const support::Json* seg = doc.find("segmented") ? doc.find("segmented")->find(name) : nullptr) {
        m.n_ = 256;  // the registry's LinearSegmentedColormap(name, spec, rcParams['image.lut'] at import)
        for (std::size_t k = 0; k < 4; ++k) {
            const support::Json* c = seg->find(kChannels[k]);
            if (!c) continue;
            Channel& ch = m.channels_[k];
            ch.present = true;
            if (c->isObject()) {
                for (const auto& v : c->find("lut")->items()) ch.lut.push_back(v.asNumber());
            } else {
                for (const auto& row : c->items()) ch.segments.push_back({row[0].asNumber(), row[1].asNumber(), row[2].asNumber()});
            }
        }
        m.init();
        return m;
    }
    if (name.size() > 2 && name.compare(name.size() - 2, 2, "_r") == 0) {
        auto base = named(name.substr(0, name.size() - 2));
        if (base && !base->hasFunctionChannels()) return base->reversed();
    }
    return std::nullopt;
}

Colormap Colormap::listed(const std::vector<Rgba>& colors, const std::string& name) {
    Colormap m;
    m.name_ = name;
    m.listed_ = true;
    m.colors_ = colors;
    m.n_ = (long long)colors.size();
    m.init();
    return m;
}

bool Colormap::hasFunctionChannels() const {
    return std::any_of(channels_.begin(), channels_.end(), [](const Channel& c) { return !c.lut.empty(); });
}

void Colormap::init() {
    lut_.assign(std::size_t(n_ + 3), Rgba{1, 1, 1, 1});
    if (listed_) {
        // self._lut = np.zeros((N + 3, 4)); self._lut[:-3] = to_rgba_array(self.colors)
        lut_.assign(std::size_t(n_ + 3), Rgba{0, 0, 0, 0});
        for (long long i = 0; i < n_; ++i) lut_[std::size_t(i)] = colors_[std::size_t(i)];
    } else {
        // np.ones((N + 3, 4)), then each channel; alpha 1 unless given
        for (std::size_t k = 0; k < 4; ++k) {
            const Channel& c = channels_[k];
            if (!c.present) continue;
            const std::vector<double> values = c.lut.empty() ? lookupTable(n_, c.segments) : c.lut;
            for (long long i = 0; i < n_; ++i) lut_[std::size_t(i)][k] = values.size() == 1 ? values[0] : values[std::size_t(i)];
        }
    }
    // _update_lut_extremes: under and over the ends, bad transparent black
    lut_[std::size_t(n_)] = lut_[0];
    lut_[std::size_t(n_ + 1)] = lut_[std::size_t(n_ - 1)];
    lut_[std::size_t(n_ + 2)] = Rgba{0, 0, 0, 0};
}

bool Colormap::resampled(long long lutsize, Colormap* out, std::string* error) const {
    if (hasFunctionChannels())
        return *error = "resampling the colormap " + name_ + " (made by functions) is not supported yet", false;
    Colormap r = *this;
    if (listed_) {
        // ListedColormap(self(np.linspace(0, 1, lutsize)))
        MaskedArray x;
        x.data = support::npLinspace(0, 1, lutsize);
        std::vector<Rgba> c;
        if (!(*this)(x, nullptr, false, &c, error)) return false;
        r.colors_ = c;
    }
    r.n_ = lutsize;
    r.init();
    *out = r;
    return true;
}

Colormap Colormap::reversed() const {
    Colormap r = *this;
    r.name_ = name_ + "_r";
    if (listed_) {
        std::reverse(r.colors_.begin(), r.colors_.end());
    } else {
        // [(1.0 - x, y1, y0) for x, y0, y1 in reversed(data)]
        for (Channel& c : r.channels_) {
            std::vector<std::array<double, 3>> rev;
            for (auto it = c.segments.rbegin(); it != c.segments.rend(); ++it) rev.push_back({1.0 - (*it)[0], (*it)[2], (*it)[1]});
            c.segments = rev;
        }
    }
    r.init();
    return r;
}

bool Colormap::operator()(const MaskedArray& x, const std::vector<double>* alpha, bool bytes, std::vector<Rgba>* out,
                          std::string* error) const {
    const std::size_t count = x.data.size();
    if (alpha && alpha->size() != 1 && alpha->size() != count)
        return *error = "alpha is array-like but its shape (" + std::to_string(alpha->size()) +
                        ",) does not match that of X (" + std::to_string(count) + ",)",
               false;
    const bool isFloat = x.dtype == DType::Float32 || x.dtype == DType::Float64;
    const bool anyMasked = std::any_of(x.mask.begin(), x.mask.end(), [](bool b) { return b; });  // np.ma.is_masked
    const double n = double(n_);
    std::vector<Rgba> lut = lut_;
    if (bytes)  // (lut * 255).astype(np.uint8)
        for (Rgba& row : lut)
            for (double& v : row) v = std::trunc(v * 255);
    const bool badIsZero = lut.back() == Rgba{0, 0, 0, 0};
    out->clear();
    for (std::size_t i = 0; i < count; ++i) {
        double xa = x.data[i];
        if (isFloat) {
            xa = x.dtype == DType::Float32 ? f32(xa * n) : xa * n;  // xa *= N (N takes float32 there)
            if (xa == n) xa = n - 1;
        }
        const bool under = xa < 0, over = xa >= n;
        const bool bad = anyMasked ? x.masked(i) : std::isnan(xa);
        // xa.astype(int): truncation; NaN (and what does not fit) becomes the smallest int64
        long long index = std::isnan(xa) || std::fabs(xa) >= 9.2e18 ? INT64_MIN : (long long)xa;
        if (under) index = n_;
        if (over) index = n_ + 1;
        if (bad) index = n_ + 2;
        index = std::clamp(index, 0LL, n_ + 2);  // take(mode='clip')
        Rgba rgba = lut[std::size_t(index)];
        if (alpha) {
            double a = std::clamp((*alpha)[alpha->size() == 1 ? 0 : i], 0.0, 1.0);
            if (bytes) a = std::trunc(a * 255);  // cast to uint8 on assignment
            rgba[3] = a;
            if (badIsZero && bad) rgba = Rgba{0, 0, 0, 0};  // the bad colour is all zeros: alpha is ignored
        }
        out->push_back(rgba);
    }
    return true;
}

bool mapColors(const MaskedArray& data, Norm& norm, const Colormap& cmap, std::optional<double> alpha,
               std::vector<Rgba>* out, std::string* error) {
    MaskedArray normed;
    if (!norm(data, &normed, error)) return false;
    std::vector<double> a;
    if (alpha) a = {*alpha};
    return cmap(normed, alpha ? &a : nullptr, false, out, error);
}

}  // namespace calculate_plot::colors
