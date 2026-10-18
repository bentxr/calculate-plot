// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (the norms), lib/matplotlib/scale.py (the transforms
// of the scale norms) and lib/matplotlib/colorizer.py (_auto_norm_from_scale).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
//
// NumPy's arithmetic decides the last bits, so each step follows its type rules: an operation on a masked array
// with a Python number is done in float64 (the number becomes an array); an in-place operation on the plain data
// keeps the array's dtype, a Python number taking that dtype first and a NumPy float64 being used as it is (the
// result rounded back); unary functions keep the dtype.

#include "colors/norms.hpp"

#include "support/histogram.hpp"
#include "support/log10.hpp"
#include "support/pyformat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::colors {

namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

const std::pair<const char*, DType> kDtypes[] = {
    {"bool", DType::Bool},     {"uint8", DType::UInt8}, {"int16", DType::Int16},     {"uint16", DType::UInt16},
    {"int32", DType::Int32},   {"int64", DType::Int64}, {"float32", DType::Float32}, {"float64", DType::Float64}};

double f32(double v) { return double(float(v)); }
// a value stored into an array of the dtype (float32 rounds)
double store(DType t, double v) { return t == DType::Float32 ? f32(v) : v; }

// Python's repr of a list of floats, for BoundaryNorm's message
std::string pyList(const std::vector<double>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) out += (i ? ", " : "") + support::pythonRepr(values[i]);
    return out + "]";
}

// np.ma's min / max of the values not masked; nullopt when all are masked or there are none
std::optional<double> maskedMin(const MaskedArray& a, bool max) {
    std::optional<double> r;
    for (std::size_t i = 0; i < a.data.size(); ++i) {
        if (a.masked(i)) continue;
        const double v = a.data[i];
        if (!r || std::isnan(v)) r = v;
        else if (!std::isnan(*r) && (max ? v > *r : v < *r)) r = v;
    }
    return r;
}

bool isScaleKind(const std::string& kind) {
    return kind == "LogNorm" || kind == "SymLogNorm" || kind == "AsinhNorm" || kind.rfind("scale:", 0) == 0;
}

// the scale behind a scale norm: "linear", "log", "symlog", "asinh" or "logit"
std::string scaleOf(const std::string& kind) {
    if (kind == "LogNorm") return "log";
    if (kind == "SymLogNorm") return "symlog";
    if (kind == "AsinhNorm") return "asinh";
    return kind.substr(6);
}

}  // namespace

std::optional<DType> dtypeNamed(const std::string& name) {
    for (const auto& [n, t] : kDtypes)
        if (name == n) return t;
    return std::nullopt;
}

const char* dtypeName(DType t) {
    for (const auto& [n, d] : kDtypes)
        if (d == t) return n;
    return "";
}

bool isFloat(DType t) { return t == DType::Float32 || t == DType::Float64; }

MaskedArray processValue(const MaskedArray& value) {
    // min_scalar_type of an array is its dtype; bool and integers of up to two bytes promote with float32 to
    // float32, the others to float64
    MaskedArray r = value;
    switch (value.dtype) {
    case DType::Bool: case DType::UInt8: case DType::Int16: case DType::UInt16: case DType::Float32:
        r.dtype = DType::Float32;
        break;
    default: r.dtype = DType::Float64;
    }
    for (double& v : r.data) v = store(r.dtype, v);
    if (r.mask.empty()) r.mask.assign(r.data.size(), false);
    return r;
}

