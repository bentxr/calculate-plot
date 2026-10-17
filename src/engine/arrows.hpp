// Derived from Matplotlib 3.11.2: lib/matplotlib/patches.py (_Style, BoxStyle, FancyBboxPatch, ConnectionStyle,
// ArrowStyle, FancyArrowPatch), lib/matplotlib/path.py (make_compound_path, circle).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"
#include "engine/bezier.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

// A style's parameters as given: "name,key=value,..." (the values floats) and keywords (numbers or None)
using StyleArgs = std::map<std::string, std::optional<double>>;

// BoxStyle: the shape around a box (square, circle, ellipse, larrow, rarrow, darrow, round, round4,
// sawtooth, roundtooth), `pad` times the mutation size outside it.
struct BoxStyle {
    enum class Kind { Square, Circle, Ellipse, RArrow, LArrow, DArrow, Round, Round4, Sawtooth, Roundtooth };
    Kind kind = Kind::Round;
    double pad = 0.3, headWidth = 1.5, headAngle = 90;
    std::optional<double> roundingSize, toothSize;
    static bool make(const std::string& stylename, const StyleArgs& kwargs, BoxStyle* out, std::string* error);
    // __call__(x0, y0, width, height, mutation_size); false where matplotlib raises
    bool operator()(double x0, double y0, double width, double height, double mutationSize, const rc::Params& rc, Path* out,
                    std::string* error) const;
};

// patches.FancyBboxPatch(xy, width, height, boxstyle): a box drawn in a box style (a text's bbox).
class FancyBboxPatch : public Patch {
public:
    explicit FancyBboxPatch(const rc::Params& rc);
    Path path() const override;  // get_path: the style's path, the y axis squeezed by the mutation aspect and back
    void setBounds(double l, double b, double w, double h) { x = l, y = b, width = w, height = h; }
    double x = 0, y = 0, width = 1, height = 1, mutationScale = 1, mutationAspect = 1;
    BoxStyle boxStyle;
};

// ConnectionStyle: how the path from posA to posB is made (arc3, angle3, angle, arc, bar), then cut
// where it leaves patchA and enters patchB, and shrunk by shrinkA and shrinkB pixels at its ends.
struct ConnectionStyle {
    enum class Kind { Arc3, Angle3, Angle, Arc, Bar };
    Kind kind = Kind::Arc3;
    double rad = 0, angleA = 0, angleB = 0, fraction = 0.3;
    std::optional<double> armA, armB, angle;
    // ConnectionStyle(stylename, **kwargs); false with matplotlib's message
    static bool make(const std::string& stylename, const StyleArgs& kwargs, ConnectionStyle* out, std::string* error);
    // connect(posA, posB); false (with the message) where matplotlib raises
    bool connect(const bezier::Point& a, const bezier::Point& b, const rc::Params& rc, Path* out, std::string* error) const;
    // __call__(posA, posB, shrinkA, shrinkB, patchA, patchB): `inA` / `inB` say whether a point is in
    // the patch (none: no patch)
    bool operator()(const bezier::Point& a, const bezier::Point& b, double shrinkA, double shrinkB, const bezier::Inside& inA,
                    const bezier::Inside& inB, const rc::Params& rc, Path* out, std::string* error) const;
};

// ArrowStyle: what is drawn along the connection path: lines with heads or brackets ('-', '->', '<|-|>',
// ']-[', '|-|', ...), or a filled shape ('simple', 'fancy', 'wedge').
struct ArrowStyle {
    enum class Kind { Curve, Simple, Fancy, Wedge };
    Kind kind = Kind::Curve;
    std::string arrow = "-";
    double headLength = .4, headWidth = .2, widthA = 1., widthB = 1., lengthA = 0.2, lengthB = 0.2;
    std::optional<double> angleA = 0., angleB = 0., scaleA, scaleB;
    double tailWidth = .2, shrinkFactor = 0.5;
    bool beginHead = false, beginBracket = false, endHead = false, endBracket = false, fillBegin = false, fillEnd = false;
    static bool make(const std::string& stylename, const StyleArgs& kwargs, ArrowStyle* out, std::string* error);
    // __call__(path, mutation_size, linewidth, aspect_ratio): the paths and whether each is filled; `list`:
    // whether they came as a list (the curves) or as one path (the shapes). False where matplotlib raises.
    bool operator()(const Path& path, double mutationSize, double linewidth, double aspectRatio, const rc::Params& rc,
                    std::vector<Path>* paths, std::vector<bool>* fills, bool* list, std::string* error) const;

private:
    bool transmute(const Path& path, double mutationSize, double linewidth, const rc::Params& rc, std::vector<Path>* paths,
                   std::vector<bool>* fills, std::string* error) const;
};

// Path.make_compound_path: the paths one after the other (a path without codes as MOVETO, LINETO...),
// STOPs left out
Path compoundPath(const std::vector<Path>& paths, const rc::Params& rc);

// patches.FancyArrowPatch(posA, posB): an arrow between two points (display coordinates through its
// transform), drawn in display coordinates; its mutation scale, line width and shrinks are in points
// (times points_to_pixels(1) of the last draw: 1 before it).
class FancyArrowPatch : public Patch {
public:
    FancyArrowPatch(const rc::Params& rc, bezier::Point posA, bezier::Point posB);
    void setPositions(bezier::Point posA, bezier::Point posB) { posA_ = posA, posB_ = posB; }
    // the path in display coordinates; matplotlib's error (raised when it is drawn) in error()
    Path path() const override;
    void draw(Renderer& renderer) override;

    ConnectionStyle connectionStyle;
    ArrowStyle arrowStyle;
    bezier::Inside patchA, patchB;  // whether a point is in patchA / patchB (none: no patch)
    double shrinkA = 2, shrinkB = 2, mutationScale = 1, mutationAspect = 1;

private:
    bool displayPaths(std::vector<Path>* paths, std::vector<bool>* fills, bool* list, std::string* error) const;
    bezier::Point posA_, posB_;
    double dpiCor_ = 1.0;
};

}  // namespace calculate_plot::engine
