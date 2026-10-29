// Derived from Matplotlib 3.11.2: lib/matplotlib/artist.py, lib/matplotlib/patches.py (Patch,
// Rectangle, Polygon, StepPatch, Wedge, Shadow), lib/matplotlib/lines.py (dash patterns), lib/matplotlib/cbook.py
// (contiguous_regions).
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
    // set_rasterized: drawn as an image by renderers that mix vector and raster output (SVG)
    bool rasterized = false;
    transforms::TransformPtr transform = std::make_shared<transforms::IdentityTransform>();
    bool transformSet = false;  // is_transform_set()
    // sticky_edges: data values autoscaling margins do not cross (bars stand on their base)
    std::vector<double> stickyX, stickyY;

protected:
    void setGcClip(GC& gc) const;  // Artist._set_gc_clip
    const rc::Params& rc_;
};

class Patch : public Artist {
public:
    explicit Patch(const rc::Params& rc);
    // set_edgecolor / set_facecolor: a colour (a name or its parts), or None for the default
    void setEdgecolor(const std::optional<colors::Spec>& color);
    void setFacecolor(const std::optional<colors::Spec>& color);
    void setLinewidth(std::optional<double> w);
    void setLinestyle(const std::string& style);
    // set_linestyle((offset, onoff)): a dash pattern (onoff None: solid); unlike 'None' it keeps the edge
    void setDashPattern(double offset, const std::optional<std::vector<double>>& onoff);
    virtual Path path() const = 0;
    virtual transforms::TransformPtr patchTransform() const { return std::make_shared<transforms::IdentityTransform>(); }
    transforms::TransformPtr fullTransform() const { return transforms::compose(patchTransform(), transform); }  // get_transform
    void draw(Renderer& renderer) override;
    // _draw_paths_with_artist_properties: the paths (each with its transform and face, or none) drawn with
    // the patch's look, in one group
    struct DrawPathArgs {
        Path path;
        transforms::Matrix transform;
        std::optional<std::vector<double>> face;
    };
    void drawPaths(Renderer& renderer, const std::vector<DrawPathArgs>& args);
    // get_window_extent: Path.get_extents of the path through get_transform() (its MOVETO and LINETO
    // vertices; Bbox.null() without any)
    transforms::Bbox windowExtent() const;
    // contains(event at (x, y)): the point in the path (each subpath from a MOVETO) through get_transform(),
    // grown by the radius _process_radius picks (the line width, or 0 without a visible edge)
    bool containsPoint(double x, double y) const;

    bool antialiased;
    std::string capstyle = "butt";
    std::string joinstyle = "miter";

    // Patch.set_alpha: the colours are made again (`isInt`: the alpha was a Python int)
    void setAlpha(std::optional<double> a, bool isInt = false);
    // What HandlerPolyCollection._update_prop sets directly: the resolved colours (no alpha applied)
    void setResolvedColors(const colors::Rgba& face, const colors::Rgba& edge) {
        facecolor_ = face;
        edgecolor_ = edge;
        faceAlphaInt_ = edgeAlphaInt_ = false;  // (rows of NumPy arrays)
        faceNumpy_ = true;
    }
    double linewidth() const { return linewidth_; }
    const std::string& linestyle() const { return linestyle_; }  // get_linestyle: a name, or "dash pattern"
    bool fill() const { return fill_; }                            // get_fill
    const colors::Rgba& edgecolorRgba() const { return edgecolor_; }  // get_edgecolor()
    const colors::Rgba& facecolorRgba() const { return facecolor_; }  // get_facecolor()
    // The colours as they were set and as they resolved, to put back what a save changes for a while
    struct Colors {
        std::optional<colors::Spec> originalEdge, originalFace;
        colors::Rgba edge, face;
    };
    Colors colorState() const { return {originalEdgecolor_, originalFacecolor_, edgecolor_, facecolor_}; }
    void setColorState(const Colors& c) {
        originalEdgecolor_ = c.originalEdge, originalFacecolor_ = c.originalFace;
        edgecolor_ = c.edge, facecolor_ = c.face;
    }
    void setFill(bool b);
    void updateFrom(const Patch& other);  // Patch.update_from, for the handle of a legend
    // set_hatch: the pattern ('/', 'x', 'o'...), or None for none; an empty one draws none either
    // (`numpy`: the pattern is a np.str_, as bar's hatches are: an SVG hatch id prints it so)
    void setHatch(const std::optional<std::string>& hatch, bool numpy = false) {
        hatch_ = hatch;
        hatchNumpy_ = numpy;
    }
    const std::optional<std::string>& hatch() const { return hatch_; }
    // What HandlerPolyCollection._update_prop sets directly: the hatch and its resolved colour
    void setResolvedHatch(const std::optional<std::string>& hatch, const colors::Rgba& color) {
        hatch_ = hatch;
        hatchNumpy_ = false;
        hatchFollowsEdge_ = false;
        hatchcolor_ = color;
    }
    // set_hatchcolor: a colour, 'edge' (the edge colour when drawn), or None for rcParams["hatch.color"]
    void setHatchcolor(const std::optional<colors::Spec>& color);
    std::vector<double> hatchcolor() const;  // get_hatchcolor()

protected:
    void applyEdgecolor();
    void applyFacecolor();
    void applyHatchcolor();
    std::optional<std::string> hatch_;
    bool hatchNumpy_ = false;
    std::optional<colors::Spec> originalHatchcolor_;
    bool hatchFollowsEdge_ = false;  // _hatch_color == 'edge'
    colors::Rgba hatchcolor_{};
    double hatchLinewidth_ = 0;     // rcParams["hatch.linewidth"] when made
    bool alphaInt_ = false;          // the alpha was given as a Python int
    bool faceAlphaInt_ = false, edgeAlphaInt_ = false;  // colors::alphaIsInt of each
    bool faceNumpy_ = false;  // the face colour is a tuple of NumPy floats
    bool fill_ = true;
    bool edgeDefault_ = false;
    std::optional<colors::Spec> originalEdgecolor_, originalFacecolor_;
    colors::Rgba edgecolor_{}, facecolor_{};
    double linewidth_ = 0;
    std::string linestyle_ = "solid";
    std::pair<double, std::optional<std::vector<double>>> unscaledDashes_{0.0, std::nullopt}, dashes_{0.0, std::nullopt};
};