bool Norm::make(const NormSpec& spec, Norm* out, std::string* error) {
    Norm n;
    n.spec_ = spec;
    const std::string& kind = spec.kind;
    // _sanitize_extrema: the limits as Python floats
    n.vmin_ = spec.vmin;
    n.vmax_ = spec.vmax;
    if (kind == "TwoSlopeNorm") {
        if (!spec.vcenter) return *error = "TwoSlopeNorm needs vcenter", false;
        if (spec.vmax && *spec.vcenter >= *spec.vmax) return *error = "vmin, vcenter, and vmax must be in ascending order", false;
        if (spec.vmin && *spec.vcenter <= *spec.vmin) return *error = "vmin, vcenter, and vmax must be in ascending order", false;
    } else if (kind == "CenteredNorm") {
        n.spec_.vcenter = spec.vcenter.value_or(0.0);
        n.vmin_ = n.vmax_ = std::nullopt;
        n.setHalfrange(spec.halfrange);
    } else if (kind == "BoundaryNorm") {
        if (spec.clip && spec.extend != "neither") return *error = "'clip=True' is not compatible with 'extend'", false;
        if (spec.extend != "neither" && spec.extend != "both" && spec.extend != "min" && spec.extend != "max")
            return *error = "extend must be one of 'neither', 'both', 'min', 'max'", false;
        if (spec.boundaries.size() < 2)
            return *error = "You must provide at least 2 boundaries (1 region) but you passed in " + pyList(spec.boundaries), false;
        n.vmin_ = spec.boundaries.front();
        n.vmax_ = spec.boundaries.back();
        n.regions_ = int(spec.boundaries.size()) - 1;
        if (spec.extend == "min" || spec.extend == "both") ++n.regions_, n.offset_ = 1;
        if (spec.extend == "max" || spec.extend == "both") ++n.regions_;
        if (n.regions_ > spec.ncolors)
            return *error = "There are " + std::to_string(n.regions_) + " color bins including extensions, but ncolors = " +
                            std::to_string(spec.ncolors) + "; ncolors must equal or exceed the number of bins",
                   false;
    } else if (kind == "SymLogNorm" || kind == "scale:symlog") {
        // SymLogNorm(linthresh, ...) has no default; the scale's defaults are linthresh 2, base 10, linscale 1
        if (kind == "SymLogNorm" && !spec.linthresh) return *error = "SymLogNorm needs linthresh", false;
        if (kind == "scale:symlog") n.spec_.linthresh = spec.linthresh.value_or(2.0);
        if (n.spec_.base <= 1.0) return *error = "'base' must be larger than 1", false;
        if (*n.spec_.linthresh <= 0.0) return *error = "'linthresh' must be positive", false;
        if (n.spec_.linscale <= 0.0) return *error = "'linscale' must be positive", false;
    } else if (kind == "AsinhNorm" || kind == "scale:asinh") {
        if (spec.linearWidth <= 0.0) return *error = "Scale parameter 'linear_width' must be strictly positive", false;
    } else if (kind != "Normalize" && kind != "LogNorm" && kind != "PowerNorm" && kind != "NoNorm" &&
               kind != "scale:linear" && kind != "scale:log" && kind != "scale:logit") {
        return *error = "the norm " + kind + " is not supported yet", false;
    }
    *out = n;
    return true;
}

void Norm::setVmin(std::optional<double> v) {
    if (spec_.kind == "CenteredNorm") {  // the setter keeps vcenter in the middle
        if (v != vmin_) {
            vmin_ = v;
            vmax_ = v ? std::optional<double>(2 * *spec_.vcenter - *v) : std::nullopt;
        }
        return;
    }
    vmin_ = v;
}

void Norm::setVmax(std::optional<double> v) {
    if (spec_.kind == "CenteredNorm") {
        if (v != vmax_) {
            vmax_ = v;
            vmin_ = v ? std::optional<double>(2 * *spec_.vcenter - *v) : std::nullopt;
        }
        return;
    }
    vmax_ = v;
}

void Norm::setHalfrange(std::optional<double> h) {
    if (!h) {
        setVmin(std::nullopt);
        setVmax(std::nullopt);
        return;
    }
    setVmin(*spec_.vcenter - std::fabs(*h));
    setVmax(*spec_.vcenter + std::fabs(*h));
}

void Norm::centeredAutoscale(const MaskedArray& a) {
    // halfrange = max(vcenter - A.min(), A.max() - vcenter): NumPy scalars of the data's dtype, with the Python
    // vcenter taking that dtype; then vmin = vcenter - abs(halfrange) and vmax = vcenter + abs(halfrange) (still
    // NumPy scalars of that dtype) through the setters
    const auto mn = maskedMin(a, false), mx = maskedMin(a, true);
    if (!mn || !mx) return;
    const DType t = a.dtype;
    const double c = store(t, *spec_.vcenter);
    const double low = store(t, c - *mn), high = store(t, *mx - c);
    const double h = high > low ? high : low;  // Python's max keeps the first of equals
    setVmin(store(t, c - std::fabs(h)));
    setVmax(store(t, c + std::fabs(h)));
}

