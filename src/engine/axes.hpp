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
    // Spine.get_window_extent: the line's box, grown by the part of its axis' ticks outside or inside.
    transforms::Bbox windowExtent();

    const std::string type;
    class Axis* axis = nullptr;  // register_axis

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
    double size() const { return size_; }
    const std::string& tickdir() const { return tickdir_; }

    Line2D tick1line, tick2line, gridline;
    Text label1, label2;

private:
    double pad() const;  // the label pad: base pad plus the part of the tick outside the Axes
    bool isX_;
    double loc_ = 0, size_, width_, basePad_;
    std::string tickdir_;
};

class Axis : public Artist {
public:
    Axis(const rc::Params& rc, Axes& axes, bool isX);
    void draw(Renderer& renderer) override;
    std::array<double, 2> viewInterval() const;
    void setViewInterval(double vmin, double vmax);  // set_view_interval(..., ignore=True)
    void setLim(double v0, double v1, bool autoscale);  // Axis._set_lim
    void setTickSwitches(const TickSwitches& switches);  // tick_params on this axis' major ticks
    void setLabelText(const std::string& text, const std::string& loc);  // set_xlabel / set_ylabel
    std::vector<Tick*> updateTicks();  // Axis._update_ticks: the ticks to draw, labelled
    // Axis._get_ticklabel_bboxes: the window extents of the visible first and second labels.
    std::pair<std::vector<transforms::Bbox>, std::vector<transforms::Bbox>> ticklabelBboxes(
        const std::vector<Tick*>& ticks, Renderer& renderer);
    std::string ticksPosition() const;
    int tickSpace() const;  // get_tick_space: how many tick labels fit along the axis  // get_ticks_position: top, bottom, left, right, default, unknown

    Text label, offsetText;
    axes::MaxNLocator locator = axes::MaxNLocator::autoLocator();
    axes::ScalarFormatter formatter;
    double labelpad;
    std::string labelPosition;  // bottom/top, left/right
    bool autoscaleOn = true;

private:
    void updateLabelPosition(Renderer& renderer);
    void updateOffsetTextPosition(const std::vector<transforms::Bbox>& bboxes,
                                  const std::vector<transforms::Bbox>& bboxes2);
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
    bool tight = false;  // _tight (None and False alike)
    bool axisOn = true, frameOn = true;

    // get_xaxis_text1_transform(pad) and friends: the transform, then the vertical and horizontal alignment.
    struct TextTransform {
        transforms::TransformPtr transform;
        std::string va, ha;
    };
    TextTransform xaxisTextTransform(int which, double padPoints);
    TextTransform yaxisTextTransform(int which, double padPoints);

    // Axes.add_line: the line in data coordinates, clipped to the Axes, counted in the data limits.
    Line2D& addLine(std::unique_ptr<Line2D> line);
    // The next colour of the property cycle when a line has none (_PropCycle.getdefaults).
    colors::Spec nextColor();
    void requestAutoscaleView(bool x, bool y);
    std::vector<std::unique_ptr<Artist>> children;  // _children: lines, ...

    // Axes.set_title(label, loc=, pad=, y=)
    void setTitle(const std::string& label, const std::string& loc, std::optional<double> pad, std::optional<double> y);

private:
    void setLimAndTransforms();
    void setTitleOffsetTrans(double points);
    void updateTitlePosition(Renderer& renderer);
    void unstaleViewLim();
    void autoscaleView(bool scalex, bool scaley);
    bool autotitlepos_ = true;
    bool ignoreExistingDataLimits_ = true;
    bool staleX_ = false, staleY_ = false;
    std::size_t cycleIndex_ = 0;
};

}  // namespace calculate_plot::engine
