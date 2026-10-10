// Derived from Matplotlib 3.11.2: lib/matplotlib/collections.py (Collection, PathCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Figure;

// A colour argument of a collection: None (the default), 'none', 'face', or colours.
struct ColorArg {
    enum Kind { Default, None, Face, Colors } kind = Default;
    std::vector<colors::Spec> colors;
    static ColorArg of(colors::Spec c);
};

class Collection : public Artist {
public:
    explicit Collection(const rc::Params& rc);

    void setFacecolor(const ColorArg& c);
    void setEdgecolor(const ColorArg& c);
    void setLinewidths(std::vector<double> lw);  // empty: the default (patch.linewidth)
    void setAlpha(std::optional<double> a);       // _set_alpha_for_array with a scalar
    void draw(Renderer& renderer) override;
    // get_datalim: the box of the data (offsets through the offset transform) and its minpos.
    struct DataLim {
        std::array<double, 4> points;
        std::array<double, 2> minpos;
    };
    DataLim dataLim() const;

    std::vector<Path> paths;
    std::vector<transforms::Matrix> transforms;  // get_transforms()
    std::vector<std::array<double, 2>> offsets;
    transforms::TransformPtr offsetTransform;
    const Figure* figure = nullptr;
    std::string groupName = "Collection";

    std::vector<colors::Rgba> facecolors() const { return facecolors_; }
    std::vector<colors::Rgba> edgecolors() const;  // get_edgecolor: the face colours for 'face'

protected:
    virtual void beforeDraw() {}

private:
    void applyFacecolor();
    void applyEdgecolor();
    std::vector<colors::Rgba> toRgbaArray(const ColorArg& c) const;

    ColorArg originalFacecolor_, originalEdgecolor_;
    std::vector<colors::Rgba> facecolors_, edgecolors_;
    bool edgeIsFace_ = false;
    std::vector<double> linewidths_;
    std::vector<std::pair<double, std::optional<std::vector<double>>>> linestyles_{{0.0, std::nullopt}};
    std::vector<bool> antialiaseds_;
};

// PathCollection: paths drawn at the offsets, scaled by sizes (areas in points²).
class PathCollection : public Collection {
public:
    PathCollection(const rc::Params& rc, std::vector<Path> paths, std::vector<double> sizes);
    void setSizes(const std::vector<double>& sizes, double dpi);

protected:
    void beforeDraw() override;

private:
    std::vector<double> sizes_;
};

}  // namespace calculate_plot::engine