void Norm::autoscaleNone(const MaskedArray& a) {
    if (a.data.empty()) return;
    if (spec_.kind == "CenteredNorm") {
        if (!vmin_ || !vmax_) centeredAutoscale(a);
        return;
    }
    MaskedArray domain = a;
    if (isScaleKind(spec_.kind)) {
        // the values whose transform is finite (masked ones are not); none: np.ma.masked, whose data is 0.0
        MaskedArray t;
        std::string ignored;
        scaleTransform(a, &t, &ignored);
        domain.data.clear();
        domain.mask.clear();
        for (std::size_t i = 0; i < a.data.size(); ++i)
            if (!t.masked(i) && std::isfinite(t.data[i])) domain.data.push_back(a.data[i]);
        domain.mask.assign(domain.data.size(), false);
        if (domain.data.empty()) {
            domain.data = {0.0};
            domain.mask = {false};
        }
    }
    if (!vmin_) {
        if (auto m = maskedMin(domain, false)) setVmin(*m);
    }
    if (!vmax_) {
        if (auto m = maskedMin(domain, true)) setVmax(*m);
    }
    if (spec_.kind == "TwoSlopeNorm" && vmin_ && vmax_) {
        const double c = *spec_.vcenter;
        if (*vmin_ >= c) vmin_ = c - (*vmax_ - c);
        if (*vmax_ <= c) vmax_ = c + (c - *vmin_);
    }
}

double Norm::scaleForward(double v) const {
    const std::string scale = scaleOf(spec_.kind);
    if (scale == "log") return support::log10(v);  // LogScale(nonpositive='mask'), base 10: np.log10
    if (scale == "symlog") {
        // SymmetricalLogTransform: linear within linthresh, else logarithmic
        const double linscaleAdj = spec_.linscale / (1.0 - 1.0 / spec_.base);
        const double logBase = std::log(spec_.base);
        const double lt = *spec_.linthresh;
        const double absA = std::fabs(v);
        if (absA <= lt) return v * linscaleAdj;
        const double sign = std::isnan(v) ? v : v > 0 ? 1.0 : v < 0 ? -1.0 : 0.0;
        return sign * lt * (linscaleAdj - std::log(lt) / logBase + std::log(absA) / logBase);
    }
    if (scale == "asinh") return spec_.linearWidth * std::asinh(v / spec_.linearWidth);
    if (scale == "logit") {
        const double d = 1 - v;
        if (!(std::fabs(v) * std::numeric_limits<double>::min() < std::fabs(d))) return kNan;  // _DomainSafeDivide
        return support::log10(v / d);
    }
    return v;  // linear
}

double Norm::scaleInverse(double v) const {
    const std::string scale = scaleOf(spec_.kind);
    if (scale == "log") return std::exp(v * std::log(10.0));  // InvertedLogTransform
    if (scale == "symlog") {
        const double linscaleAdj = spec_.linscale / (1.0 - 1.0 / spec_.base);
        const double lt = *spec_.linthresh;
        const double absA = std::fabs(v);
        if (absA <= lt * linscaleAdj) return v / linscaleAdj;
        const double sign = std::isnan(v) ? v : v > 0 ? 1.0 : v < 0 ? -1.0 : 0.0;
        return sign * lt * std::exp((absA / lt - linscaleAdj) * std::log(spec_.base));
    }
    if (scale == "asinh") return spec_.linearWidth * std::sinh(v / spec_.linearWidth);
    if (scale == "logit") return 1.0 / (1 + std::pow(10.0, -v));  // LogisticTransform
    return v;
}

bool Norm::scaleTransform(const MaskedArray& value, MaskedArray* out, std::string* error) const {
    // the scale's transform of a masked array: log keeps the dtype (np.log10 of the data) and masks what is not
    // positive; symlog, asinh and logit work in float64 (masked arithmetic with Python numbers)
    const std::string scale = scaleOf(spec_.kind);
    *out = value;
    if (scale == "linear") return true;
    if (scale == "log") {
        for (std::size_t i = 0; i < value.data.size(); ++i) {
            const double v = value.data[i];
            if (v <= 0) out->mask[i] = true;  // the ufunc's domain
            out->data[i] = value.dtype == DType::Float32 ? double(support::log10f(float(v))) : support::log10(v);
        }
        return true;
    }
    if (scale == "symlog" && value.dtype == DType::Float32) {
        // np.abs(values) <= linthresh compares in float32; when all are inside the result is values * linscale_adj
        // (float64), else np.log of the float32 magnitudes, which is NumPy's own float32 logarithm
        for (std::size_t i = 0; i < value.data.size(); ++i)
            if (!value.masked(i) && !(std::fabs(value.data[i]) <= f32(*spec_.linthresh)))
                return *error = "a symlog norm of 8- or 16-bit integer or float32 data is not supported yet", false;
    }
    out->dtype = DType::Float64;
    for (std::size_t i = 0; i < value.data.size(); ++i) {
        out->data[i] = scaleForward(value.data[i]);
        if (scale == "logit" && !(value.data[i] > 0 && std::isfinite(out->data[i]))) out->mask[i] = true;
    }
    return true;
}

