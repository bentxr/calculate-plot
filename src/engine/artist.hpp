// Derived from Matplotlib 3.11.2: lib/matplotlib/artist.py, lib/matplotlib/patches.py (Patch,
// Rectangle), lib/matplotlib/lines.py (dash patterns).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "colors/colors.hpp"
#include "engine/renderer.hpp"
#include "transforms/transforms.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::engine {

// The dash pattern of a named line style ((offset, dashes)), unscaled; lines._get_dash_pattern.
std::pair<double, std::optional<std::vector<double>>> dashPattern(const std::string& style, const rc::Params& rc);
// lines._scale_dashes: dashes scaled by the line width when rcParams["lines.scale_dashes"].
std::pair<double, std::optional<std::vector<double>>> scaleDashes(
    const std::pair<double, std::optional<std::vector<double>>>& pattern, double linewidth, const rc::Params& rc);

class Artist {
public:
    explicit Artist(const rc::Params& rc) : rc_(rc) {}
    virtual ~Artist() = default;
    virtual void draw(Renderer& renderer) = 0;
    const rc::Params& rc() const { return rc_; }

    bool visible = true;
    std::optional<double> alpha;
    bool clipOn = true;
    transforms::BboxPtr clipBox;
    std::optional<ClipPath> clipPath;
    std::optional<std::string> gid, url;
    std::optional<bool> snap;
    std::optional<std::array<double, 3>> sketch;
    double zorder = 0;
    std::string label;
    transforms::TransformPtr transform = std::make_shared<transforms::IdentityTransform>();

protected:
    void setGcClip(GC& gc) const;  // Artist._set_gc_clip
    const rc::Params& rc_;
};

class Patch : public Artist {
public:
    explicit Patch(const rc::Params& rc);
    void setEdgecolor(const std::optional<std::string>& color);
    void setFacecolor(const std::optional<std::string>& color);
    void setLinewidth(std::optional<double> w);
    void setLinestyle(const std::string& style);
    virtual Path path() const = 0;
    virtual transforms::TransformPtr patchTransform() const { return std::make_shared<transforms::IdentityTransform>(); }
    transforms::TransformPtr fullTransform() const { return transforms::compose(patchTransform(), transform); }  // get_transform
    void draw(Renderer& renderer) override;

    bool antialiased;
    std::string capstyle = "butt";
    std::string joinstyle = "miter";

    void setAlpha(std::optional<double> a);  // Patch.set_alpha: the colours are made again
    // What HandlerPolyCollection._update_prop sets directly: the resolved colours (no alpha applied)
    void setResolvedColors(const colors::Rgba& face, const colors::Rgba& edge) {
        facecolor_ = face;
        edgecolor_ = edge;
    }
    double linewidth() const { return linewidth_; }
    void setFill(bool b);

protected:
    void applyEdgecolor();
    void applyFacecolor();
    bool fill_ = true;
    bool edgeDefault_ = false;
    std::optional<std::string> originalEdgecolor_, originalFacecolor_;
    colors::Rgba edgecolor_{}, facecolor_{};
    double linewidth_ = 0;
    std::string linestyle_ = "solid";
    std::pair<double, std::optional<std::vector<double>>> unscaledDashes_{0.0, std::nullopt}, dashes_{0.0, std::nullopt};
};

class Rectangle : public Patch {
public:
    Rectangle(const rc::Params& rc, double x0, double y0, double width, double height, double angle = 0.0);
    Path path() const override { return Path::unitRectangle(rc_); }
    transforms::TransformPtr patchTransform() const override;

    double x0, y0, width, height, angle;
};

// FancyBboxPatch with the box styles "round" (pad, rounding_size) and "square" (pad).
class FancyBboxPatch : public Patch {
public:
    FancyBboxPatch(const rc::Params& rc, double x, double y, double width, double height, std::string boxstyle,
                   double pad, std::optional<double> roundingSize, double mutationScale);
    Path path() const override;
    void setBounds(double l, double b, double w, double h) { x_ = l, y_ = b, width_ = w, height_ = h; }
    double mutationScale;
    double mutationAspect = 1;

private:
    double x_, y_, width_, height_;
    std::string boxstyle_;
    double pad_;
    std::optional<double> roundingSize_;
};

}  // namespace calculate_plot::engine
