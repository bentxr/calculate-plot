// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py (GraphicsContextBase).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"
#include "transforms/transforms.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

class Patch;

// A clip path as TransformedPath.get_transformed_path_and_affine() gives it. With `patch` (a
// TransformedPatchPath), the path and its transform are the patch's when it is drawn.
struct ClipPath {
    Path path;
    transforms::TransformPtr affine;
    const Patch* patch = nullptr;
};

// GraphicsContextBase: the drawing state each draw call carries.
struct GC {
    explicit GC(const rc::Params& rc);

    double alpha = 1.0;
    bool forcedAlpha = false;  // if true, alpha overrides the A of RGBA
    bool antialiased = true;
    std::string capstyle = "butt";
    std::optional<std::array<double, 4>> cliprect;  // the box's corners x0, y0, x1, y1
    std::optional<ClipPath> clippath;
    double dashOffset = 0.0;
    std::optional<std::vector<double>> dashes;
    std::string joinstyle = "round";
    double linewidth = 1.0;
    std::vector<double> rgb{0.0, 0.0, 0.0, 1.0};
    std::optional<std::string> hatch;
    bool hatchNumpy = false;  // patches keep their hatch as np.str_, collections as str (SVG ids differ)
    std::optional<std::vector<double>> hatchColor;
    double hatchLinewidth;
    // Whether the alpha of the face drawn with this context is a Python int (the hatch colour's never
    // is: set_hatch_color makes floats of it)
    bool faceAlphaInt = false;
    bool faceNumpy = false;  // the face is a tuple of NumPy floats (a legend handle's first colour)
    std::optional<std::string> url;
    std::optional<std::string> gid;
    std::optional<bool> snap;
    std::optional<std::array<double, 3>> sketch;

    void setForeground(const std::vector<double>& rgba);  // set_foreground(fg, isRGBA=True)
    void setAlpha(std::optional<double> a);
    // set_dashes; the reason where matplotlib raises (a negative value, or none that is positive)
    const char* setDashes(double offset, const std::optional<std::vector<double>>& dashList);
};

// GraphicsContextBase.set_dashes' checks: why a dash list cannot be set, or nullptr.
const char* dashListProblem(const std::optional<std::vector<double>>& dashList);

}  // namespace calculate_plot::engine