bool Norm::scaleCall(const MaskedArray& in, MaskedArray* out, std::string* error, bool clip) {
    MaskedArray value = processValue(in);
    if (!vmin_ || !vmax_) autoscaleNone(value);
    if (*vmin_ > *vmax_) return *error = "vmin must be less or equal to vmax", false;
    if (*vmin_ == *vmax_) {  // np.full_like(value, 0)
        *out = value;
        std::fill(out->data.begin(), out->data.end(), 0.0);
        return true;
    }
    if (clip)  // np.clip with Python limits: they take the data's dtype
        for (double& v : value.data) v = std::clamp(v, store(value.dtype, *vmin_), store(value.dtype, *vmax_));
    MaskedArray t;
    if (!scaleTransform(value, &t, error)) return false;
    const double tvmin = scaleForward(*vmin_), tvmax = scaleForward(*vmax_);
    if (!std::isfinite(tvmin) || !std::isfinite(tvmax)) return *error = "Invalid vmin or vmax", false;
    const double span = tvmax - tvmin;
    for (std::size_t i = 0; i < t.data.size(); ++i) {
        if (t.masked(i)) continue;  // masked in-place operations leave masked data alone
        double v = store(t.dtype, t.data[i] - tvmin);
        if (!(std::fabs(v) * std::numeric_limits<double>::min() < std::fabs(span))) t.mask[i] = true;  // _DomainSafeDivide
        else v = store(t.dtype, v / span);
        t.data[i] = v;
        if (!std::isfinite(v)) t.mask[i] = true;  // masked_invalid
    }
    *out = t;
    return true;
}

bool Norm::operator()(const MaskedArray& in, MaskedArray* out, std::string* error, std::optional<bool> clipArg) {
    const bool clip = clipArg.value_or(spec_.clip);
    const std::string& kind = spec_.kind;
    if (kind == "NoNorm") {
        *out = in;
        if (out->mask.empty()) out->mask.assign(out->data.size(), false);
        return true;
    }
    if (isScaleKind(kind)) return scaleCall(in, out, error, clip);
    MaskedArray r = processValue(in);
    if (kind == "BoundaryNorm") {
        const double vmin = *vmin_, vmax = *vmax_;
        const DType t = r.dtype;
        const long long ncmap = spec_.ncolors;
        const long long maxCol = clip ? ncmap - 1 : ncmap;
        MaskedArray o;
        o.dtype = DType::Int16;
        o.mask = r.mask;
        for (std::size_t i = 0; i < r.data.size(); ++i) {
            double x = r.masked(i) ? store(t, vmax + 1) : r.data[i];  // xx.filled(vmax + 1)
            if (clip) x = std::clamp(x, store(t, vmin), store(t, vmax));
            // np.digitize: searchsorted(boundaries, x, side='right'); NaN sorts last
            long long k = std::isnan(x) ? (long long)spec_.boundaries.size()
                                        : (long long)(std::upper_bound(spec_.boundaries.begin(), spec_.boundaries.end(), x) -
                                                      spec_.boundaries.begin());
            long long iret = k - 1 + offset_;
            if (ncmap > regions_) {
                if (regions_ == 1) {
                    if (iret == 0) iret = (ncmap - 1) / 2;
                } else {
                    iret = (long long)(double(ncmap - 1) / double(regions_ - 1) * double(iret));  // astype(int16)
                }
            }
            iret = (long long)(short)iret;
            if (x < store(t, vmin)) iret = -1;
            if (x >= store(t, vmax)) iret = maxCol;
            o.data.push_back(double(iret));
        }
        *out = o;
        return true;
    }
    if (kind == "TwoSlopeNorm") {
        autoscaleNone(r);
        const double c = *spec_.vcenter;
        if (!(*vmin_ <= c && c <= *vmax_)) return *error = "vmin, vcenter, vmax must increase monotonically", false;
        const std::vector<double> xp{*vmin_, c, *vmax_}, fp{0, 0.5, 1};
        MaskedArray o = r;
        o.dtype = DType::Float64;
        o.data = support::npInterp(o.data, xp, fp, -kInf, kInf);
        *out = o;
        return true;
    }
    if (kind == "PowerNorm") {
        autoscaleNone(r);
        const double vmin = *vmin_, vmax = *vmax_;
        if (vmin > vmax) return *error = "minvalue must be less than or equal to maxvalue", false;
        const DType t = r.dtype;
        if (vmin == vmax) {
            std::fill(r.data.begin(), r.data.end(), 0.0);
        } else {
            // the limits are Python floats: they take the data's dtype (and so does gamma)
            for (std::size_t i = 0; i < r.data.size(); ++i) {
                double v = r.data[i];
                if (clip) v = std::clamp(r.masked(i) ? store(t, vmax) : v, store(t, vmin), store(t, vmax));
                v = store(t, v - store(t, vmin));
                v = store(t, v / store(t, vmax - vmin));
                if (v > 0) v = t == DType::Float32 ? double(std::pow(float(v), float(spec_.gamma))) : std::pow(v, spec_.gamma);
                r.data[i] = v;
            }
        }
        *out = r;
        return true;
    }
    // Normalize and CenteredNorm
    if (!vmin_ || !vmax_) autoscaleNone(r);
    if (!vmin_ || !vmax_) return *error = "the norm has no limits: all the data are masked", false;
    // process_value(vmin): a Python float becomes float64
    const double vmin = *vmin_, vmax = *vmax_;
    if (vmin == vmax) {
        std::fill(r.data.begin(), r.data.end(), 0.0);
    } else if (vmin > vmax) {
        return *error = "minvalue must be less than or equal to maxvalue", false;
    } else {
        if (clip) {  // np.clip(result.filled(vmax), vmin, vmax) with NumPy float64 limits: the result is float64
            for (std::size_t i = 0; i < r.data.size(); ++i) r.data[i] = std::clamp(r.masked(i) ? vmax : r.data[i], vmin, vmax);
            r.dtype = DType::Float64;
        }
        const double span = vmax - vmin;
        for (double& v : r.data) {
            v = store(r.dtype, v - vmin);
            v = store(r.dtype, v / span);
        }
    }
    *out = r;
    return true;
}

