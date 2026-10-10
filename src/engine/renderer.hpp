// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py (the RendererBase interface).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/gc.hpp"
#include "engine/path.hpp"
#include "transforms/transforms.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

// What artists draw with: matplotlib's RendererBase methods.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual double dpi() const = 0;
    virtual double width() const = 0;   // the canvas, in pixels
    virtual double height() const = 0;
    virtual const rc::Params& rc() const = 0;

    virtual void openGroup(const std::string& s, const std::optional<std::string>& gid) { (void)s, (void)gid; }
    virtual void closeGroup(const std::string& s) { (void)s; }
    virtual void drawPath(const GC& gc, const Path& path, const transforms::Matrix& affine,
                          const std::optional<std::vector<double>>& face) = 0;

    GC newGc() const { return GC(rc()); }
    double pointsToPixels(double points) const { return points * dpi() / 72; }
};

}  // namespace calculate_plot::engine
