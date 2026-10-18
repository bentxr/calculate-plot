// Derived from Matplotlib 3.11.2: lib/matplotlib/colors.py (Normalize, TwoSlopeNorm, CenteredNorm,
// make_norm_from_scale and the norms it makes, PowerNorm, BoundaryNorm, NoNorm), lib/matplotlib/scale.py (the
// transforms of the scale norms) and lib/matplotlib/colorizer.py (_auto_norm_from_scale).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::colors {

// The NumPy dtypes a figure file's arrays can have.
enum class DType { Bool, UInt8, Int16, UInt16, Int32, Int64, Float32, Float64 };
std::optional<DType> dtypeNamed(const std::string& name);  // "uint8", "float32", ...
const char* dtypeName(DType t);
bool isFloat(DType t);

// A NumPy masked array of one dimension: each value exact in its dtype, and its mask (empty: nomask).
struct MaskedArray {
    std::vector<double> data;
    std::vector<bool> mask;
    DType dtype = DType::Float64;
    bool masked(std::size_t i) const { return !mask.empty() && mask[i]; }
};

// What a norm is made of: the class and its keyword arguments.
struct NormSpec {
    // "Normalize", "LogNorm", "SymLogNorm", "AsinhNorm", "PowerNorm", "TwoSlopeNorm", "CenteredNorm", "BoundaryNorm",
    // "NoNorm", or "scale:<name>" for the norm a scale name makes (norm="log": linear, log, symlog, asinh, logit)
    std::string kind = "Normalize";
    std::optional<double> vmin, vmax;
    bool clip = false;
    double gamma = 1;                                 // PowerNorm
    std::optional<double> vcenter;                    // TwoSlopeNorm (required), CenteredNorm (default 0)
    std::optional<double> halfrange;                  // CenteredNorm
    std::optional<double> linthresh;                  // SymLogNorm (required)
    double linscale = 1, base = 10;                   // SymLogNorm
    double linearWidth = 1;                           // AsinhNorm
    std::vector<double> boundaries;                   // BoundaryNorm
    long long ncolors = 0;                            // BoundaryNorm
    std::string extend = "neither";                   // BoundaryNorm
};

// A norm of matplotlib.colors: maps data to [0, 1] (BoundaryNorm: to colour indices), autoscaling the limits it
// lacks from the first data it maps.
class Norm {
public:
    // The constructor's checks; on error returns false with matplotlib's message.
    static bool make(const NormSpec& spec, Norm* out, std::string* error);

    // __call__(value, clip): false with matplotlib's message when it raises, or a "not supported yet" message.
    bool operator()(const MaskedArray& value, MaskedArray* out, std::string* error, std::optional<bool> clip = std::nullopt);
    bool inverse(const MaskedArray& value, MaskedArray* out, std::string* error) const;
    // autoscale_None of data already through process_value
    void autoscaleNone(const MaskedArray& processed);
    bool scaled() const { return vmin_.has_value() && vmax_.has_value(); }
    std::optional<double> vmin() const { return vmin_; }
    std::optional<double> vmax() const { return vmax_; }
    const NormSpec& spec() const { return spec_; }
    void setVmin(std::optional<double> v);
    void setVmax(std::optional<double> v);

private:
    bool scaleCall(const MaskedArray& value, MaskedArray* out, std::string* error, bool clip);
    bool scaleTransform(const MaskedArray& value, MaskedArray* out, std::string* error) const;
    double scaleForward(double v) const;   // the scale's transform of one float64
    double scaleInverse(double v) const;
    void centeredAutoscale(const MaskedArray& processed);
    void setHalfrange(std::optional<double> halfrange);

    NormSpec spec_;
    std::optional<double> vmin_, vmax_;
    int regions_ = 0, offset_ = 0;  // BoundaryNorm
};

// Normalize.process_value: a copy as float32 (bool and integers of up to two bytes) or float64.
MaskedArray processValue(const MaskedArray& value);

}  // namespace calculate_plot::colors