bool Norm::inverse(const MaskedArray& in, MaskedArray* out, std::string* error) const {
    const std::string& kind = spec_.kind;
    if (kind == "BoundaryNorm") return *error = "BoundaryNorm is not invertible", false;
    if (kind == "NoNorm") {
        *out = in;
        return true;
    }
    if (!scaled()) return *error = isScaleKind(kind) || kind == "PowerNorm" ? "Not invertible until scaled"
                                                                           : "Not invertible until both vmin and vmax are set",
                          false;
    MaskedArray v = processValue(in);
    const double vmin = *vmin_, vmax = *vmax_;
    if (isScaleKind(kind)) {
        if (vmin > vmax) return *error = "vmin must be less or equal to vmax", false;
        const double tvmin = scaleForward(vmin), tvmax = scaleForward(vmax);
        if (!std::isfinite(tvmin) || !std::isfinite(tvmax)) return *error = "Invalid vmin or vmax", false;
        v.dtype = DType::Float64;
        for (std::size_t i = 0; i < v.data.size(); ++i) {
            const double x = v.data[i] * (tvmax - tvmin) + tvmin;
            v.data[i] = scaleInverse(x);
            // LogisticTransform: 1 / (1 + 10 ** -x) of a masked array masks the powers that overflow
            if (scaleOf(kind) == "logit" && !std::isfinite(std::pow(10.0, -x))) v.mask[i] = true;
        }
        *out = v;
        return true;
    }
    if (kind == "TwoSlopeNorm") {
        const std::vector<double> xp{0, 0.5, 1}, fp{vmin, *spec_.vcenter, vmax};
        v.dtype = DType::Float64;
        v.data = support::npInterp(v.data, xp, fp, -kInf, kInf);
        v.mask.assign(v.data.size(), false);
        *out = v;
        return true;
    }
    if (kind == "PowerNorm") {
        for (double& x : v.data) {
            if (x > 0) x = store(v.dtype, std::pow(x, 1 / spec_.gamma));
            x = store(v.dtype, x * (vmax - vmin));
            x = store(v.dtype, x + vmin);
        }
        *out = v;
        return true;
    }
    // Normalize: vmin + val * (vmax - vmin), masked arithmetic in float64
    v.dtype = DType::Float64;
    for (double& x : v.data) x = vmin + x * (vmax - vmin);
    *out = v;
    return true;
}

}  // namespace calculate_plot::colors