// patches.Shadow: another patch's path and look, darkened (shade), half transparent, and shifted by
// (ox, oy) points.
class Shadow : public Patch {
public:
    Shadow(const rc::Params& rc, const Patch& patch, double ox, double oy, double shade = 0.7);
    Path path() const override { return patch_.path(); }
    transforms::TransformPtr patchTransform() const override;  // the patch's, then the shift
    void draw(Renderer& renderer) override;                    // _update_transform, then Patch.draw

private:
    const Patch& patch_;
    double ox_, oy_;
    double oxPixels_ = 0, oyPixels_ = 0;
};

class Rectangle : public Patch {
public:
    Rectangle(const rc::Params& rc, double x0, double y0, double width, double height, double angle = 0.0);
    // Path.unit_rectangle(), the one path all rectangles share: its _interpolation_steps are what bar (100) and
    // axhspan / axvspan (180) set last on it in the figure (`unitSteps`; 1 when not set)
    Path path() const override {
        Path p = Path::unitRectangle(rc_);
        if (unitSteps) p.interpolationSteps = *unitSteps;
        return p;
    }
    transforms::TransformPtr patchTransform() const override;

    double x0, y0, width, height, angle;
    std::shared_ptr<const int> unitSteps;
};

// patches.Polygon(xy, closed=): the vertices as given. A closed polygon gets its first vertex again at
// the end unless it is there already (then the path is closed); an unclosed one loses a last vertex
// equal to the first. `closed` is the value given: Python's None counts as not closed but is not
// equal to False (set_closed(None) then makes the vertices again).
class Polygon : public Patch {
public:
    Polygon(const rc::Params& rc, std::vector<double> xy, std::optional<bool> closed);
    Path path() const override { return path_; }
    void setClosed(bool closed);              // set_closed(bool(closed))
    void setXY(std::vector<double> xy);       // set_xy: x0, y0, x1, y1, ...
    const std::vector<double>& xy() const { return *path_.vertices; }  // get_xy: the path's vertices

private:
    std::optional<bool> closed_;
    Path path_;
};

// The baseline of a StepPatch: None, one value, or one per step.
struct StepBaseline {
    enum Kind { None, Scalar, Array } kind = Scalar;
    std::vector<double> values{0.0};
};

// StepPatch._update_path: for each run of steps whose value (and baseline) is not NaN, the edges
// repeated, the values repeated, down to the baseline at both ends (closed back along an array
// baseline; open without one); x and y swapped for horizontal steps. False with matplotlib's message.
bool stepPatchPath(const std::vector<double>& values, const std::vector<double>& edges, const StepBaseline& baseline,
                   bool vertical, const rc::Params& rc, Path* out, std::string* error);

// patches.StepPatch: a PathPatch of those steps (made by stairs).
class StepPatch : public Patch {
public:
    StepPatch(const rc::Params& rc, Path path) : Patch(rc), path_(std::move(path)) {}
    Path path() const override { return path_; }

private:
    Path path_;
};

// patches.PathPatch: a patch of a given path (boxplot's boxes).
class PathPatch : public Patch {
public:
    PathPatch(const rc::Params& rc, Path path) : Patch(rc), path_(std::move(path)) {}
    Path path() const override { return path_; }

private:
    Path path_;
};

// patches.Wedge(center, r, theta1, theta2, width): the arc of the circle from theta1 to theta2 degrees,
// closed through the centre (a full circle when they are 360 apart), or an annulus `width` wide.
class Wedge : public Patch {
public:
    Wedge(const rc::Params& rc, double cx, double cy, double r, double theta1, double theta2,
          std::optional<double> width = std::nullopt);
    Path path() const override { return path_; }
    // the vertices and codes of that path
    static void pathData(double cx, double cy, double r, double theta1, double theta2, std::optional<double> width,
                         std::vector<double>* vertices, std::vector<unsigned char>* codes);
    // set_center, set_radius, set_theta1, set_theta2, set_width: the path made again (a new path)
    void set(double cx, double cy, double r, double theta1, double theta2, std::optional<double> width);
    double cx, cy, r, theta1, theta2;
    std::optional<double> width;

private:
    Path path_;
};

}  // namespace calculate_plot::engine
