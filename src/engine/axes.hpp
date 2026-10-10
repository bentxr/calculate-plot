// Derived from Matplotlib 3.11.2: lib/matplotlib/axes/_base.py (_AxesBase), lib/matplotlib/axis.py
// (Tick, XTick, YTick, Axis, XAxis, YAxis) and lib/matplotlib/spines.py (Spine).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "axes/ticker.hpp"
#include "engine/artist.hpp"
#include "engine/gridspec.hpp"
#include "engine/lines.hpp"
#include "engine/text.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;
class Figure;

// A line along one side of the Axes (linear spines only).
class Spine : public Patch {
public:
    Spine(const rc::Params& rc, Axes& axes, std::string type);
    Path path() const override { return path_; }
    void draw(Renderer& renderer) override;
    // Spine.get_spine_transform, the position ('outward', 0) unless set
    transforms::TransformPtr spineTransform();

    const std::string type;

private:
    void adjustLocation();
    Axes& axes_;
    Path path_;
    bool positionSet_ = false;
};

// The visibility switches of a group of ticks (Axis._major_tick_kw).
struct TickSwitches {
    bool tick1On = true, tick2On = true, label1On = true, label2On = false;
    bool gridOn = false;
};

// Tick: its two tick marks (markers), grid line and two labels at one location.
class Tick : public Artist {
public:
    Tick(const rc::Params& rc, Axes& axes, bool isX, bool major, const TickSwitches& switches);
    void updatePosition(double loc);
    void applySwitches(const TickSwitches& switches);
    void draw(Renderer& renderer) override;
    double loc() const { return loc_; }

    Line2D tick1line, tick2line, gridline;
    Text label1, label2;

private:
    double pad() const;  // the label pad: base pad plus the part of the tick outside the Axes
    Axes& axes_;
    bool isX_, major_;
    double loc_ = 0, size_, width_, basePad_;
    std::string tickdir_;
};

class Axis : public Artist {
public:
    Axis(const rc::Params& rc, Axes& axes, bool isX);
    void draw(Renderer& renderer) override;
    std::array<double, 2> viewInterval() const;
    void setTickSwitches(const TickSwitches& switches);  // tick_params on this axis' major ticks

    Text label, offsetText;
    axes::MaxNLocator locator = axes::MaxNLocator::autoLocator();
    axes::ScalarFormatter formatter;

private:
    std::vector<Tick*> updateTicks();
    std::vector<Tick*> majorTicks(std::size_t count);
    Axes& axes_;
    bool isX_;
    TickSwitches majorTickKw_;
    std::vector<std::unique_ptr<Tick>> majorTicks_;
};

class Axes : public Artist {
public:
    Axes(const rc::Params& rc, Figure& figure, const SubplotSpec& spec);
    void draw(Renderer& renderer) override;

    Figure& figure;
    std::shared_ptr<transforms::Bbox> position;
    std::shared_ptr<transforms::TransformedBbox> bbox;
    std::shared_ptr<transforms::Bbox> viewLim, dataLim;
    std::shared_ptr<transforms::TransformWrapper> transScale;
    transforms::TransformPtr transAxes, transLimits, transData, xaxisTransform, yaxisTransform;
    std::unique_ptr<Rectangle> patch;
    std::unique_ptr<Spine> spines[4];  // left, right, bottom, top
    Spine& spine(const std::string& type);
    std::unique_ptr<Axis> xaxis, yaxis;
    std::unique_ptr<Text> title, leftTitle, rightTitle;
    double xmargin, ymargin;
    bool axisOn = true, frameOn = true;

    // get_xaxis_text1_transform(pad) and friends: the transform, then the vertical and horizontal alignment.
    struct TextTransform {
        transforms::TransformPtr transform;
        std::string va, ha;
    };
    TextTransform xaxisTextTransform(int which, double padPoints);
    TextTransform yaxisTextTransform(int which, double padPoints);

private:
    void setLimAndTransforms();
};

}  // namespace calculate_plot::engine
