// Derived from Matplotlib 3.11.2: lib/matplotlib/axes/_base.py (_AxesBase), lib/matplotlib/axis.py
// (Tick, XTick, YTick, Axis, XAxis, YAxis) and lib/matplotlib/spines.py (Spine).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "axes/log_ticker.hpp"
#include "axes/ticker.hpp"
#include "engine/artist.hpp"
#include "engine/collections.hpp"
#include "engine/gridspec.hpp"
#include "engine/lines.hpp"
#include "engine/text.hpp"

#include <array>
#include <limits>
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
    // What Axis.grid's keywords have set on the grid lines so far (each call goes to the lines there
    // are, Tick._apply_params, and the new ticks copy the first): unset is what a tick starts with
    std::optional<colors::Spec> gridColor;
    std::optional<std::string> gridLinestyle;
    std::optional<double> gridLinewidth, gridAlpha;
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
    bool isMajor;  // in Axis.majorTicks (or minorTicks)

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
    // get_view_interval: through Axes.viewLim, which first runs a pending autoscale
    std::array<double, 2> viewInterval() const;
    void setViewInterval(double vmin, double vmax);  // set_view_interval(..., ignore=True)
    // Axis._set_lim(v0, v1, emit=, auto=): `autoscale` unset keeps the autoscaling as it is; with
    // `emit` the Axes sharing this axis take the limits too.
    void setLim(double v0, double v1, std::optional<bool> autoscale, bool emit = true);
    void setTickSwitches(const TickSwitches& switches);  // tick_params on this axis' major ticks
    // set_tick_params(which='both', labelbottom=/labelleft= (first) or labeltop=/labelright=)
    void setLabelsOn(bool first, bool on);
    // The Axis the locators and formatters belong to: this one, or the one whose tickers it shares.
    Axis& tickerAxis() const;
    void setLabelText(const std::string& text, const std::string& loc);  // set_xlabel / set_ylabel
    std::vector<Tick*> updateTicks();  // Axis._update_ticks: the ticks to draw, labelled
    // Axis._get_ticklabel_bboxes: the window extents of the visible first and second labels.
    std::pair<std::vector<transforms::Bbox>, std::vector<transforms::Bbox>> ticklabelBboxes(
        const std::vector<Tick*>& ticks, Renderer& renderer);
    std::string ticksPosition() const;  // get_ticks_position: top, bottom, left, right, default, unknown
    int tickSpace() const;              // get_tick_space: how many tick labels fit along the axis
    void setMinorTickSwitches(const TickSwitches& switches);  // tick_params(which='minor')
    const TickSwitches& tickSwitches(bool major) const { return major ? majorTickKw_ : minorTickKw_; }
    // Axis.grid(visible, which, **line properties): None toggles; properties turn the grid on
    void grid(std::optional<bool> on, const std::string& which, const TickSwitches& style, bool styled);
    // Axis.get_tightbbox(renderer, for_layout_only=): the box of the tick labels, the label and the
    // offset text; nullopt when there is none.
    std::optional<transforms::Bbox> tightbbox(Renderer& renderer, bool forLayoutOnly);

    // The scale: "linear" or "log" (base 10). _set_scale sets the locators and formatters.
    void setScale(const std::string& name);
    const std::string& scale() const { return scale_; }
    transforms::TransformPtr scaleTransform() const;  // get_transform()
    double scaleForward(double v) const;               // the scale's transform of one value
    double scaleInverse(double v) const;
    double minpos() const;                             // get_minpos(): the smallest positive data
    double sharedMinpos() const;                       // the least get_minpos() of the Axes sharing the axis
    // The major locator's nonsingular and view_limits
    std::pair<double, double> locatorNonsingular(double v0, double v1);
    std::pair<double, double> locatorViewLimits(double v0, double v1);

    Text label, offsetText;
    axes::MaxNLocator locator = axes::MaxNLocator::autoLocator();
    axes::ScalarFormatter formatter;
    axes::LogLocator logLocator, logMinorLocator;
    std::optional<axes::AutoMinorLocator> minorLocator;  // of a linear scale, if rcParams["xtick.minor.visible"]
    axes::LogFormatterSciNotation logFormatter, logMinorFormatter;
    // NullFormatter() set as the major or the minor formatter: no labels, no offset text. The scale's
    // defaults replace it when the scale is set.
    bool nullMajorFormatter = false, nullMinorFormatter = false;
    // FixedLocator(locs) and FixedFormatter(seq) as the major locator and formatter (boxplot's
    // manage_ticks): every location, the i-th labelled seq[i] ('' past its end). The scale's defaults
    // replace them when the scale is set.
    std::optional<std::vector<double>> fixedLocs;
    std::optional<std::vector<std::string>> fixedLabels;
    double labelpad;
    std::string labelPosition;  // bottom/top, left/right
    bool autoscaleOn = true;

private:
    void updateLabelPosition(Renderer& renderer);
    void updateOffsetTextPosition(const std::vector<transforms::Bbox>& bboxes,
                                  const std::vector<transforms::Bbox>& bboxes2);
    std::vector<Tick*> majorTicks(std::size_t count);
    std::vector<Tick*> minorTicks(std::size_t count);
    std::string offsetString() const;
    Axes& axes_;
    bool isX_;
    std::string scale_ = "linear";
    TickSwitches majorTickKw_, minorTickKw_;
    std::vector<std::unique_ptr<Tick>> majorTicks_, minorTicks_;
};

// The Axes sharing one direction's view limits (Axes._shared_axes[name].get_siblings), in the order
// matplotlib's Grouper gives them, and the Axis their shared locators and formatters belong to.
struct SharedAxes {
    std::vector<Axes*> members;
    Axis* tickerAxis = nullptr;
};

