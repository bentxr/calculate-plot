// Derived from Matplotlib 3.11.2: src/_image_wrapper.cpp (image_resample: the choice of pixel type and the
// parameters).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "transforms/transforms.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::render {

// matplotlib._image's interpolation constants
enum class Interpolation {
    Nearest, Bilinear, Bicubic, Spline16, Spline36, Hanning, Hamming, Hermite, Kaiser, Quadric, Catrom, Gaussian,
    Bessel, Mitchell, Sinc, Lanczos, Blackman
};
std::optional<Interpolation> interpolationNamed(const std::string& upperName);  // "NEAREST", "BILINEAR", ...

// The arrays the resampler takes: grey (one value a pixel) or RGBA, of these dtypes (int8 and int16 are read as
// their unsigned kinds, as matplotlib's wrapper does).
enum class PixelType { Gray8, Gray16, Gray32, Gray64, Rgba8, Rgba16, Rgba32, Rgba64 };
std::optional<PixelType> pixelTypeOf(const std::string& dtype, bool rgba);  // "uint8", "float32", ...

struct ResampleParams {
    Interpolation interpolation = Interpolation::Nearest;
    // the transform from input to output pixels when it is affine; else `mesh` gives, for each output pixel
    // centre (row by row), the input point it comes from (the inverse transform of x + 0.5, y + 0.5)
    std::optional<transforms::Matrix> affine;
    const std::vector<double>* mesh = nullptr;
    bool resample = false;
    double alpha = 1;  // (taken as a C float, as the wrapper's parameter is)
    bool norm = false;
    double radius = 1;  // (a C float too)
};

// _image.resample(input, output, transform, ...): blends the resampled input into `output` (which must have the
// input's pixel type). Arrays are C-contiguous, rows from the first.
bool resample(PixelType type, const void* input, int inWidth, int inHeight, void* output, int outWidth, int outHeight,
              const ResampleParams& params, std::string* error);

}  // namespace calculate_plot::render
