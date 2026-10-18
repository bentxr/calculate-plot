// Derived from Matplotlib 3.11.2: src/_image_wrapper.cpp (image_resample: the choice of pixel type and the
// parameters).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "render/resample.hpp"

#include "agg_hooks.h"  // (before any Agg header)
#include "image_resample.h"

namespace calculate_plot::render {

namespace {

const std::pair<const char*, Interpolation> kNames[] = {
    {"NEAREST", Interpolation::Nearest},   {"BILINEAR", Interpolation::Bilinear}, {"BICUBIC", Interpolation::Bicubic},
    {"SPLINE16", Interpolation::Spline16}, {"SPLINE36", Interpolation::Spline36}, {"HANNING", Interpolation::Hanning},
    {"HAMMING", Interpolation::Hamming},   {"HERMITE", Interpolation::Hermite},   {"KAISER", Interpolation::Kaiser},
    {"QUADRIC", Interpolation::Quadric},   {"CATROM", Interpolation::Catrom},     {"GAUSSIAN", Interpolation::Gaussian},
    {"BESSEL", Interpolation::Bessel},     {"MITCHELL", Interpolation::Mitchell}, {"SINC", Interpolation::Sinc},
    {"LANCZOS", Interpolation::Lanczos},   {"BLACKMAN", Interpolation::Blackman}};

}  // namespace

std::optional<Interpolation> interpolationNamed(const std::string& upperName) {
    for (const auto& [name, value] : kNames)
        if (upperName == name) return value;
    return std::nullopt;
}

std::optional<PixelType> pixelTypeOf(const std::string& dtype, bool rgba) {
    if (dtype == "uint8" || dtype == "int8") return rgba ? PixelType::Rgba8 : PixelType::Gray8;
    if (dtype == "uint16" || dtype == "int16") return rgba ? PixelType::Rgba16 : PixelType::Gray16;
    if (dtype == "float32") return rgba ? PixelType::Rgba32 : PixelType::Gray32;
    if (dtype == "float64") return rgba ? PixelType::Rgba64 : PixelType::Gray64;
    return std::nullopt;
}

bool resample(PixelType type, const void* input, int inWidth, int inHeight, void* output, int outWidth, int outHeight,
              const ResampleParams& p, std::string* error) {
    if (!p.affine && (!p.mesh || p.mesh->size() != std::size_t(outWidth) * std::size_t(outHeight) * 2))
        return *error = "Inverse transformed mesh array should have shape (" +
                        std::to_string(std::size_t(outWidth) * std::size_t(outHeight)) + ", 2)",
               false;
    resample_params_t params;
    params.interpolation = interpolation_e(int(p.interpolation));
    params.transform_mesh = nullptr;
    params.resample = p.resample;
    params.norm = p.norm;
    params.radius = float(p.radius);
    params.alpha = float(p.alpha);
    params.is_affine = p.affine.has_value();
    if (p.affine) {
        // convert_trans_affine: the first two rows of the 3x3 matrix
        const auto& m = *p.affine;
        params.affine = agg::trans_affine(m.sx, m.shy, m.shx, m.sy, m.tx, m.ty);
    } else {
        params.transform_mesh = p.mesh->data();
    }
    calculate_plot::agg_hooks::cellBlockLimitExceeded() = false;
    switch (type) {
    case PixelType::Gray8: ::resample<agg::gray8>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Gray16: ::resample<agg::gray16>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Gray32: ::resample<agg::gray32>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Gray64: ::resample<agg::gray64>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Rgba8: ::resample<agg::rgba8>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Rgba16: ::resample<agg::rgba16>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Rgba32: ::resample<agg::rgba32>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    case PixelType::Rgba64: ::resample<agg::rgba64>(input, inWidth, inHeight, output, outWidth, outHeight, params); break;
    }
    if (calculate_plot::agg_hooks::cellBlockLimitExceeded()) return *error = "Exceeded cell block limit", false;
    return true;
}

}  // namespace calculate_plot::render