class Axes : public Artist {
public:
    Axes(const rc::Params& rc, Figure& figure, const SubplotSpec& spec);  // a subplot: its cells' position
    // Axes(fig, rect): at a position of its own (Figure.add_axes), with no subplot spec when `spec` is unset
    Axes(const rc::Params& rc, Figure& figure, std::shared_ptr<transforms::Bbox> position,
         const std::optional<SubplotSpec>& spec);
    void draw(Renderer& renderer) override;

    Figure& figure;
    std::optional<SubplotSpec> subplotSpec;  // get_subplotspec
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
    // Axes.add_line; the line keeps its transform when it was set (axhline, axvline)
    Line2D& addLine(std::unique_ptr<Line2D> line, bool keepTransform = false);
    Patch& addPatch(std::unique_ptr<Patch> patch);  // Axes.add_patch
    // Axes.add_artist of a patch: in data coordinates, clipped to the Axes, but no data limits
    Patch& addArtist(std::unique_ptr<Patch> patch);
    Text& addText(std::unique_ptr<Text> text);          // Axes._add_text, as Axes.text does it
    // Axes.contains_point(point): the pixel point in the patch's path, grown by a contour of 1 pixel
    bool containsPoint(double x, double y) const;
    // What a new line takes from the property cycle (_get_lines' _PropCycle.getdefaults): the properties
    // of the next entry that are not among those given. With all of them given the cycle stays.
    std::vector<std::pair<std::string, support::Json>> lineDefaults(const std::vector<std::string>& given);
    // What a new polygon of fill takes from the patches' cycle (_get_patches_for_fill's
    // _PropCycle.getdefaults(kw, ignore)): the next entry less the `ignore`d properties, when `given`
    // leaves one of those unset; else nothing, and the cycle stays.
    std::vector<std::pair<std::string, support::Json>> patchDefaults(const std::vector<std::string>& given,
                                                                     const std::vector<std::string>& ignore);
    // The patches' own cycle (_get_patches_for_fill.get_next_color), used by scatter and fills.
    colors::Spec nextPatchColor() { return nextColor(&patchCycleIndex_); }
    // The lines' cycle (_get_lines.get_next_color), used by hist.
    colors::Spec nextLineColor() { return nextColor(&cycleIndex_); }
    // Axes.add_collection(autolim=True): a pending autoscale runs first, then the data limits grow
    // by the collection's and an autoscale is requested.
    Collection& addCollection(std::unique_ptr<Collection> collection);
    // vlines / hlines: add_collection(autolim=False), then the corners of the collection's data box
    // (nanmin / nanmax) grow the data limits and an autoscale is requested.
    Collection& addLinesCollection(std::unique_ptr<Collection> collection);
    void requestAutoscaleView(bool x, bool y);
    // update_datalim(xys): nothing unless some value is finite; then the data limits grow by the points
    void updateDatalim(const std::vector<std::array<double, 2>>& xys, bool updatex = true, bool updatey = true);
    void setScale(bool x, const std::string& name);  // set_xscale / set_yscale
    void unstaleViewLim();                            // what reading Axes.viewLim does first
    void clearStale(bool x) { (x ? staleX_ : staleY_) = false; }
    // Axes.sharex / sharey: this Axes joins `other`'s group, uses its tickers and takes its limits.
    void share(bool x, Axes& other);
    const SharedAxes& shared(bool x) const { return *shared_[x ? 0 : 1]; }
    // _label_outer_xaxis / _label_outer_yaxis: the tick labels and offset texts of the inner sides go.
    void labelOuter(bool x);
    std::array<double, 2> minpos{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    std::vector<std::unique_ptr<Artist>> children;  // _children: lines, ...
    std::unique_ptr<Artist> legend;                  // legend_
    // The errorbar calls' containers, which a legend lists under their labels
    std::vector<ErrorbarContainer> containers;

    // get_tightbbox(renderer, for_layout_only=): the box of the Axes with its decorations; nullopt
    // when the Axes is invisible.
    std::optional<transforms::Bbox> tightbbox(Renderer& renderer, bool forLayoutOnly);
    void setPosition(const transforms::BboxBase& pos);  // _set_position(pos, which='both')
    // set_aspect: a number (height of one data unit over its width), or unset for 'auto'; adjustable
    // 'box' (the Axes' box shrinks); anchor: where the shrunk box sits in the original (Bbox.coefs)
    std::optional<double> aspect;
    std::array<double, 2> anchor{0.5, 0.5};
    // apply_aspect(): the active position from the original one and the aspect
    void applyAspect();
    std::shared_ptr<transforms::Bbox> originalPosition;  // get_position(original=True)

    // Axes.set_title(label, loc=, pad=, y=)
    Text& setTitle(const std::string& label, const std::string& loc, std::optional<double> pad, std::optional<double> y);

private:
    void setLimAndTransforms();
    void setTitleOffsetTrans(double points);
    void updateTitlePosition(Renderer& renderer);
    void autoscaleView(bool scalex, bool scaley);
    void updateTransScale();
    bool autotitlepos_ = true;
    bool ignoreExistingDataLimits_ = true;
    bool staleX_ = false, staleY_ = false;
    std::shared_ptr<SharedAxes> shared_[2];  // x, y
    std::size_t cycleIndex_ = 0, patchCycleIndex_ = 0;
    colors::Spec nextColor(std::size_t* index);
    void updateFromPath(const std::vector<double>& xy, bool updatex, bool updatey);
};

}  // namespace calculate_plot::engine
