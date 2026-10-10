// Derived from Matplotlib 3.11.2: lib/matplotlib/collections.py (Collection, PathCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colormap.hpp"
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

// What a kind of collection uses when given nothing (the _get_default_* methods, _edge_default).
struct CollectionDefaults {
    std::string facecolorKey = "patch.facecolor";  // empty: 'none'
    std::string edgecolorKey = "patch.edgecolor";
    bool edgeDefault = false;  // _edge_default: an edge even without force_edgecolor
    std::string linewidthKey = "patch.linewidth";
    std::string antialiasedKey = "patch.antialiased";
    double zorder = 1;
};

class Collection : public Artist {
public:
    explicit Collection(const rc::Params& rc, CollectionDefaults defaults = {});

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
    virtual DataLim dataLim() const;
    bool transformSet = false;  // is_transform_set(): otherwise add_collection gives transData

    std::vector<Path> paths;
    std::vector<transforms::Matrix> transforms;  // get_transforms()
    std::vector<std::array<double, 2>> offsets;
    transforms::TransformPtr offsetTransform;
    const Figure* figure = nullptr;
    std::string groupName = "Collection";

    // ScalarMappable: the values mapped through the norm and the colormap (set_array)
    std::optional<std::vector<double>> array;
    colors::Normalize norm;
    std::optional<colors::ListedColormap> cmap;

    std::vector<colors::Rgba> facecolors() const { return facecolors_; }
    std::vector<colors::Rgba> edgecolors() const;  // get_edgecolor: the face colours for 'face'

protected:
    virtual void beforeDraw() {}

private:
    void applyFacecolor();
    void applyEdgecolor();
    void updateScalarMappable();
    std::vector<colors::Rgba> toRgbaArray(const ColorArg& c) const;

    CollectionDefaults defaults_;
    ColorArg originalFacecolor_, originalEdgecolor_;
    std::vector<colors::Rgba> facecolors_, edgecolors_;
    bool edgeIsFace_ = false;
    std::vector<double> linewidths_;
    std::vector<std::pair<double, std::optional<std::vector<double>>>> linestyles_{{0.0, std::nullopt}};
    std::vector<bool> antialiaseds_;
};

// PolyCollection: closed polygons in the collection's transform (one offset at the origin).
class PolyCollection : public Collection {
public:
    PolyCollection(const rc::Params& rc, const std::vector<std::vector<std::array<double, 2>>>& verts);
};

// FillBetweenPolyCollection('x', t, f1, f2, where=): the regions between two curves.
class FillBetweenPolyCollection : public PolyCollection {
public:
    FillBetweenPolyCollection(const rc::Params& rc, const std::vector<double>& t, const std::vector<double>& f1,
                              const std::vector<double>& f2, const std::vector<bool>& where);
    DataLim dataLim() const override { return bbox_; }

private:
    static std::vector<std::vector<std::array<double, 2>>> makeVerts(const std::vector<double>& t,
                                                                     const std::vector<double>& f1,
                                                                     const std::vector<double>& f2,
                                                                     const std::vector<bool>& where);
    DataLim bbox_;
};

// LineCollection: line segments (vlines, hlines, error bars).
class LineCollection : public Collection {
public:
    LineCollection(const rc::Params& rc, const std::vector<std::vector<std::array<double, 2>>>& segments);
    DataLim dataLim() const override;  // the segments' extents (get_path_collection_extents)
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
