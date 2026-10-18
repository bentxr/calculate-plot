// Derived from Matplotlib 3.11.2: lib/matplotlib/image.py (_ImageBase, AxesImage, _resample, composite_images,
// _draw_list_compositing_images), lib/matplotlib/colorizer.py (to_rgba of images) and lib/matplotlib/cbook.py
// (safe_masked_invalid).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colormap.hpp"
#include "colors/norms.hpp"
#include "engine/artist.hpp"
#include "support/json.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;

// A NumPy array of two dimensions (rows x cols) or three (rows x cols x channels): its values row by row, each
// exact in its dtype, and its mask (empty: nomask).
struct ImageArray {
    int rows = 0, cols = 0, channels = 0;  // channels 0: two dimensions
    colors::DType dtype = colors::DType::Float64;
    std::vector<double> data;
    std::vector<bool> mask;
    std::size_t size() const { return data.size(); }
    bool masked(std::size_t i) const { return !mask.empty() && mask[i]; }
    bool anyMasked() const;
};

// np.asarray of a figure file's value: nested lists of numbers (int64 when all are integers, bool when all are
// booleans, else float64), or {"$array": {"dtype", "data"}}. Arrays of one to three dimensions; false and why
// for ragged lists or values the dtype cannot hold.
bool readArray(const support::Json& v, ImageArray* out, std::string* error);

// An image in an Axes (AxesImage).
class AxesImage : public Artist {
public:
    AxesImage(const rc::Params& rc, Axes& axes);

    // set_data: _normalize_image_array (masked where not finite; RGB(A) clipped to its range and integers made
    // uint8); false with matplotlib's message
    bool setData(const ImageArray& a, std::string* error);
    const ImageArray& data() const { return a_; }
    // set_alpha of a 2-D array (a scalar alpha is Artist::alpha)
    std::optional<ImageArray> alphaArray;
    colors::Colormap cmap;
    std::shared_ptr<colors::Norm> norm;  // (a norm object may be shared by several images, as in matplotlib)
    std::string origin = "upper", interpolation = "auto", interpolationStage = "auto";
    bool filternorm = true, resample = true;
    double filterrad = 4.0;
    std::optional<std::array<double, 4>> extent;  // left, right, bottom, top
    std::array<double, 4> getExtent() const;
    double scalarAlpha() const;  // _get_scalar_alpha
    bool canComposite() const;   // can_composite
    void draw(Renderer& renderer) override;

    // make_image: the RGBA pixels, where their lower left corner goes, and the transform from the image to the
    // canvas; nullopt when nothing shows (or on an error, given to the renderer)
    struct Made {
        RgbaImage image;
        double x = 0, y = 0;
        transforms::TransformPtr transform;
    };
    std::optional<Made> makeImage(Renderer& renderer, double magnification, bool unsampled);

private:
    Axes& axes_;
    ImageArray a_;
};

// _draw_list_compositing_images: the artists in order, with adjacent images that can be put together drawn as one
// image when the renderer composites (the SVG renderer does, unless rcParams["image.composite_image"] is off).
void drawListCompositingImages(Renderer& renderer, const transforms::BboxPtr& parentBbox,
                               const std::vector<Artist*>& artists);

}  // namespace calculate_plot::engine
