// Derived from Matplotlib 3.11.2: lib/matplotlib/axes/_base.py (_AxesBase), lib/matplotlib/axis.py
// (Tick, XTick, YTick, Axis, XAxis, YAxis) and lib/matplotlib/spines.py (Spine).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/axes.hpp"

#include "agg/path_queries.h"
#include "agg/path_text.h"
#include "engine/annotation.hpp"
#include "engine/figure.hpp"
#include "engine/inset.hpp"
#include "engine/legend.hpp"
#include "rc/rc_params.hpp"
#include "support/pyformat.hpp"
#include "support/raised.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace calculate_plot::engine {

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();

// _val_or_rc with several names: the first rcParam that is not None, else the last.
std::string rcText(const rc::Params& rc, std::initializer_list<const char*> names) {
    const char* last = nullptr;
    for (const char* n : names) {
        last = n;
        if (!rc.value(n).isNull() && n != *(names.end() - 1)) return rc.text(n);
    }
    return rc.text(last);
}
double rcNumber(const rc::Params& rc, std::initializer_list<const char*> names) {
    const char* last = nullptr;
    for (const char* n : names) {
        last = n;
        if (!rc.value(n).isNull() && n != *(names.end() - 1)) return rc.number(n);
    }
    return rc.number(last);
}

// The font size of an rcParam (Text(fontsize=rcParams[key])).
double fontSizeOf(const rc::Params& rc, const char* key) { return text::fontSizeOf(rc.value(key), rc); }

// mtransforms._interval_contains_close
bool intervalContainsClose(double a, double b, double val, double rtol = 1e-10) {
    if (a > b) std::swap(a, b);
    rtol = (b - a) * rtol;
    return a - rtol <= val && val <= b + rtol;
}

using transforms::unionOf;

// BboxBase._is_finite
bool isFiniteBox(const transforms::Bbox& b) {
    const double w = b.width(), h = b.height();
    return (w > 0 || h > 0) && w < std::numeric_limits<double>::infinity() && h < std::numeric_limits<double>::infinity();
}

}  // namespace

// --- Spine ---------------------------------------------------------------------------------------

Spine::Spine(const rc::Params& rc, Axes& axes, std::string spineType) : Patch(rc), type(std::move(spineType)), axes_(axes) {
    // linear_spine's placeholder paths; _adjust_location sets the ends at draw time.
    if (type == "left") path_ = Path::make({0.0, 0.999, 0.0, 0.999}, {}, rc);
    else if (type == "right") path_ = Path::make({1.0, 0.999, 1.0, 0.999}, {}, rc);
    else if (type == "bottom") path_ = Path::make({0.999, 0.0, 0.999, 0.0}, {}, rc);
    else path_ = Path::make({0.999, 1.0, 0.999, 1.0}, {}, rc);
    setFacecolor("none");
    setEdgecolor(rc.text("axes.edgecolor"));
    setLinewidth(rc.number("axes.linewidth"));
    capstyle = "projecting";
    zorder = 2.5;
    transform = axes.transData;
    visible = rc.boolean("axes.spines." + type);
}

transforms::TransformPtr Spine::spineTransform() {
    // ('outward', 0.0): the axis' grid transform, short-circuited
    const transforms::TransformPtr base =
        (type == "left" || type == "right") ? axes_.yaxisTransform : axes_.xaxisTransform;
    if (!positionSet_) {  // _ensure_position_is_set -> set_position
        positionSet_ = true;
        transform = base;
    }
    return base;
}

void Spine::adjustLocation() {
    axes_.unstaleViewLim();  // _get_bounds_or_viewLim reads axes.viewLim
    const auto v = axes_.viewLim->points();
    std::vector<double> vertices = *path_.vertices;
    if (type == "left" || type == "right") {
        vertices[1] = v[1];
        vertices[3] = v[3];
    } else {
        vertices[0] = v[0];
        vertices[2] = v[2];
    }
    path_.vertices = std::make_shared<std::vector<double>>(std::move(vertices));
}

transforms::Bbox Spine::windowExtent() {
    adjustLocation();
    // Patch.get_window_extent: the path's extents through the transform
    const auto t = transforms::compose(patchTransform(), transform);
    const std::vector<double>& v = *path_.vertices;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
        const auto q = t->transform(v[i], v[i + 1]);
        if (i == 0 || q[0] < x0) x0 = q[0];
        if (i == 0 || q[0] > x1) x1 = q[0];
        if (i == 0 || q[1] < y0) y0 = q[1];
        if (i == 0 || q[1] > y1) y1 = q[1];
    }
    const transforms::Bbox bb({x0, y0, x1, y1});
    if (!axis || !axis->visible) return bb;
    std::vector<transforms::Bbox> boxes{bb};
    const std::vector<Tick*> drawn = axis->updateTicks();
    // a drawn major tick, then a drawn minor tick (each group's ticks share their properties)
    const Tick* groups[2] = {nullptr, nullptr};
    for (const Tick* tick : drawn)
        if (!groups[tick->isMajor ? 0 : 1]) groups[tick->isMajor ? 0 : 1] = tick;
    for (const Tick* tick : groups) {
        if (!tick) continue;
        auto p = bb.points();
        double padout = 0.5, padin = 0.5;
        if (tick->tickdir() == "out") padout = 1, padin = 0;
        else if (tick->tickdir() == "in") padout = 0, padin = 1;
        const double dpi = axes_.figure.dpi;
        padout = padout * tick->size() / 72 * dpi;
        padin = padin * tick->size() / 72 * dpi;
        if (tick->tick1line.visible) {
            if (type == "left") p[0] = p[0] - padout, p[2] = p[2] + padin;
            else if (type == "bottom") p[1] = p[1] - padout, p[3] = p[3] + padin;
        }
        if (tick->tick2line.visible) {
            if (type == "right") p[2] = p[2] + padout, p[0] = p[0] - padin;
            else if (type == "top") p[3] = p[3] + padout, p[1] = p[1] - padout;  // (sic: padout, as matplotlib)
        }
        boxes.push_back(transforms::Bbox(p));
    }
    return unionOf(boxes);
}

void Spine::draw(Renderer& renderer) {
    adjustLocation();
    Patch::draw(renderer);
}

// --- Tick ----------------------------------------------------------------------------------------

Tick::Tick(const rc::Params& rc, Axes& axes, bool isX, bool major, const TickSwitches& sw)
    : Artist(rc),
      tick1line(rc, {}, {}),
      tick2line(rc, {}, {}),
      gridline(rc, {}, {}),
      label1(rc, kNan, kNan, ""),
      label2(rc, kNan, kNan, ""),
      isMajor(major),
      axes_(axes),
      isX_(isX) {
    const std::string name = isX ? "xtick" : "ytick";
    const std::string mm = major ? "major" : "minor";
    // the keywords set_tick_params has kept, else the rcParams
    size_ = sw.size ? *sw.size : rc.number(name + "." + mm + ".size");
    width_ = sw.width ? *sw.width : rc.number(name + "." + mm + ".width");
    basePad_ = sw.pad ? *sw.pad : rc.number(name + "." + mm + ".pad");
    const colors::Spec color = sw.color ? *sw.color : colors::Spec(rc.text(name + ".color"));
    colors::Spec labelcolor = sw.labelcolor ? *sw.labelcolor : colors::Spec(rc.text(name + ".labelcolor"));
    if (const auto* text = std::get_if<std::string>(&labelcolor); text && *text == "inherit") labelcolor = rc.text(name + ".color");
    const double labelsize = sw.labelsize ? *sw.labelsize : fontSizeOf(rc, (name + ".labelsize").c_str());
    zorder = sw.zorder ? *sw.zorder : major ? 2 + 0.01 : 2;  // Line2D.zorder (+ 0.01)

    // Tick.__init__ with the rcParams: the alpha of a colour that has an alpha channel, else
    // grid.alpha. What Axis.grid was given comes over it.
    const std::string rcGridColor = mm == "major" ? rcText(rc, {"grid.major.color", "grid.color"})
                                                  : rcText(rc, {"grid.minor.color", "grid.color"});
    const std::string gridLinestyle = sw.gridLinestyle ? *sw.gridLinestyle
                                      : mm == "major" ? rcText(rc, {"grid.major.linestyle", "grid.linestyle"})
                                                      : rcText(rc, {"grid.minor.linestyle", "grid.linestyle"});
    const double gridLinewidth = sw.gridLinewidth ? *sw.gridLinewidth
                                 : mm == "major" ? rcNumber(rc, {"grid.major.linewidth", "grid.linewidth"})
                                                 : rcNumber(rc, {"grid.minor.linewidth", "grid.linewidth"});
    colors::Spec gridColor = rcGridColor;
    double gridAlpha = 0;
    if (colors::hasAlphaChannel(rcGridColor)) {
        gridAlpha = (*colors::toRgba(rcGridColor, std::nullopt, rc, nullptr))[3];
    } else {
        gridAlpha = mm == "major" ? rcNumber(rc, {"grid.major.alpha", "grid.alpha"}) : rcNumber(rc, {"grid.minor.alpha", "grid.alpha"});
    }
    if (sw.gridColor) gridColor = *sw.gridColor;
    if (sw.gridAlpha) gridAlpha = *sw.gridAlpha;
    std::string error;
    for (Line2D* line : {&tick1line, &tick2line}) {
        line->color = color;
        line->setLinestyle("none", &error);
        line->zorder = zorder;
        line->markeredgecolor = color;
        line->markersize = size_;
        line->markeredgewidth = width_;
    }
    tick1line.visible = sw.tick1On;
    tick2line.visible = sw.tick2On;
    gridline.color = gridColor;
    gridline.alpha = gridAlpha;
    // Axis.set_clip_path(axes.patch) reaches the grid lines (copied to every tick): a clip box
    gridline.clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), axes.patch->fullTransform());
    gridline.visible = sw.gridOn;
    gridline.setLinestyle(gridLinestyle, &error);
    gridline.setLinewidth(gridLinewidth);
    gridline.setMarker("", &error);
    const auto labelRgba = *colors::toRgba(labelcolor, std::nullopt, rc, nullptr);
    for (Text* tickLabel : {&label1, &label2}) {
        tickLabel->font.size = labelsize;
        tickLabel->color = labelRgba;
        tickLabel->figure = &axes.figure;
        if (sw.labelfontfamily) tickLabel->font.family = *sw.labelfontfamily;
        tickLabel->setRotation(sw.labelrotation ? *sw.labelrotation : 0.0);
        if (sw.labelrotationMode) tickLabel->rotationMode = *sw.labelrotationMode;
    }
    label1.visible = sw.label1On;
    label2.visible = sw.label2On;
    applyTickdir(sw.tickdir);
    updatePosition(0);

    // XTick / YTick
    if (isX) {
        tick1line.setData({0}, {0});
        tick1line.transform = axes.spine("bottom").spineTransform();
        tick2line.setData({0}, {1});
        tick2line.transform = axes.spine("top").spineTransform();
        gridline.setData({0, 0}, {0, 1});
        gridline.transform = axes.xaxisTransform;
        label1.x = 0, label1.y = 0;
        label2.x = 0, label2.y = 1;
    } else {
        tick1line.setData({0}, {0});
        tick1line.transform = axes.spine("left").spineTransform();
        tick2line.setData({1}, {0});
        tick2line.transform = axes.spine("right").spineTransform();
        gridline.setData({0, 1}, {0, 0});
        gridline.transform = axes.yaxisTransform;
        label1.x = 0, label1.y = 0;
        label2.x = 1, label2.y = 0;
    }
    placeLabels();
    // (XTick.__init__ sets the alignments with the transforms: _apply_params later sets the transforms only)
    const auto t1 = isX ? axes.xaxisTextTransform(1, pad()) : axes.yaxisTextTransform(1, pad());
    const auto t2 = isX ? axes.xaxisTextTransform(2, pad()) : axes.yaxisTextTransform(2, pad());
    label1.verticalAlignment = t1.va, label1.horizontalAlignment = t1.ha;
    label2.verticalAlignment = t2.va, label2.horizontalAlignment = t2.ha;
}

void Tick::applyTickdir(const std::optional<std::string>& tickdir) {
    std::string error;
    tickdir_ = tickdir ? *tickdir : rc_.text(isX_ ? "xtick.direction" : "ytick.direction");
    if (isX_) {
        tick1line.setMarker(tickdir_ == "out" ? "tickdown" : tickdir_ == "in" ? "tickup" : "|", &error);
        tick2line.setMarker(tickdir_ == "out" ? "tickup" : tickdir_ == "in" ? "tickdown" : "|", &error);
    } else {
        tick1line.setMarker(tickdir_ == "out" ? "tickleft" : tickdir_ == "in" ? "tickright" : "_", &error);
        tick2line.setMarker(tickdir_ == "out" ? "tickright" : tickdir_ == "in" ? "tickleft" : "_", &error);
    }
}

void Tick::placeLabels() {
    label1.transform = (isX_ ? axes_.xaxisTextTransform(1, pad()) : axes_.yaxisTextTransform(1, pad())).transform;
    label2.transform = (isX_ ? axes_.xaxisTextTransform(2, pad()) : axes_.yaxisTextTransform(2, pad())).transform;
}

void Tick::applyParams(const TickParams& p) {
    if (p.gridOn) gridline.visible = *p.gridOn;
    if (p.tick1On) tick1line.visible = *p.tick1On;
    if (p.tick2On) tick2line.visible = *p.tick2On;
    if (p.label1On) label1.visible = *p.label1On;
    if (p.label2On) label2.visible = *p.label2On;
    if (p.size || p.width || p.pad || p.tickdir) {
        if (p.size) size_ = *p.size;
        if (p.width) width_ = *p.width;
        if (p.pad) basePad_ = *p.pad;
        applyTickdir(p.tickdir ? p.tickdir : std::optional<std::string>(tickdir_));
        for (Line2D* line : {&tick1line, &tick2line}) {
            line->markersize = size_;
            line->markeredgewidth = width_;
        }
        placeLabels();
    }
    for (Line2D* line : {&tick1line, &tick2line}) {
        if (p.color) line->color = *p.color, line->markeredgecolor = *p.color;
        if (p.zorder) line->zorder = *p.zorder;
    }
    if (p.zorder) zorder = *p.zorder;
    for (Text* l : {&label1, &label2}) {
        if (p.labelrotation) l->setRotation(*p.labelrotation);
        if (p.labelsize) l->font.size = *p.labelsize;
        if (p.labelcolor) l->color = *colors::toRgba(*p.labelcolor, std::nullopt, rc_, nullptr);
        if (p.labelfontfamily) l->font.family = *p.labelfontfamily;
        if (p.labelrotationMode) l->rotationMode = *p.labelrotationMode;
    }
    // the grid line's properties: a colour with an alpha channel and no alpha gives both
    std::optional<colors::Spec> gridColor = p.gridColor;
    std::optional<double> gridAlpha = p.gridAlpha;
    if (gridColor && !gridAlpha && colors::hasAlphaChannel(*gridColor)) {
        const auto rgba = *colors::toRgba(*gridColor, std::nullopt, rc_, nullptr);
        gridColor = colors::Spec(std::vector<double>{rgba[0], rgba[1], rgba[2]});
        gridAlpha = rgba[3];
    }
    std::string error;
    if (gridColor) gridline.color = *gridColor;
    if (gridAlpha) gridline.alpha = *gridAlpha;
    if (p.gridLinestyle) gridline.setLinestyle(*p.gridLinestyle, &error);
    if (p.gridLinewidth) gridline.setLinewidth(*p.gridLinewidth);
}

void TickParams::mergeInto(TickSwitches* kw) const {
    if (tick1On) kw->tick1On = *tick1On;
    if (tick2On) kw->tick2On = *tick2On;
    if (label1On) kw->label1On = *label1On;
    if (label2On) kw->label2On = *label2On;
    if (gridOn) kw->gridOn = *gridOn;
    if (size) kw->size = size;
    if (width) kw->width = width;
    if (pad) kw->pad = pad;
    if (zorder) kw->zorder = zorder;
    if (labelsize) kw->labelsize = labelsize;
    if (labelrotation) kw->labelrotation = labelrotation;
    if (tickdir) kw->tickdir = tickdir;
    if (labelrotationMode) kw->labelrotationMode = labelrotationMode;
    if (color) kw->color = color;
    if (labelcolor) kw->labelcolor = labelcolor;
    if (labelfontfamily) kw->labelfontfamily = labelfontfamily;
    if (gridColor) kw->gridColor = gridColor;
    if (gridLinestyle) kw->gridLinestyle = gridLinestyle;
    if (gridLinewidth) kw->gridLinewidth = gridLinewidth;
    if (gridAlpha) kw->gridAlpha = gridAlpha;
}

double Tick::pad() const {
    const double padding = tickdir_ == "in" ? 0.0 : tickdir_ == "inout" ? 0.5 : 1.0;
    return basePad_ + size_ * padding;
}

void Tick::updatePosition(double loc) {
    if (isX_) {
        tick1line.setXdata({loc});
        tick2line.setXdata({loc});
        gridline.setXdata({loc});
        label1.x = loc;
        label2.x = loc;
    } else {
        tick1line.setYdata({loc});
        tick2line.setYdata({loc});
        gridline.setYdata({loc});
        label1.y = loc;
        label2.y = loc;
    }
    loc_ = loc;
}

void Tick::applySwitches(const TickSwitches& sw) {
    gridline.visible = sw.gridOn;
    tick1line.visible = sw.tick1On;
    tick2line.visible = sw.tick2On;
    label1.visible = sw.label1On;
    label2.visible = sw.label2On;
}

void Tick::draw(Renderer& renderer) {
    if (!visible) return;
    const std::string name = isX_ ? "xtick" : "ytick";
    renderer.openGroup(name, gid);
    gridline.draw(renderer);
    tick1line.draw(renderer);
    tick2line.draw(renderer);
    label1.draw(renderer);
    label2.draw(renderer);
    renderer.closeGroup(name);
}

// --- Axis ----------------------------------------------------------------------------------------

Axis::Axis(const rc::Params& rc, Axes& axes, bool isX)
    : Artist(rc), label(rc, kNan, kNan, ""), offsetText(rc, kNan, kNan, ""), axes_(axes), isX_(isX) {
    label.font.size = fontSizeOf(rc, "axes.labelsize");
    label.font.weight = rc.value("axes.labelweight").isNumber() ? text::FontValue(int(rc.number("axes.labelweight")))
                                                                 : text::FontValue(rc.text("axes.labelweight"));
    label.setColor(rc.text("axes.labelcolor"));
    label.figure = offsetText.figure = &axes.figure;
    const std::string name = isX ? "xtick" : "ytick";
    std::string tickColor = rc.text(name + ".labelcolor");
    if (tickColor == "inherit") tickColor = rc.text(name + ".color");
    if (isX) {  // XAxis._init
        label.x = 0.5, label.y = 0, label.verticalAlignment = "top", label.horizontalAlignment = "center";
        label.transform = transforms::blend(axes.transAxes, std::make_shared<transforms::IdentityTransform>());
        offsetText.x = 1, offsetText.y = 0, offsetText.verticalAlignment = "top", offsetText.horizontalAlignment = "right";
        offsetText.transform = transforms::blend(axes.transAxes, std::make_shared<transforms::IdentityTransform>());
    } else {  // YAxis._init
        label.x = 0, label.y = 0.5, label.verticalAlignment = "bottom", label.horizontalAlignment = "center";
        label.setRotation("vertical");
        label.rotationMode = "anchor";
        label.transform = transforms::blend(std::make_shared<transforms::IdentityTransform>(), axes.transAxes);
        offsetText.x = 0, offsetText.y = 0.5, offsetText.verticalAlignment = "baseline";
        offsetText.horizontalAlignment = "left";
        offsetText.transform = transforms::blend(axes.transAxes, std::make_shared<transforms::IdentityTransform>());
    }
    offsetText.font.size = fontSizeOf(rc, (name + ".labelsize").c_str());
    offsetText.setColor(tickColor);
    majorTickKw_.gridOn = rc.boolean("axes.grid") &&
                          (rc.text("axes.grid.which") == "both" || rc.text("axes.grid.which") == "major");
    // Axes.set_axisbelow(rcParams["axes.axisbelow"]): under the patches (True), between them and the
    // lines ('line'), or over the lines (False)
    const support::Json& below = rc.value("axes.axisbelow");
    zorder = below.isString() ? 1.5 : below.asBool() ? 0.5 : 2.5;
    labelpad = rc.number("axes.labelpad");
    labelPosition = isX ? "bottom" : "left";
    setScale("linear");
}

void Axis::widenViewInterval(double vmin, double vmax) {
    const auto old = viewInterval();
    // min(vmin, vmax, oldmin) and the like: the first of equal values
    const auto least = [](std::initializer_list<double> v) {
        double m = *v.begin();
        for (double x : v) if (x < m) m = x;
        return m;
    };
    const auto most = [](std::initializer_list<double> v) {
        double m = *v.begin();
        for (double x : v) if (x > m) m = x;
        return m;
    };
    if (old[0] < old[1]) setViewInterval(least({vmin, vmax, old[0]}), most({vmin, vmax, old[1]}));
    else setViewInterval(most({vmin, vmax, old[0]}), least({vmin, vmax, old[1]}));
}

bool Axis::setTicks(const std::vector<double>& ticks, const std::optional<std::vector<std::string>>& labels, std::string* error) {
    Axis& owner = tickerAxis();
    if (!ticks.empty()) {
        double lo = ticks[0], hi = ticks[0];  // min(ticks), max(ticks)
        for (double v : ticks) {
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        for (Axes* sibling : axes_.shared(isX_).members) (isX_ ? *sibling->xaxis : *sibling->yaxis).widenViewInterval(lo, hi);
    }
    owner.fixedLocs = ticks;
    if (!labels) return true;
    if (labels->empty()) {  // NullFormatter
        owner.nullMajorFormatter = true;
        owner.fixedLabels.reset();
        owner.dictLabels.reset();
        return true;
    }
    if (labels->size() != ticks.size())
        return *error = "The number of FixedLocator locations (" + std::to_string(ticks.size()) +
                        "), usually from a call to set_ticks, does not match the number of labels (" + std::to_string(labels->size()) + ").",
               false;
    owner.nullMajorFormatter = false;
    owner.fixedLabels.reset();
    owner.dictLabels = std::vector<std::pair<double, std::string>>{};
    for (std::size_t i = 0; i < ticks.size(); ++i) owner.dictLabels->emplace_back(ticks[i], (*labels)[i]);
    return true;
}

void Axis::setViewInterval(double vmin, double vmax) {
    // (self.axes.viewLim: asking for it brings any pending autoscaling up to date first, of both directions)
    axes_.unstaleViewLim();
    auto p = axes_.viewLim->points();
    if (isX_) p[0] = vmin, p[2] = vmax;
    else p[1] = vmin, p[3] = vmax;
    axes_.viewLim->setPoints(p);
}

void Axis::setLim(double v0, double v1, std::optional<bool> autoscale, bool emit) {
    if (scale_ == "log" && (v0 <= 0 || v1 <= 0)) {  // non-positive limits are ignored (with a warning)
        const auto old = viewInterval();
        if (v0 <= 0) v0 = old[0];
        if (v1 <= 0) v1 = old[1];
    }
    const bool reverse = v0 > v1;
    std::tie(v0, v1) = locatorNonsingular(v0, v1);
    if (logScale()) {  // LogScale.limit_range_for_scale with this axis' own minpos
        double mp = minpos();
        if (!std::isfinite(mp)) mp = 1e-300;
        v0 = v0 <= 0 ? mp : v0;
        v1 = v1 <= 0 ? mp : v1;
    }
    if (scale_ == "logit") {  // LogitScale.limit_range_for_scale
        double mp = minpos();
        if (!std::isfinite(mp)) mp = 1e-7;
        v0 = v0 <= 0 ? mp : v0;
        v1 = v1 >= 1 ? 1 - mp : v1;
    }
    // sorted([v0, v1], reverse=reverse)
    if ((v0 > v1) != reverse && v0 != v1) std::swap(v0, v1);
    setViewInterval(v0, v1);
    const std::vector<Axes*>& siblings = axes_.shared(isX_).members;
    for (Axes* ax : siblings) ax->clearStale(isX_);  // _stale_viewlims[name] = False
    if (autoscale) autoscaleOn = *autoscale;
    if (emit)
        for (Axes* other : siblings)
            if (other != &axes_) (isX_ ? *other->xaxis : *other->yaxis).setLim(v0, v1, autoscale, false);
}

Axis& Axis::tickerAxis() const { return *axes_.shared(isX_).tickerAxis; }

void Axis::setLabelsOn(bool first, bool on) {
    for (TickSwitches* kw : {&majorTickKw_, &minorTickKw_}) (first ? kw->label1On : kw->label2On) = on;
    for (auto& t : majorTicks_) t->applySwitches(majorTickKw_);
    for (auto& t : minorTicks_) t->applySwitches(minorTickKw_);
    offsetText.visible = majorTickKw_.label1On || majorTickKw_.label2On;
}

void Axis::setDateUnits() {
    if (dateUnits) return;  // (the converter is there: nothing new)
    dateUnits = true;
    Axis& owner = tickerAxis();  // set_major_locator / set_major_formatter: the tickers this axis shares
    if (owner.fixedLocs) return;  // (isDefault_majloc: a FixedLocator someone set stays)
    owner.dateTickers = true;
    owner.dateLocator = axes::dates::AutoDateLocator();
    owner.dateLocator.intervalMultiples = rc_.boolean("date.interval_multiples");
    owner.dateFormatter = axes::dates::AutoDateFormatter();
    static const char* const keys[7] = {"year", "month", "day", "hour", "minute", "second", "microsecond"};
    for (std::size_t i = 0; i < 7; ++i) owner.dateFormatter.formats[i] = rc_.text(std::string("date.autoformatter.") + keys[i]);
}

void Axis::setScale(const std::string& name, const ScaleOptions& options) {
    scale_ = name;
    scaleOptions_ = options;
    dateTickers = false;  // (the scale's own locators and formatters replace the date ones)
    nullMajorFormatter = nullMinorFormatter = false;
    fixedLocs.reset();
    fixedLabels.reset();
    dictLabels.reset();
    // The locators and formatters keep their settings of before matplotlib 2 in the classic style
    const bool classic = rc_.boolean("_internal.classic_mode");
    if (name == "log" || name == "functionlog") {  // LogScale.set_default_locators_and_formatters (FuncScaleLog's too)
        logLocator = axes::LogLocator();
        logLocator.base = options.base;
        logLocator.classic = classic;
        if (classic) logLocator.numticks = 15;
        logMinorLocator = logLocator;
        logMinorLocator.subs.clear();
        logMinorLocator.autoSubs = true;
        if (options.subs) logMinorLocator.subs = *options.subs, logMinorLocator.autoSubs = false;
        logFormatter = axes::LogFormatterSciNotation();
        logFormatter.base = options.base;
        logFormatter.minExponent = int(rc_.number("axes.formatter.min_exponent"));
        if (classic) logFormatter.minorThresholds = {0, 0};
        logMinorFormatter = logFormatter;
        logMinorFormatter.labelOnlyBase = options.subs.has_value();
    } else if (name == "symlog") {  // SymmetricalLogScale.set_default_locators_and_formatters
        symlogLocator = axes::SymmetricalLogLocator();
        symlogLocator.base = options.base, symlogLocator.linthresh = options.linthresh;
        symlogMinorLocator = symlogLocator;
        if (options.subs) symlogMinorLocator.subs = *options.subs;
        logFormatter = axes::LogFormatterSciNotation();
        logFormatter.base = options.base;
        logFormatter.linthresh = options.linthresh;  // (set_locs takes it from the axis' transform)
        logFormatter.minExponent = int(rc_.number("axes.formatter.min_exponent"));
        if (classic) logFormatter.minorThresholds = {0, 0};
        nullMinorFormatter = true;  // NullFormatter()
    } else if (name == "logit") {  // LogitScale.set_default_locators_and_formatters
        logitLocator = axes::LogitLocator();
        logitMinorLocator = logitLocator;
        logitMinorLocator.minor = true;
        logitFormatter = axes::LogitFormatter();
        logitFormatter.oneHalf = options.oneHalf, logitFormatter.useOverline = options.useOverline;
        logitMinorFormatter = logitFormatter;
        logitMinorFormatter.minor = true;
    } else {
        locator = axes::MaxNLocator::autoLocator();
        if (classic) {
            locator.nbins = 9;
            locator.setSteps({1, 2, 5, 10});
        }
        locator.roundNumbers = rc_.text("axes.autolimit_mode") == "round_numbers";
        // ScalarFormatter()
        axes::ScalarFormatter::Settings settings;
        settings.useOffset = rc_.boolean("axes.formatter.useoffset");
        settings.offsetThreshold = int(rc_.number("axes.formatter.offset_threshold"));
        const auto limits = rc_.numbers("axes.formatter.limits");
        settings.powerLimits = {int(limits[0]), int(limits[1])};
        settings.useMathText = rc_.boolean("axes.formatter.use_mathtext");
        settings.unicodeMinus = rc_.boolean("axes.unicode_minus");
        formatter = axes::ScalarFormatter(settings);
        // the minor locator: AutoMinorLocator() if rcParams["xtick.minor.visible"], else NullLocator()
        const std::string tick = isX_ ? "xtick" : "ytick";
        if (rc_.boolean(tick + ".minor.visible")) {
            minorLocator = axes::AutoMinorLocator();
            const auto& ndivs = rc_.value(tick + ".minor.ndivs");  // 'auto', or the divisions
            if (ndivs.isNumber()) minorLocator->ndivs = int(ndivs.asNumber());
        }
    }
}

transforms::TransformPtr Axis::scaleTransform() const {
    if (scale_ == "function") return std::make_shared<transforms::FuncTransform>(scaleOptions_.forward, scaleOptions_.inverse);
    if (scale_ == "functionlog")  // FuncTransform(forward, inverse) + LogTransform(base)
        return transforms::compose(std::make_shared<transforms::FuncTransform>(scaleOptions_.forward, scaleOptions_.inverse),
                                   std::make_shared<transforms::LogTransform>(scaleOptions_.base));
    if (logScale()) return std::make_shared<transforms::LogTransform>(scaleOptions_.base, scaleOptions_.logClip);
    if (scale_ == "symlog")
        return std::make_shared<transforms::SymmetricalLogTransform>(scaleOptions_.base, scaleOptions_.linthresh, scaleOptions_.linscale);
    if (scale_ == "logit") return std::make_shared<transforms::LogitTransform>(scaleOptions_.clip);
    return std::make_shared<transforms::IdentityTransform>();
}

double Axis::scaleForward(double v) const {
    if (scale_ == "function" || scale_ == "functionlog") {
        const double f = transforms::FuncTransform(scaleOptions_.forward, scaleOptions_.inverse).transform1d(v);
        return scale_ == "function" ? f : transforms::LogTransform(scaleOptions_.base).transform1d(f);
    }
    if (logScale()) return transforms::LogTransform(scaleOptions_.base, scaleOptions_.logClip).transform1d(v);
    if (scale_ == "symlog") return transforms::SymmetricalLogTransform(scaleOptions_.base, scaleOptions_.linthresh, scaleOptions_.linscale).transform1d(v);
    if (scale_ == "logit") return transforms::LogitTransform(scaleOptions_.clip).transform1d(v);
    return v;
}
double Axis::scaleInverse(double v) const {
    if (scale_ == "function" || scale_ == "functionlog") {
        const double f = scale_ == "function" ? v : transforms::LogTransform(scaleOptions_.base).inverse1d(v);
        return transforms::FuncTransform(scaleOptions_.inverse, scaleOptions_.forward).transform1d(f);
    }
    if (logScale()) return transforms::LogTransform(scaleOptions_.base).inverse1d(v);
    if (scale_ == "symlog") return transforms::SymmetricalLogTransform(scaleOptions_.base, scaleOptions_.linthresh, scaleOptions_.linscale).inverse1d(v);
    if (scale_ == "logit") return transforms::LogitTransform::inverse1d(v);
    return v;
}
double Axis::minpos() const { return axes_.minpos[isX_ ? 0 : 1]; }

double Axis::sharedMinpos() const {
    double mp = std::numeric_limits<double>::infinity();
    for (const Axes* ax : axes_.shared(isX_).members) mp = std::min(mp, ax->minpos[isX_ ? 0 : 1]);
    return mp;
}

std::pair<double, double> Axis::locatorNonsingular(double v0, double v1) {
    if (tickerAxis().dateTickers && !tickerAxis().fixedLocs) return tickerAxis().dateLocator.nonsingular(v0, v1);
    if (scale_ == "logit" && !tickerAxis().fixedLocs) {
        logitLocator.minpos = tickerAxis().minpos();  // LogitLocator.nonsingular: its axis' get_minpos() (the last to set the scale)
        return logitLocator.nonsingular(v0, v1);
    }
    // (set_ticks put a FixedLocator in the log scale's LogLocator's place)
    if (logScale() && !tickerAxis().fixedLocs) {
        logLocator.minpos = sharedMinpos();  // LogLocator.nonsingular: the least over the shared axes
        return logLocator.nonsingular(v0, v1);
    }
    return axes::nonsingular(v0, v1, 0.05);  // Locator.nonsingular
}

std::pair<double, double> Axis::locatorViewLimits(double v0, double v1) {
    if (tickerAxis().dateTickers && !tickerAxis().fixedLocs) return axes::nonsingular(v0, v1);  // Locator.view_limits
    if (logScale() && !tickerAxis().fixedLocs) {
        logLocator.minpos = sharedMinpos();
        return logLocator.viewLimits(v0, v1, rc_.text("axes.autolimit_mode") == "round_numbers");
    }
    if (scale_ == "symlog" && !tickerAxis().fixedLocs)
        return symlogLocator.viewLimits(v0, v1, rc_.text("axes.autolimit_mode") == "round_numbers");
    if (scale_ == "logit" && !tickerAxis().fixedLocs) {
        logitLocator.tickSpace = tickerAxis().tickSpace();
        return logitLocator.viewLimits(v0, v1, rc_.text("axes.autolimit_mode") == "round_numbers");
    }
    locator.tickSpace = tickerAxis().tickSpace();  // the locator's axis
    if (tickerAxis().fixedLocs) return axes::nonsingular(v0, v1);  // Locator.view_limits: _nonsingular
    return locator.viewLimits(v0, v1);
}

void Axis::grid(std::optional<bool> on, const std::string& which, const TickSwitches& style, bool styled) {
    if (styled && (!on || !*on)) on = true;  // properties turn the grid on
    const auto apply = [&](TickSwitches& kw, std::vector<std::unique_ptr<Tick>>& ticks) {
        kw.gridOn = on ? *on : !kw.gridOn;
        if (style.gridColor) kw.gridColor = style.gridColor;
        if (style.gridLinestyle) kw.gridLinestyle = style.gridLinestyle;
        if (style.gridLinewidth) kw.gridLinewidth = style.gridLinewidth;
        if (style.gridAlpha) kw.gridAlpha = style.gridAlpha;
        // existing ticks: _apply_params(gridOn=..., grid_*=...); new ones are made with the keywords.
        // (Ticks are made when drawn, so none exist yet.)
        (void)ticks;
    };
    if (which == "minor" || which == "both") apply(minorTickKw_, minorTicks_);
    if (which == "major" || which == "both") apply(majorTickKw_, majorTicks_);
}

void Axis::setTickParams(const std::string& which, bool reset, const TickParams& p) {
    for (const bool major : {true, false}) {
        if (which != "both" && which != (major ? "major" : "minor")) continue;
        TickSwitches& kw = major ? majorTickKw_ : minorTickKw_;
        auto& ticks = major ? majorTicks_ : minorTicks_;
        if (reset) {
            // _reset_major_tick_kw / _reset_minor_tick_kw: only gridOn from the rcParams, then reset_ticks
            kw = TickSwitches{};
            const std::string w = rc_.text("axes.grid.which");
            kw.gridOn = rc_.boolean("axes.grid") && (w == "both" || w == (major ? "major" : "minor"));
            p.mergeInto(&kw);
            ticks.clear();
        } else {
            p.mergeInto(&kw);
            for (auto& tick : ticks) tick->applyParams(p);
        }
    }
    if (!reset) {
        // the labels' switches and colour reach the offset text too
        if (p.label1On || p.label2On) offsetText.visible = majorTickKw_.label1On || majorTickKw_.label2On;
        if (p.labelcolor) offsetText.color = *colors::toRgba(*p.labelcolor, std::nullopt, rc_, nullptr);
    }
}

bool Axis::setTicksPosition(const std::string& position, std::string* error) {
    const std::string one = isX_ ? "bottom" : "left", two = isX_ ? "top" : "right";
    TickParams p;
    if (position == two) {
        p.tick2On = p.label2On = true, p.tick1On = p.label1On = false;
    } else if (position == one) {
        p.tick2On = p.label2On = false, p.tick1On = p.label1On = true;
    } else if (position == "both") {
        p.tick2On = p.tick1On = true;
    } else if (position == "none") {
        p.tick2On = p.tick1On = false;
    } else if (position == "default") {
        p.tick2On = true, p.label2On = false, p.tick1On = p.label1On = true;
    } else {
        return *error = support::pythonStrRepr(position) + " is not a valid value for position; supported values are '" + one + "', '" + two +
                        "', 'both', 'default', 'none'",
               false;
    }
    setTickParams("both", false, p);
    if (isX_) {
        if (position == two) tickPosition_ = "top", offsetText.verticalAlignment = "bottom";
        else if (position == one || position == "default") tickPosition_ = "bottom", offsetText.verticalAlignment = "top";
    } else if (position == one || position == two) {
        setOffsetPosition(position, error);
    }
    return true;
}

bool Axis::tickSide(const std::string& side, std::string* error) {
    // labels stay off when they were off before
    const bool labelled = majorTickKw_.label1On || majorTickKw_.label2On;
    if (!setTicksPosition(side, error)) return false;
    TickParams p;
    (side == "top" || side == "right" ? p.label2On : p.label1On) = labelled;
    setTickParams("both", false, p);
    return true;
}

bool Axis::setLabelPosition(const std::string& position, std::string* error) {
    if (isX_) {
        if (position != "top" && position != "bottom")
            return *error = support::pythonStrRepr(position) + " is not a valid value for position; supported values are 'top', 'bottom'", false;
        label.verticalAlignment = position == "top" ? "baseline" : "top";
    } else {
        if (position != "left" && position != "right")
            return *error = support::pythonStrRepr(position) + " is not a valid value for position; supported values are 'left', 'right'", false;
        label.rotationMode = "anchor";
        label.verticalAlignment = position == "left" ? "bottom" : "top";
    }
    labelPosition = position;
    return true;
}

bool Axis::setOffsetPosition(const std::string& position, std::string* error) {
    if (position != "left" && position != "right")
        return *error = support::pythonStrRepr(position) + " is not a valid value for position; supported values are 'left', 'right'", false;
    offsetText.x = position == "left" ? 0 : 1;
    offsetText.horizontalAlignment = position;
    return true;
}

void Axis::setMinorTickSwitches(const TickSwitches& sw) {
    minorTickKw_ = sw;
    for (auto& t : minorTicks_) t->applySwitches(sw);
}

std::vector<Tick*> Axis::minorTicks(std::size_t count) {
    while (minorTicks_.size() < count)
        minorTicks_.push_back(std::make_unique<Tick>(rc_, axes_, isX_, false, minorTickKw_));
    std::vector<Tick*> out;
    for (std::size_t i = 0; i < count; ++i) out.push_back(minorTicks_[i].get());
    return out;
}

std::string Axis::offsetString() const {
    // NullFormatter.get_offset() is '' (a log scale's formatters have none either)
    const Axis& owner = tickerAxis();
    if (logScale() || scale_ == "symlog" || scale_ == "logit" || owner.nullMajorFormatter || owner.fixedLabels || owner.dictLabels ||
        owner.dateTickers)
        return std::string();
    return formatter.offsetText();
}

void Axis::setLabelText(const std::string& t, const std::string& loc) {
    label.text = t;
    if (isX_) label.x = loc == "left" ? 0 : loc == "right" ? 1 : 0.5;
    else label.y = loc == "bottom" ? 0 : loc == "top" ? 1 : 0.5;
    label.horizontalAlignment = isX_ ? loc : (loc == "bottom" ? "left" : loc == "top" ? "right" : "center");
}

std::string Axis::ticksPosition() const {
    // Axis._get_ticks_position over a major tick and, where the minor locator is not a NullLocator
    // (log scales), a minor one: what the ticks of a group show is what its keywords say
    std::vector<const TickSwitches*> groups{&majorTickKw_};
    if (logScale() || scale_ == "symlog" || scale_ == "logit") groups.push_back(&minorTickKw_);
    const auto all = [&](bool tick1, bool tick2, bool label1) {
        for (const TickSwitches* k : groups)
            if (k->tick1On != tick1 || k->tick2On != tick2 || k->label1On != label1 || k->label2On == label1) return false;
        return true;
    };
    if (all(true, false, true)) return isX_ ? "bottom" : "left";
    if (all(false, true, false)) return isX_ ? "top" : "right";
    // 'default': both tick marks, labels on the first side only
    bool isDefault = true;
    for (const TickSwitches* k : groups) isDefault = isDefault && k->tick1On && k->tick2On && k->label1On && !k->label2On;
    return isDefault ? "default" : "unknown";
}

std::pair<std::vector<transforms::Bbox>, std::vector<transforms::Bbox>> Axis::ticklabelBboxes(
    const std::vector<Tick*>& ticks, Renderer& renderer) {
    std::pair<std::vector<transforms::Bbox>, std::vector<transforms::Bbox>> out;
    for (Tick* t : ticks)
        if (t->label1.visible) out.first.push_back(t->label1.windowExtent(renderer));
    for (Tick* t : ticks)
        if (t->label2.visible) out.second.push_back(t->label2.windowExtent(renderer));
    return out;
}

void Axis::updateLabelPosition(Renderer& renderer) {
    // _get_tick_boxes_siblings: this Axes alone (no aligned labels yet)
    const auto [bboxes, bboxes2] = ticklabelBboxes(updateTicks(), renderer);
    const double dpi = axes_.figure.dpi;
    if (isX_) {
        if (labelPosition == "bottom") {
            std::vector<transforms::Bbox> all = bboxes;
            all.push_back(axes_.spine("bottom").windowExtent());
            const auto b = unionOf(all).points();
            label.y = b[1] - labelpad * dpi / 72;
        } else {
            std::vector<transforms::Bbox> all = bboxes2;
            all.push_back(axes_.spine("top").windowExtent());
            const auto b = unionOf(all).points();
            label.y = b[3] + labelpad * dpi / 72;
        }
    } else {
        if (labelPosition == "left") {
            std::vector<transforms::Bbox> all = bboxes;
            all.push_back(axes_.spine("left").windowExtent());
            const auto b = unionOf(all).points();
            label.x = b[0] - labelpad * dpi / 72;
        } else {
            std::vector<transforms::Bbox> all = bboxes2;
            all.push_back(axes_.spine("right").windowExtent());
            const auto b = unionOf(all).points();
            label.x = b[2] + labelpad * dpi / 72;
        }
    }
}

void Axis::updateOffsetTextPosition(const std::vector<transforms::Bbox>& bboxes,
                                    const std::vector<transforms::Bbox>& bboxes2) {
    constexpr double kOffsetTextPad = 3;  // Axis.OFFSETTEXTPAD
    const double dpi = axes_.figure.dpi;
    const auto box = axes_.bbox->points();
    if (isX_) {
        if (tickPosition_ == "bottom") {
            const double bottom = bboxes.empty() ? std::min(box[1], box[3]) : unionOf(bboxes).points()[1];
            offsetText.y = bottom - kOffsetTextPad * dpi / 72;
        } else {
            const double top = bboxes2.empty() ? std::max(box[1], box[3]) : unionOf(bboxes2).points()[3];
            offsetText.y = top + kOffsetTextPad * dpi / 72;
        }
    } else {
        offsetText.y = std::max(box[1], box[3]) + kOffsetTextPad * dpi / 72;
    }
}

std::array<double, 2> Axis::viewInterval() const {
    axes_.unstaleViewLim();
    const auto p = axes_.viewLim->points();
    return isX_ ? std::array<double, 2>{p[0], p[2]} : std::array<double, 2>{p[1], p[3]};
}

void Axis::setTickSwitches(const TickSwitches& sw) {
    majorTickKw_ = sw;
    for (auto& t : majorTicks_) t->applySwitches(sw);
    // set_tick_params: the offset text shows when the major ticks have labels on either side
    offsetText.visible = sw.label1On || sw.label2On;
}

std::vector<Tick*> Axis::majorTicks(std::size_t count) {
    while (majorTicks_.size() < count)
        majorTicks_.push_back(std::make_unique<Tick>(rc_, axes_, isX_, true, majorTickKw_));
    std::vector<Tick*> out;
    for (std::size_t i = 0; i < count; ++i) out.push_back(majorTicks_[i].get());
    return out;
}

double Axis::tickSpace() const {
    // Bbox.unit().transformed(transAxes - dpi_scale_trans): transAxes, then the inches' inverse
    const auto t = transforms::compose(axes_.transAxes, axes_.figure.dpiScaleTrans->inverted());
    const auto ll = t->transform(0, 0), ul = t->transform(0, 1), lr = t->transform(1, 0);
    const double length = (isX_ ? lr[0] - ll[0] : ul[1] - ll[1]) * 72;
    // _get_tick_label_size: the major ticks' labelsize keyword, else rcParams[xtick.labelsize]
    const double labelsize = majorTickKw_.labelsize ? *majorTickKw_.labelsize : fontSizeOf(rc_, isX_ ? "xtick.labelsize" : "ytick.labelsize");
    const double size = labelsize * (isX_ ? 3 : 2);
    // (the locators take int() of it: axes::tickSpaceCount)
    if (size > 0) return std::floor(length / size);
    return 2147483647;
}

namespace {

// cbook._g_sig_digits(value, delta): the significant digits to %g-format value known to within delta; nothing
// where math.floor(math.log10(...)) raises (a delta that is NaN or not positive)
std::optional<int> gSigDigits(double value, double delta) {
    if (!std::isfinite(value)) return 0;
    if (delta == 0) {
        if (value == 0) return 3;
        delta = std::fabs(std::nextafter(value, std::copysign(std::numeric_limits<double>::infinity(), value)) - value);  // np.spacing
    }
    if (!(delta > 0) || !std::isfinite(delta)) return std::nullopt;
    const int before = value != 0 ? int(std::floor(std::log10(std::fabs(value)))) + 1 : 1;
    return std::max(0, before - int(std::floor(std::log10(delta))));
}

std::optional<std::string> percent(const std::string& fmt, double v) {
    const auto f = support::percentFormat(fmt, v, false);
    if (f.kind != support::Formatted::Text) return std::nullopt;
    return f.text;
}

std::optional<std::string> brace(const std::string& fmt, double v) {
    const auto f = support::braceFormat(fmt, v, false);
    if (f.kind != support::Formatted::Text) return std::nullopt;
    return f.text;
}

}  // namespace

std::optional<std::string> Axis::formatDataShort(double value) const {
    const Axis& owner = tickerAxis();
    if (owner.nullMajorFormatter || owner.fixedLabels) return std::string();  // (both say '' without a position)
    if (owner.dictLabels) {  // FuncFormatter: tickd.get(x, '')
        std::string found;
        for (const auto& [at, text] : *owner.dictLabels)
            if (at == value) found = text;
        return found;
    }
    if (owner.dateTickers) {  // AutoDateFormatter.__call__(x): its format for the locator's scale
        std::string text, error;
        const double unit = owner.fixedLocs ? 1.0 : owner.dateLocator.unit();
        if (!owner.dateFormatter.format(value, unit, &text, &error)) return std::nullopt;
        return text;
    }
    if (logScale() || scale_ == "symlog") {  // LogFormatter: ('%-12g' % value).rstrip()
        auto s = percent("%-12g", value);
        if (s) s->erase(s->find_last_not_of(' ') + 1);
        return s;
    }
    if (scale_ == "logit") {  // LogitFormatter: scientific below 0.1, fixed below 0.9, else 1 - the rest
        if (value < 0.1) return brace("{:e}", value);
        if (value < 0.9) return brace("{:f}", value);
        const auto rest = brace("{:e}", 1 - value);
        return rest ? std::optional<std::string>("1-" + *rest) : std::nullopt;
    }
    // ScalarFormatter: as many digits as one pixel of the formatter's axis tells apart
    const Axes& on = owner.axes_;
    const auto& t = isX_ ? on.xaxisTransform : on.yaxisTransform;
    const auto inverse = t->inverted();
    const auto screen = isX_ ? t->transform(value, 0.0) : t->transform(0.0, value);
    double delta = 0;
    for (const double step : {-1.0, 1.0}) {
        const auto n = isX_ ? inverse->transform(screen[0] + step, screen[1]) : inverse->transform(screen[0], screen[1] + step);
        const double d = std::fabs((isX_ ? n[0] : n[1]) - value);
        delta = std::isnan(delta) || std::isnan(d) ? std::numeric_limits<double>::quiet_NaN() : std::max(delta, d);  // .max()
    }
    const auto digits = gSigDigits(value, delta);
    if (!digits) return std::nullopt;
    auto s = percent("%-#." + std::to_string(*digits) + "g", value);
    if (!s) return std::nullopt;
    if (rc_.boolean("axes.unicode_minus")) {  // fix_minus
        std::string out;
        for (char c : *s) out += c == '-' ? std::string("\xe2\x88\x92") : std::string(1, c);
        return out;
    }
    return s;
}

std::vector<Tick*> Axis::updateTicks() {
    // The locators and formatters read the limits, the tick space and the minpos of their own axis
    // (the one this axis shares its tickers with); the ticks are then kept by this axis' limits.
    Axis& owner = tickerAxis();
    const auto ownerView = owner.viewInterval();
    const auto view = viewInterval();
    const auto groupVisible = [](const TickSwitches& kw) {
        return kw.tick1On || kw.tick2On || kw.label1On || kw.label2On || kw.gridOn;
    };
    std::vector<Tick*> ticks;
    if (groupVisible(majorTickKw_)) {
        // the major locator's locations, then its formatter's labels
        std::vector<double> locs;
        if (owner.fixedLocs) {
            locs = *owner.fixedLocs;
        } else if (owner.dateTickers) {
            std::string error;
            if (!owner.dateLocator.ticks(ownerView[0], ownerView[1], &locs, &error)) support::noteRaised(error);
        } else if (logScale()) {
            logLocator.tickSpace = owner.tickSpace();
            logLocator.minpos = owner.minpos();
            locs = logLocator.tickValues(ownerView[0], ownerView[1]);
        } else if (scale_ == "symlog") {
            locs = owner.symlogLocator.tickValues(ownerView[0], ownerView[1]);
        } else if (scale_ == "logit") {
            owner.logitLocator.tickSpace = owner.tickSpace();
            owner.logitLocator.minpos = owner.minpos();
            locs = owner.logitLocator.tickValues(ownerView[0], ownerView[1]);
        } else {
            locator.tickSpace = owner.tickSpace();
            locs = locator.tickValues(ownerView[0], ownerView[1]);
        }
        std::vector<std::string> labels;
        if (owner.nullMajorFormatter) {
            labels.assign(locs.size(), "");
        } else if (owner.fixedLabels) {  // FixedFormatter: the i-th label, '' past its end
            for (std::size_t i = 0; i < locs.size(); ++i) labels.push_back(i < owner.fixedLabels->size() ? (*owner.fixedLabels)[i] : "");
        } else if (owner.dictLabels) {  // tickd.get(x, ''): the last label given for an equal location
            for (double v : locs) {
                std::string found;
                for (const auto& [at, text] : *owner.dictLabels)
                    if (at == v) found = text;
                labels.push_back(found);
            }
        } else if (owner.dateTickers) {
            for (double v : locs) {
                std::string text, error;
                // (set_ticks gave the formatter the FixedLocator, which has no unit: _get_unit raises, the unit is a day)
                const double unit = owner.fixedLocs ? 1.0 : owner.dateLocator.unit();
                if (!owner.dateFormatter.format(v, unit, &text, &error)) support::noteRaised(error);
                labels.push_back(text);
            }
        } else if (logScale() || scale_ == "symlog") {
            logFormatter.setLocs(ownerView[0], ownerView[1]);
            for (double v : locs) labels.push_back(logFormatter(v));
        } else if (scale_ == "logit") {  // format_ticks: set_locs of the locations, then each
            owner.logitFormatter.setLocs(locs);
            for (double v : locs) labels.push_back(owner.logitFormatter(v));
        } else {
            formatter.setViewInterval(ownerView[0], ownerView[1]);
            formatter.setLocs(locs);
            for (double v : locs) labels.push_back(formatter(v));
        }
        const std::vector<Tick*> major = majorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            major[i]->updatePosition(locs[i]);
            major[i]->label1.text = major[i]->label2.text = labels[i];
            ticks.push_back(major[i]);
        }
    }
    // The minor locator: LogLocator(subs='auto') on log scales, NullLocator otherwise
    if (groupVisible(minorTickKw_) && logScale()) {
        logMinorLocator.tickSpace = owner.tickSpace();
        logMinorLocator.minpos = owner.minpos();
        logLocator.tickSpace = owner.tickSpace();
        logLocator.minpos = owner.minpos();
        // get_minorticklocs: the minor locations a major one covers go (within this axis' limits)
        const auto locs = axes::withoutMajorTicks(logMinorLocator.tickValues(ownerView[0], ownerView[1]),
                                                  owner.fixedLocs ? *owner.fixedLocs : logLocator.tickValues(ownerView[0], ownerView[1]),
                                                  view[0], view[1], scaleOptions_.base);
        logMinorFormatter.setLocs(ownerView[0], ownerView[1]);
        const std::vector<Tick*> minor = minorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            minor[i]->updatePosition(locs[i]);
            minor[i]->label1.text = minor[i]->label2.text = owner.nullMinorFormatter ? "" : logMinorFormatter(locs[i]);
            ticks.push_back(minor[i]);
        }
    }
    // SymmetricalLogLocator(subs) on a symlog scale, its labels empty (NullFormatter); the minor ticks within
    // 1e-5 of the view (through the scale's transform) of a major one go
    if (groupVisible(minorTickKw_) && scale_ == "symlog") {
        const auto majorLocs = owner.fixedLocs ? *owner.fixedLocs : owner.symlogLocator.tickValues(ownerView[0], ownerView[1]);
        double lo = scaleForward(view[0]), hi = scaleForward(view[1]);
        if (lo > hi) std::swap(lo, hi);
        const double tol = (hi - lo) * 1e-5;
        std::vector<double> locs;
        for (double m : owner.symlogMinorLocator.tickValues(ownerView[0], ownerView[1])) {
            bool close = false;
            for (double M : majorLocs) close = close || std::fabs(scaleForward(m) - scaleForward(M)) <= tol;
            if (!close) locs.push_back(m);
        }
        const std::vector<Tick*> minor = minorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            minor[i]->updatePosition(locs[i]);
            minor[i]->label1.text = minor[i]->label2.text = "";
            ticks.push_back(minor[i]);
        }
    }
    // LogitLocator(minor=True) on a logit scale, labelled by LogitFormatter(minor=True)
    if (groupVisible(minorTickKw_) && scale_ == "logit") {
        owner.logitLocator.tickSpace = owner.logitMinorLocator.tickSpace = owner.tickSpace();
        owner.logitLocator.minpos = owner.logitMinorLocator.minpos = owner.minpos();
        const auto majorLocs = owner.fixedLocs ? *owner.fixedLocs : owner.logitLocator.tickValues(ownerView[0], ownerView[1]);
        double lo = scaleForward(view[0]), hi = scaleForward(view[1]);
        if (lo > hi) std::swap(lo, hi);
        const double tol = (hi - lo) * 1e-5;
        std::vector<double> locs;
        for (double m : owner.logitMinorLocator.tickValues(ownerView[0], ownerView[1])) {
            bool close = false;
            for (double M : majorLocs) close = close || std::fabs(scaleForward(m) - scaleForward(M)) <= tol;
            if (!close) locs.push_back(m);
        }
        owner.logitMinorFormatter.setLocs(locs);
        const std::vector<Tick*> minor = minorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            minor[i]->updatePosition(locs[i]);
            minor[i]->label1.text = minor[i]->label2.text = owner.nullMinorFormatter ? "" : owner.logitMinorFormatter(locs[i]);
            ticks.push_back(minor[i]);
        }
    }
    // AutoMinorLocator on a linear scale that has one; its labels are empty (NullFormatter)
    // (every axis makes the same locator from the rcParams: this one's is as good as the owner's)
    if (groupVisible(minorTickKw_) && (scale_ == "linear" || scale_ == "function") && minorLocator) {
        locator.tickSpace = owner.tickSpace();
        // get_majorticklocs(): the major locator's (the date locator's on a date axis)
        std::vector<double> majorLocs;
        if (owner.fixedLocs) {
            majorLocs = *owner.fixedLocs;
        } else if (owner.dateTickers) {
            std::string error;
            if (!owner.dateLocator.ticks(ownerView[0], ownerView[1], &majorLocs, &error)) support::noteRaised(error);
        } else {
            majorLocs = locator.tickValues(ownerView[0], ownerView[1]);
        }
        const auto locs = axes::withoutMajorTicks((*minorLocator)(majorLocs, ownerView[0], ownerView[1]), majorLocs, view[0], view[1]);
        const std::vector<Tick*> minor = minorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            minor[i]->updatePosition(locs[i]);
            minor[i]->label1.text = minor[i]->label2.text = "";
            ticks.push_back(minor[i]);
        }
    }
    if (ticks.empty()) return {};
    double low = view[0], high = view[1];
    if (low > high) std::swap(low, high);
    const double lowT = scaleForward(low), highT = scaleForward(high);
    std::vector<Tick*> toDraw;
    for (Tick* t : ticks)
        if (intervalContainsClose(lowT, highT, scaleForward(t->loc()))) toDraw.push_back(t);
    return toDraw;
}

void Axis::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup("matplotlib.axis", gid);
    const std::vector<Tick*> ticks = updateTicks();
    const auto [tlb1, tlb2] = ticklabelBboxes(ticks, renderer);
    for (Tick* t : ticks) t->draw(renderer);
    updateLabelPosition(renderer);
    label.draw(renderer);
    updateOffsetTextPosition(tlb1, tlb2);
    offsetText.text = offsetString();
    offsetText.draw(renderer);
    renderer.closeGroup("matplotlib.axis");
}

std::optional<transforms::Bbox> Axis::tightbbox(Renderer& renderer, bool forLayoutOnly) {
    if (!visible) return std::nullopt;
    const std::vector<Tick*> ticks = updateTicks();
    updateLabelPosition(renderer);
    const auto [tlb1, tlb2] = ticklabelBboxes(ticks, renderer);
    updateOffsetTextPosition(tlb1, tlb2);
    offsetText.text = offsetString();
    std::vector<transforms::Bbox> bboxes;
    if (offsetText.visible) bboxes.push_back(offsetText.windowExtent(renderer));
    bboxes.insert(bboxes.end(), tlb1.begin(), tlb1.end());
    bboxes.insert(bboxes.end(), tlb2.begin(), tlb2.end());
    if (label.visible) {
        auto p = label.windowExtent(renderer).points();
        if (forLayoutOnly) {
            // only the label's centre counts along its axis
            if (isX_ && p[2] - p[0] > 0) {
                p[0] = (p[0] + p[2]) / 2 - 0.5;
                p[2] = p[0] + 1.0;
            }
            if (!isX_ && p[3] - p[1] > 0) {
                p[1] = (p[1] + p[3]) / 2 - 0.5;
                p[3] = p[1] + 1.0;
            }
        }
        bboxes.push_back(transforms::Bbox(p));
    }
    std::vector<transforms::Bbox> finite;
    for (const auto& b : bboxes)
        if (isFiniteBox(b)) finite.push_back(b);
    if (finite.empty()) return std::nullopt;
    return unionOf(finite);
}

// --- Axes ----------------------------------------------------------------------------------------

Axes::Axes(const rc::Params& rc, Figure& fig, const SubplotSpec& spec) : Axes(rc, fig, spec.position(fig.subplotpars), spec) {}

Axes::Axes(const rc::Params& rc, Figure& fig, std::shared_ptr<transforms::Bbox> pos, const std::optional<SubplotSpec>& spec)
    : Artist(rc), figure(fig), xmargin(rc.number("axes.xmargin")), ymargin(rc.number("axes.ymargin")) {
    subplotSpec = spec;
    position = std::move(pos);
    originalPosition = std::make_shared<transforms::Bbox>(position->points());
    // set_figure
    bbox = std::make_shared<transforms::TransformedBbox>(position, fig.transSubfigure);
    dataLim = std::make_shared<transforms::Bbox>(std::array<double, 4>{
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()});  // Bbox.null()
    viewLim = std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1});
    setLimAndTransforms();
    const char* sides[4] = {"left", "right", "bottom", "top"};
    for (int i = 0; i < 4; ++i) spines[i] = std::make_unique<Spine>(rc, *this, sides[i]);
    xaxis = std::make_unique<Axis>(rc, *this, true);
    spine("bottom").axis = spine("top").axis = xaxis.get();
    yaxis = std::make_unique<Axis>(rc, *this, false);
    spine("left").axis = spine("right").axis = yaxis.get();
    shared_[0] = std::make_shared<SharedAxes>(SharedAxes{{this}, xaxis.get()});
    shared_[1] = std::make_shared<SharedAxes>(SharedAxes{{this}, yaxis.get()});

    // clear(): the titles and the patch; the view limits (0, 1) stay.
    const double titleSize = fontSizeOf(rc, "axes.titlesize");
    const text::FontValue titleWeight = rc.value("axes.titleweight").isNumber()
                                            ? text::FontValue(int(rc.number("axes.titleweight")))
                                            : text::FontValue(rc.text("axes.titleweight"));
    autotitlepos_ = rc.value("axes.titley").isNull();
    const double titleY = autotitlepos_ ? 1.0 : rc.number("axes.titley");
    const double xs[3] = {0.5, 0.0, 1.0};
    const char* has[3] = {"center", "left", "right"};
    std::unique_ptr<Text>* titles[3] = {&title, &leftTitle, &rightTitle};
    for (int i = 0; i < 3; ++i) {
        auto t = std::make_unique<Text>(rc, xs[i], titleY, "");
        t->font.size = titleSize;
        t->font.weight = titleWeight;
        t->verticalAlignment = "baseline";
        t->horizontalAlignment = has[i];
        t->figure = &fig;
        *titles[i] = std::move(t);
    }
    setTitleOffsetTrans(rc.number("axes.titlepad"));
    patch = std::make_unique<Rectangle>(rc, 0.0, 0.0, 1.0, 1.0);
    patch->setFacecolor(rc.text("axes.facecolor"));
    patch->setEdgecolor("none");
    patch->setLinewidth(0.0);
    patch->transform = transAxes;

    // The first major tick is made while clearing (Axis.set_clip_path reaches majorTicks), then
    // tick_params sets the ticks' visibility from the rcParams.
    const auto sw = [&](bool isX) {
        TickSwitches s;
        if (isX) {
            s.tick2On = rc.boolean("xtick.top") && rc.boolean("xtick.major.top");
            s.tick1On = rc.boolean("xtick.bottom") && rc.boolean("xtick.major.bottom");
            s.label2On = rc.boolean("xtick.labeltop") && rc.boolean("xtick.major.top");
            s.label1On = rc.boolean("xtick.labelbottom") && rc.boolean("xtick.major.bottom");
        } else {
            s.tick1On = rc.boolean("ytick.left") && rc.boolean("ytick.major.left");
            s.tick2On = rc.boolean("ytick.right") && rc.boolean("ytick.major.right");
            s.label1On = rc.boolean("ytick.labelleft") && rc.boolean("ytick.major.left");
            s.label2On = rc.boolean("ytick.labelright") && rc.boolean("ytick.major.right");
        }
        // grid(False), then grid(rcParams["axes.grid"], which=rcParams["axes.grid.which"], axis=rcParams["axes.grid.axis"])
        const std::string gridAxis = rc.text("axes.grid.axis");
        s.gridOn = rc.boolean("axes.grid") && (rc.text("axes.grid.which") == "both" || rc.text("axes.grid.which") == "major") &&
                   (gridAxis == "both" || gridAxis == (isX ? "x" : "y"));
        return s;
    };
    xaxis->setTickSwitches(sw(true));
    yaxis->setTickSwitches(sw(false));
    const auto minorSw = [&](bool isX) {  // tick_params(which='minor', ...)
        TickSwitches s;
        if (isX) {
            s.tick2On = rc.boolean("xtick.top") && rc.boolean("xtick.minor.top");
            s.tick1On = rc.boolean("xtick.bottom") && rc.boolean("xtick.minor.bottom");
            s.label2On = rc.boolean("xtick.labeltop") && rc.boolean("xtick.minor.top");
            s.label1On = rc.boolean("xtick.labelbottom") && rc.boolean("xtick.minor.bottom");
        } else {
            s.tick1On = rc.boolean("ytick.left") && rc.boolean("ytick.minor.left");
            s.tick2On = rc.boolean("ytick.right") && rc.boolean("ytick.minor.right");
            s.label1On = rc.boolean("ytick.labelleft") && rc.boolean("ytick.minor.left");
            s.label2On = rc.boolean("ytick.labelright") && rc.boolean("ytick.minor.right");
        }
        // (grid(False) is for the major grid only: the minor one of an axis that rcParams["axes.grid.axis"]
        // leaves out stays as Axis.clear set it, on)
        s.gridOn = rc.boolean("axes.grid") && (rc.text("axes.grid.which") == "both" || rc.text("axes.grid.which") == "minor");
        return s;
    };
    updateTransScale();  // the end of clear()
    xaxis->setMinorTickSwitches(minorSw(true));
    yaxis->setMinorTickSwitches(minorSw(false));
    // set_tick_params goes over the ticks of each axis, which makes the first one of each list
    // (_LazyTickList), and making a tick sets the position of the two spines it sits on: from here on
    // the spines have their own transforms, also where no tick is ever drawn.
    for (const char* side : {"bottom", "top", "left", "right"}) spine(side).spineTransform();
}

void Axes::setTitleOffsetTrans(double points) {
    auto offset = std::make_shared<transforms::ScaledTranslation>(0.0, points / 72, figure.dpiScaleTrans);
    for (Text* t : {title.get(), leftTitle.get(), rightTitle.get()}) {
        t->transform = transforms::compose(transAxes, offset);
        t->clipBox.reset();
    }
}

Text& Axes::setTitle(const std::string& titleText, const std::string& loc, std::optional<double> pad, std::optional<double> y) {
    double ty = 1.0;
    if (y) {
        ty = *y;
        autotitlepos_ = false;
    } else if (!rc_.value("axes.titley").isNull()) {
        ty = rc_.number("axes.titley");
        autotitlepos_ = false;
    }
    Text& t = loc == "left" ? *leftTitle : loc == "right" ? *rightTitle : *title;
    setTitleOffsetTrans(pad ? *pad : rc_.number("axes.titlepad"));
    t.text = titleText;
    t.font.size = fontSizeOf(rc_, "axes.titlesize");
    const auto& weight = rc_.value("axes.titleweight");
    t.font.weight = weight.isNumber() ? text::FontValue(int(weight.asNumber())) : text::FontValue(weight.asString());
    t.verticalAlignment = "baseline";
    t.horizontalAlignment = loc;
    const std::string color = rc_.text("axes.titlecolor");
    if (color != "auto" && color != "AUTO" && color != "Auto") t.setColor(color);
    t.y = ty;
    return t;
}

void Axes::updateTitlePosition(Renderer& renderer) {
    if (!autotitlepos_) return;
    Text* titles[3] = {title.get(), leftTitle.get(), rightTitle.get()};
    if (titles[0]->text.empty() && titles[1]->text.empty() && titles[2]->text.empty()) return;
    // The Axes to clear: this one, its child Axes and its twins (no aligned titles yet); the children are
    // placed first. Of each, the top of its x axis when the ticks or the label are on top, else of its box.
    std::vector<Axes*> axs{this};
    for (auto& child : childAxes) axs.push_back(child.get());
    if (twinned)
        for (Axes* a : *twinned)
            if (a != this) axs.push_back(a);
    for (auto& child : childAxes) child->applyAspect();
    double top = -std::numeric_limits<double>::infinity();
    for (Axes* a : axs) {
        std::optional<transforms::Bbox> bb;
        const std::string ticks = a->xaxis->ticksPosition();
        if (ticks == "top" || ticks == "unknown" || a->xaxis->labelPosition == "top") bb = a->xaxis->tightbbox(renderer, false);
        // ax.spines.get("outline", ax).get_window_extent(): the Axes' own box
        const auto p = bb ? bb->points() : a->bbox->points();
        const double ymax = std::max(p[1], p[3]);
        if (ymax > top) top = ymax;
    }
    for (Text* t : titles) {
        t->y = 1.0;
        for (Axes* a : t->text.empty() ? std::vector<Axes*>{} : axs) {
            Axis& ya = *a->yaxis;
            ya.tightbbox(renderer, false);  // updates the offset text
            if (ya.offsetText.visible && !ya.offsetText.text.empty()) {
                // a title that meets a y axis' offset text goes above it
                const auto bb = ya.offsetText.windowExtent(renderer).points();
                const auto tb = t->windowExtent(renderer).points();
                const double x0 = std::max(std::min(tb[0], tb[2]), std::min(bb[0], bb[2]));
                const double x1 = std::min(std::max(tb[0], tb[2]), std::max(bb[0], bb[2]));
                const double y0 = std::max(std::min(tb[1], tb[3]), std::min(bb[1], bb[3]));
                const double y1 = std::min(std::max(tb[1], tb[3]), std::max(bb[1], bb[3]));
                if (x0 <= x1 && y0 <= y1) top = std::max(bb[1], bb[3]);  // Bbox.intersection is not None
            }
        }
        if (top < 0) return;  // the top of the Axes is not on the figure: the titles stay
        const auto ymin = [&] {
            const auto p = t->windowExtent(renderer).points();
            return std::min(p[1], p[3]);
        };
        if (ymin() < top) {
            const auto inv = transAxes->inverted();
            t->y = inv->transform(0, top)[1];
            // this does not always get the bottom to the top, so it is adjusted once more
            const double low = ymin();
            if (low < top) t->y = inv->transform(0., 2 * top - low)[1];
        }
    }
    // all the titles line up at the highest baseline
    double ymax = titles[0]->y;
    for (Text* t : titles) ymax = std::max(ymax, t->y);
    for (Text* t : titles) t->y = ymax;
}

void Axes::updateFromPath(const std::vector<double>& v, bool updatex, bool updatey) {
    // Bbox.update_from_path(path, ignore=ignore_existing_data_limits, updatex, updatey)
    if (v.empty() || !(updatex || updatey)) return;
    const double inf = std::numeric_limits<double>::infinity();
    auto points = ignoreExistingDataLimits_ ? std::array<double, 4>{inf, inf, -inf, -inf} : dataLim->points();
    auto pos = ignoreExistingDataLimits_ ? std::array<double, 2>{inf, inf} : minpos;
    double minx = inf, maxx = -inf, miny = inf, maxy = -inf, posx = inf, posy = inf;
    for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
        if (!std::isfinite(v[i]) || !std::isfinite(v[i + 1])) continue;
        minx = std::min(minx, v[i]), maxx = std::max(maxx, v[i]);
        miny = std::min(miny, v[i + 1]), maxy = std::max(maxy, v[i + 1]);
        if (v[i] > 0) posx = std::min(posx, v[i]);
        if (v[i + 1] > 0) posy = std::min(posy, v[i + 1]);
    }
    auto current = dataLim->points();
    // Python's min(a, b) / max(a, b): the first unless the second is beyond it
    if (updatex) {
        current[0] = minx < points[0] ? minx : points[0];
        current[2] = maxx > points[2] ? maxx : points[2];
        minpos[0] = posx < pos[0] ? posx : pos[0];
    }
    if (updatey) {
        current[1] = miny < points[1] ? miny : points[1];
        current[3] = maxy > points[3] ? maxy : points[3];
        minpos[1] = posy < pos[1] ? posy : pos[1];
    }
    dataLim->setPoints(current);
}

Line2D& Axes::addLine(std::unique_ptr<Line2D> line, bool keepTransform) {
    line->axes = this;
    if (!keepTransform) line->transform = transData;  // _set_artist_props
    // set_clip_path(self.patch): a Rectangle clips by its box
    line->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    // _update_line_limits
    const Path& path = line->path();
    if (path.size()) {
        std::vector<double> v = *path.vertices;
        const auto update = transforms::containsBranchSeparately(line->transform, transData);
        // A line whose transform equals transData (as transforms compare: an axhline's blend does
        // while the view limits are still (0, 1)) gives its path as it is; otherwise its path goes
        // through trf_to_data = line_trf - transData: as one affine when transData is affine, and
        // whole (trf_to_data.transform_path) when it is not (a log scale).
        if (!transforms::equal(*line->transform, *transData) && (update[0] || update[1])) {
            const auto toData = transforms::subtract(line->transform, transData);
            if (transData->isAffine()) {
                const auto m = toData->matrix();
                for (std::size_t k = 0; k + 1 < v.size(); k += 2) {
                    const auto q = transforms::apply(m, v[k], v[k + 1]);
                    v[k] = q[0], v[k + 1] = q[1];
                }
            } else {
                for (std::size_t k = 0; k + 1 < v.size(); k += 2) {
                    const auto q = toData->transform(v[k], v[k + 1]);
                    v[k] = q[0], v[k + 1] = q[1];
                }
            }
        }
        updateFromPath(v, update[0], update[1]);
        ignoreExistingDataLimits_ = false;
    }
    if (line->label.empty()) line->label = "_child" + std::to_string(children.size());
    children.push_back(std::move(line));
    return static_cast<Line2D&>(*children.back());
}

Text& Axes::addText(std::unique_ptr<Text> t) {
    // Axes.text: the patch clips it (a Rectangle clips by its box) when clipping is on, which it is
    // not unless asked for; _set_artist_props: the figure and transData
    t->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    t->figure = &figure;
    t->transform = transData;
    children.push_back(std::move(t));
    return static_cast<Text&>(*children.back());
}

bool Axes::containsPoint(double x, double y) const {
    // Patch.contains_point(point, radius=1.0): Path.contains_point through the patch's (affine) transform
    const Path p = patch->path();
    mpl::PathIterator it(p.vertices, p.codes, p.shouldSimplify, p.simplifyThreshold);
    const auto m = patch->fullTransform()->matrix();
    agg::trans_affine t(m.sx, m.shy, m.shx, m.sy, m.tx, m.ty);
    return mpl::point_in_path(x, y, 1.0, it, t);
}

Patch& Axes::addPatch(std::unique_ptr<Patch> p) {
    if (!p->transformSet) p->transform = transData;  // _set_artist_props
    p->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    // _update_patch_limits: the ends of the path's segments (iter_bezier) through patch_trf - transData;
    // a Rectangle of no width and no height counts for nothing
    const auto* rect = dynamic_cast<const Rectangle*>(p.get());
    if (!rect || rect->width != 0 || rect->height != 0) {
        const auto t = p->fullTransform();
        const auto update = transforms::containsBranchSeparately(t, transData);
        if (update[0] || update[1]) {
            const auto toData = transforms::subtract(t, transData);
            std::vector<std::array<double, 2>> xys;
            if (rect) {
                const double corners[8][2] = {{0, 0}, {1, 0}, {1, 0}, {1, 1}, {1, 1}, {0, 1}, {0, 1}, {0, 0}};
                for (const auto& c : corners) xys.push_back(toData->transform(c[0], c[1]));
            } else {
                for (const auto& q : bezierEnds(p->path())) xys.push_back(toData->transform(q[0], q[1]));
            }
            updateDatalim(xys, update[0], update[1]);
        }
    }
    if (p->label.empty()) p->label = "_child" + std::to_string(children.size());
    children.push_back(std::move(p));
    return static_cast<Patch&>(*children.back());
}

Patch& Axes::addArtist(std::unique_ptr<Patch> p) {
    if (!p->transformSet) p->transform = transData;  // _set_artist_props
    p->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    children.push_back(std::move(p));
    return static_cast<Patch&>(*children.back());
}

std::vector<std::pair<std::string, support::Json>> Axes::patchDefaults(const std::vector<std::string>& given,
                                                                       const std::vector<std::string>& ignore) {
    const auto cycle = rc_.propCycle();
    const auto& entry = cycle[patchCycleIndex_];
    const auto has = [](const std::vector<std::string>& list, const std::string& p) { return std::find(list.begin(), list.end(), p) != list.end(); };
    bool unset = false;
    for (const auto& p : entry) unset = unset || (!has(ignore, p.first) && !has(given, p.first));
    if (!unset) return {};
    patchCycleIndex_ = (patchCycleIndex_ + 1) % cycle.size();
    std::vector<std::pair<std::string, support::Json>> out;
    for (const auto& p : entry)
        if (!has(ignore, p.first)) out.push_back(p);
    return out;
}

std::vector<std::pair<std::string, support::Json>> Axes::lineDefaults(const std::vector<std::string>& given) {
    // _PropCycle.getdefaults(kw): the next entry, when the keywords leave some property of it unset
    const auto cycle = rc_.propCycle();
    const auto& entry = cycle[cycleIndex_];
    const auto isGiven = [&](const std::string& property) { return std::find(given.begin(), given.end(), property) != given.end(); };
    if (std::all_of(entry.begin(), entry.end(), [&](const auto& p) { return isGiven(p.first); })) return {};
    cycleIndex_ = (cycleIndex_ + 1) % cycle.size();
    // _setdefaults: only what the keywords do not say
    std::vector<std::pair<std::string, support::Json>> out;
    for (const auto& p : entry)
        if (!isGiven(p.first)) out.push_back(p);
    return out;
}

colors::Spec Axes::nextColor(std::size_t* index) {
    // _PropCycle.get_next_color(): the colour of the next entry; black, and the cycle stays, without one
    const auto cycle = rc_.propCycle();
    for (const auto& [property, value] : cycle[*index]) {
        if (property != "color") continue;
        *index = (*index + 1) % cycle.size();
        return colors::specOf(value);
    }
    return std::string("k");
}

void Axes::updateDatalim(const std::vector<std::array<double, 2>>& xys, bool updatex, bool updatey) {
    // update_datalim: nothing unless some value is finite; then Bbox.update_from_data_xy
    bool anyFinite = false;
    for (const auto& p : xys) anyFinite = anyFinite || std::isfinite(p[0]) || std::isfinite(p[1]);
    if (!anyFinite) return;
    std::vector<double> flat;
    for (const auto& p : xys) flat.push_back(p[0]), flat.push_back(p[1]);
    updateFromPath(flat, updatex, updatey);
    ignoreExistingDataLimits_ = false;
}


Collection& Axes::addCollection(std::unique_ptr<Collection> c) {
    if (c->label.empty()) c->label = "_child" + std::to_string(children.size());
    c->figure = &figure;
    if (!c->transformSet) c->transform = transData;  // _set_artist_props
    c->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    unstaleViewLim();
    // _update_collection_limits: the box's corners and its minpos (when finite)
    const auto d = c->dataLim(transData);
    std::vector<std::array<double, 2>> points{{d.points[0], d.points[1]}, {d.points[2], d.points[3]}};
    if (std::isfinite(d.minpos[0]) || std::isfinite(d.minpos[1])) points.push_back(d.minpos);
    updateDatalim(points);
    requestAutoscaleView(true, true);
    Collection& ref = *c;
    children.push_back(std::move(c));
    return ref;
}

Collection& Axes::addLinesCollection(std::unique_ptr<Collection> c) {
    if (c->label.empty()) c->label = "_child" + std::to_string(children.size());
    c->figure = &figure;
    if (!c->transformSet) c->transform = transData;
    c->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    Collection& ref = *c;
    children.push_back(std::move(c));
    if (!ref.paths.empty()) {
        const auto d = ref.dataLim(transData);
        updateDatalim({{d.points[0], d.points[1]}, {d.points[2], d.points[3]}});
        requestAutoscaleView(true, true);
    }
    return ref;
}

void Axes::requestAutoscaleView(bool x, bool y) {
    staleX_ = staleX_ || x;
    staleY_ = staleY_ || y;
}

void Axes::unstaleViewLim() {
    // a pending autoscale of any of the Axes sharing a direction counts, and is spent for all of them
    bool need[2] = {false, false};
    for (int d = 0; d < 2; ++d)
        for (const Axes* ax : shared_[d]->members) need[d] = need[d] || (d == 0 ? ax->staleX_ : ax->staleY_);
    if (!need[0] && !need[1]) return;
    for (int d = 0; d < 2; ++d)
        for (Axes* ax : shared_[d]->members) (d == 0 ? ax->staleX_ : ax->staleY_) = false;
    autoscaleView(need[0], need[1]);
}

void Axes::autoscaleView(bool scalex, bool scaley) {
    const auto handle = [&](bool scale, Axis& axis, bool isX, double margin) {
        if (!(scale && axis.autoscaleOn)) return;
        const std::vector<Axes*>& siblings = shared_[isX ? 0 : 1]->members;
        // the sticky edges of the data children of the Axes sharing the axis, sorted (only with a margin;
        // positive ones on a log scale)
        std::vector<double> stickies;
        if (margin != 0)
            for (const Axes* ax : siblings)
                for (const auto& c : ax->children)
                    for (double v : isX ? c->stickyX : c->stickyY) stickies.push_back(v);
        // (np.sort and np.searchsorted: NaN after every number)
        const auto nanLast = [](double a, double b) { return !std::isnan(a) && (std::isnan(b) || a < b); };
        std::sort(stickies.begin(), stickies.end(), nanLast);
        if (axis.scale() == "log") stickies.erase(std::remove_if(stickies.begin(), stickies.end(), [](double v) { return !(v > 0); }), stickies.end());
        // the finite ends of the data limits of all the Axes sharing the axis
        bool any = false;
        double x0 = 0, x1 = 0;
        for (const Axes* ax : siblings) {
            const auto d = ax->dataLim->points();
            for (double val : {isX ? d[0] : d[1], isX ? d[2] : d[3]}) {
                if (!std::isfinite(val)) continue;
                if (!any || val < x0) x0 = val;
                if (!any || val > x1) x1 = val;
                any = true;
            }
        }
        if (!any) {
            const auto v = viewLim->points();
            const bool mutated = isX ? (v[0] != 0 || v[2] != 1) : (v[1] != 0 || v[3] != 1);
            if (mutated) return;
            x0 = -std::numeric_limits<double>::infinity();
            x1 = std::numeric_limits<double>::infinity();
        }
        std::tie(x0, x1) = axis.locatorNonsingular(x0, x1);
        // the stickies the margins must not cross: the largest below x0 + tol, the smallest above x1 - tol
        std::optional<double> x0bound, x1bound;
        if (!stickies.empty()) {
            const double tol = 1e-5 * std::abs(x1 - x0);
            const auto i0 = std::lower_bound(stickies.begin(), stickies.end(), x0 + tol, nanLast) - stickies.begin() - 1;
            if (i0 != -1) x0bound = stickies[std::size_t(i0)];
            const auto i1 = std::lower_bound(stickies.begin(), stickies.end(), x1 - tol, nanLast) - stickies.begin();
            if (std::size_t(i1) != stickies.size()) x1bound = stickies[std::size_t(i1)];
        }
        if (axis.logScale()) {  // limit_range_for_scale(x0, x1, minimum_minpos)
            double mp = axis.sharedMinpos();
            if (!std::isfinite(mp)) mp = 1e-300;
            x0 = x0 <= 0 ? mp : x0;
            x1 = x1 <= 0 ? mp : x1;
        }
        if (axis.scale() == "logit") {
            double mp = axis.sharedMinpos();
            if (!std::isfinite(mp)) mp = 1e-7;
            x0 = x0 <= 0 ? mp : x0;
            x1 = x1 >= 1 ? 1 - mp : x1;
        }
        const double x0t = axis.scaleForward(x0), x1t = axis.scaleForward(x1);
        double delta = (x1t - x0t) * margin;
        if (!std::isfinite(delta)) delta = 0;
        x0 = axis.scaleInverse(x0t - delta), x1 = axis.scaleInverse(x1t + delta);
        if (x0bound) x0 = std::max(x0, *x0bound);
        if (x1bound) x1 = std::min(x1, *x1bound);
        if (!tight) std::tie(x0, x1) = axis.locatorViewLimits(x0, x1);
        // set_xbound: sorted, reversed if the axis is inverted
        const auto view = axis.viewInterval();
        const bool inverted = view[0] > view[1];
        if ((x0 > x1) != inverted && x0 != x1) std::swap(x0, x1);
        axis.setLim(x0, x1, std::nullopt);  // auto=None keeps the autoscale state
    };
    handle(scalex, *xaxis, true, xmargin);
    handle(scaley, *yaxis, false, ymargin);
}

void Axes::share(bool x, Axes& other) {
    // Grouper.join(self, other): a group made now lists this Axes before `other`; later ones follow
    std::shared_ptr<SharedAxes>& theirs = other.shared_[x ? 0 : 1];
    if (theirs->members.size() == 1) theirs->members.insert(theirs->members.begin(), this);
    else theirs->members.push_back(this);
    shared_[x ? 0 : 1] = theirs;  // with other's tickers (self.xaxis.major = other.xaxis.major)
    // matplotlib then takes other's limits, autoscaling and scale. Axes are shared as they are made
    // (plt.subplots), when those are still the ones a new Axes has; sharing Axes that have been used
    // needs set_xlim(*other.get_xlim(), emit=False, auto=other.get_autoscalex_on()) and the scale here.
}

Axes* Axes::twin(bool x, std::string* error) {
    if (!subplotSpec) return *error = "a twin of an Axes without a subplot spec is not supported yet", nullptr;
    // (the twins' adjustable becomes 'datalim': an aspect then changes the data limits, which is not ported)
    if (aspect) return *error = "twins of an Axes with an aspect (adjustable 'datalim') are not supported yet", nullptr;
    // _make_twin_axes: add_subplot(ss, sharex=self) (or sharey)
    Axes* t = figure.addSubplot(*subplotSpec);
    Axis& mine = x ? *xaxis : *yaxis;
    Axis& theirs = x ? *t->xaxis : *t->yaxis;
    t->share(x, *this);
    // sharex: other's limits (emit=False, auto=other's), then other's scale
    const auto lim = mine.viewInterval();
    theirs.setScale(mine.scale(), mine.scaleOptions());  // (the same scale: its base, functions...)
    t->updateTransScale();
    theirs.setLim(lim[0], lim[1], mine.autoscaleOn, false);
    // (set_adjustable('datalim') on both: what an aspect would change is refused for twins)
    t->zorder = zorder;
    if (!twinned) twinned = std::make_shared<std::vector<Axes*>>(std::vector<Axes*>{this});
    twinned->push_back(t);
    t->twinned = twinned;
    updateTwinnedPatchVisibility();
    std::string ignored;
    if (x) {  // twinx
        t->yaxis->tickSide("right", &ignored);
        t->yaxis->setLabelPosition("right", &ignored);
        t->yaxis->setOffsetPosition("right", &ignored);
        t->xaxis->autoscaleOn = xaxis->autoscaleOn;
        yaxis->tickSide("left", &ignored);
        t->xaxis->visible = false;
    } else {  // twiny
        t->xaxis->tickSide("top", &ignored);
        t->xaxis->setLabelPosition("top", &ignored);
        t->yaxis->autoscaleOn = yaxis->autoscaleOn;
        xaxis->tickSide("bottom", &ignored);
        t->yaxis->visible = false;
    }
    return t;
}

void Axes::updateTwinnedPatchVisibility() {
    if (!twinned) return;
    std::vector<Axes*> order = *twinned;
    const auto made = [&](const Axes* a) {
        for (std::size_t i = 0; i < figure.axes.size(); ++i)
            if (figure.axes[i].get() == a) return i;
        return figure.axes.size();
    };
    std::stable_sort(order.begin(), order.end(), [&](const Axes* a, const Axes* b) {
        return a->zorder < b->zorder || (a->zorder == b->zorder && made(a) < made(b));
    });
    for (Axes* a : order) a->patch->visible = a == order.front() && a->frameOn;
}

void Axes::labelOuter(bool x) {
    if (!subplotSpec) return;
    const GridSpec& gs = *subplotSpec->gridspec;
    Axis& axis = x ? *xaxis : *yaxis;
    // SubplotSpec.is_first_row / is_last_row / is_first_col / is_last_col
    const int c1 = std::min(subplotSpec->num1 % gs.ncols, subplotSpec->num2 % gs.ncols);
    const int c2 = std::max(subplotSpec->num1 % gs.ncols, subplotSpec->num2 % gs.ncols);
    const bool first = x ? subplotSpec->num1 / gs.ncols == 0 : c1 == 0;
    const bool last = x ? subplotSpec->num2 / gs.ncols + 1 == gs.nrows : c2 + 1 == gs.ncols;
    // The labels and offset texts of the sides that face another Axes go: for x the top one unless
    // in the first row, the bottom one unless in the last row; for y the left one unless in the
    // first column, the right one unless in the last column.
    const bool keep1 = x ? last : first, keep2 = x ? first : last;  // label1: bottom, left
    const auto remove = [&](bool label1) {
        // (matplotlib also empties the axis label on that side; the Axes have none yet when made)
        axis.setLabelsOn(label1, false);
        // offsetText.get_position(): (1, 0) on x, (0, 0.5) on y until the first draw
        const double at = x ? axis.offsetText.y : axis.offsetText.x;
        if (at == (label1 ? 0 : 1)) axis.offsetText.visible = false;
    };
    // matplotlib's order: x top then bottom, y left then right
    if (x) {
        if (!keep2) remove(false);
        if (!keep1) remove(true);
    } else {
        if (!keep1) remove(true);
        if (!keep2) remove(false);
    }
}

void Axes::updateTransScale() {
    transScale->set(transforms::blend(xaxis->scaleTransform(), yaxis->scaleTransform()));
}

void Axes::setScale(bool x, const std::string& name, const AxisScaleOptions& options) {
    // Axis._set_axes_scale: every Axes sharing the axis takes the scale; each sets the shared tickers'
    // locators and formatters anew, so they end up belonging to the last one's axis.
    Axis& axis = x ? *xaxis : *yaxis;
    const double inf = std::numeric_limits<double>::infinity();
    const auto oldDefault = axis.locatorNonsingular(-inf, inf);
    SharedAxes& group = *shared_[x ? 0 : 1];
    for (Axes* ax : group.members) {
        Axis& theirs = x ? *ax->xaxis : *ax->yaxis;
        theirs.setScale(name, options);
        group.tickerAxis = &theirs;
        ax->updateTransScale();
    }
    const auto newDefault = axis.locatorNonsingular(-inf, inf);
    if (oldDefault != newDefault) autoscaleView(x, !x);
}

Spine& Axes::spine(const std::string& type) {
    for (auto& s : spines)
        if (s->type == type) return *s;
    return *spines[0];
}

void Axes::setLimAndTransforms() {
    transAxes = std::make_shared<transforms::BboxTransformTo>(bbox);
    transScale = std::make_shared<transforms::TransformWrapper>(std::make_shared<transforms::IdentityTransform>());
    transLimits = std::make_shared<transforms::BboxTransformFrom>(
        std::make_shared<transforms::TransformedBbox>(viewLim, transScale));
    transData = transforms::compose(transScale, transforms::compose(transLimits, transAxes));
    xaxisTransform = transforms::blend(transData, transAxes);
    yaxisTransform = transforms::blend(transAxes, transData);
}

Axes::TextTransform Axes::xaxisTextTransform(int which, double padPoints) {
    const std::string align = rc_.text("xtick.alignment");
    const double dy = which == 1 ? -1 * padPoints / 72 : padPoints / 72;
    auto offset = std::make_shared<transforms::ScaledTranslation>(0, dy, figure.dpiScaleTrans);
    return {transforms::compose(spine(which == 1 ? "bottom" : "top").spineTransform(), offset),
            which == 1 ? "top" : "bottom", align};
}

Axes::TextTransform Axes::yaxisTextTransform(int which, double padPoints) {
    const std::string align = rc_.text("ytick.alignment");
    const double dx = which == 1 ? -1 * padPoints / 72 : padPoints / 72;
    auto offset = std::make_shared<transforms::ScaledTranslation>(dx, 0, figure.dpiScaleTrans);
    return {transforms::compose(spine(which == 1 ? "left" : "right").spineTransform(), offset), align,
            which == 1 ? "right" : "left"};
}

void Axes::setPosition(const transforms::BboxBase& pos) {
    position->setPoints(pos.points());
    originalPosition->setPoints(pos.points());
}

std::array<double, 4> Axes::locatorPosition() const {
    const auto box = std::make_shared<transforms::TransformedBbox>(
        transforms::Bbox::fromBounds(locator->bounds[0], locator->bounds[1], locator->bounds[2], locator->bounds[3]),
        transforms::subtract(locator->transform, figure.transSubfigure));
    return box->points();
}

Axes* Axes::insetAxes(const std::array<double, 4>& bounds, double z) {
    // the bounds through the locator once (with no renderer) are its original position
    Locator loc{bounds, transAxes};
    const auto box = std::make_shared<transforms::TransformedBbox>(
        transforms::Bbox::fromBounds(bounds[0], bounds[1], bounds[2], bounds[3]), transforms::subtract(transAxes, figure.transSubfigure));
    const auto p = box->points();
    childAxes.push_back(std::make_unique<Axes>(rc_, figure, transforms::Bbox::fromBounds(p[0], p[1], p[2] - p[0], p[3] - p[1]), std::nullopt));
    Axes* inset = childAxes.back().get();
    inset->zorder = z;
    inset->locator = loc;
    inset->label = "inset_axes";
    return inset;
}

Axes* Axes::secondaryAxis(bool x, const std::string& location, double pos, std::shared_ptr<const support::PyLambda> forward,
                          std::shared_ptr<const support::PyLambda> inverse) {
    // SecondaryAxis.__init__: an Axes at [0, 1, 1, 0.0001] (x) or [0, 1, 0.0001, 1] (y) of the figure
    childAxes.push_back(std::make_unique<Axes>(rc_, figure, x ? transforms::Bbox::fromBounds(0, 1., 1, 0.0001)
                                                              : transforms::Bbox::fromBounds(0, 1., 0.0001, 1),
                                               std::nullopt));
    Axes* s = childAxes.back().get();
    Secondary info;
    info.parent = this;
    info.x = x;
    info.locstrings = x ? std::array<std::string, 2>{"top", "bottom"} : std::array<std::string, 2>{"right", "left"};
    s->secondary = info;
    // set_location: from x = 0 to 1 at y = pos (height 1e-10) in the parent's transAxes, or the other way
    (void)location;
    s->locator = Locator{x ? std::array<double, 4>{0, pos, 1., 1e-10} : std::array<double, 4>{pos, 0, 1e-10, 1}, transAxes};
    // set_functions(functions): then _set_scale
    s->secondary->forward = std::move(forward);
    s->secondary->inverse = std::move(inverse);
    s->setSecondaryScale();
    // the other axis: NullLocator(), no ticks; only the spines of this axis
    Axis& other = x ? *s->yaxis : *s->xaxis;
    other.tickerAxis().fixedLocs = std::vector<double>{};
    std::string unused;
    other.setTicksPosition("none", &unused);
    const char* otherSpines[2] = {x ? "left" : "top", x ? "right" : "bottom"};
    for (const char* name : otherSpines) s->spine(name).visible = false;
    for (const auto& name : s->secondary->locstrings) s->spine(name).visible = true;
    if (pos < 0.5) std::swap(s->secondary->locstrings[0], s->secondary->locstrings[1]);
    s->setAlignment(s->secondary->locstrings[0], &unused);
    return s;
}

bool Axes::setAlignment(const std::string& align, std::string* error) {
    auto& loc = secondary->locstrings;
    if (align != loc[0] && align != loc[1])
        return *error = support::pythonStrRepr(align) + " is not a valid value for align; supported values are " +
                        support::pythonStrRepr(loc[0]) + ", " + support::pythonStrRepr(loc[1]),
               false;
    if (align == loc[1]) std::swap(loc[0], loc[1]);  // the orientation changes
    spine(loc[0]).visible = true;
    spine(loc[1]).visible = false;
    Axis& axis = secondary->x ? *xaxis : *yaxis;
    return axis.setTicksPosition(align, error) && axis.setLabelPosition(align, error);
}

void Axes::setSecondaryScale() {
    // a log parent: functionlog of the functions the other way round; any other: function
    const std::string pscale = (secondary->x ? *secondary->parent->xaxis : *secondary->parent->yaxis).scale();
    if (secondary->parentScale && *secondary->parentScale == pscale) return;
    Axis& axis = secondary->x ? *xaxis : *yaxis;
    std::vector<double> ticks;
    if (secondary->ticksSet && axis.tickerAxis().fixedLocs) ticks = *axis.tickerAxis().fixedLocs;  // get_ticklocs()
    AxisScaleOptions options;
    options.forward = secondary->inverse;
    options.inverse = secondary->forward;
    setScale(secondary->x, pscale == "log" ? "functionlog" : "function", options);
    if (secondary->ticksSet) axis.tickerAxis().fixedLocs = ticks;  // set_major_locator(FixedLocator(ticks))
    secondary->parentScale = pscale;
}

void Axes::setSecondaryLims() {
    const Axis& parentAxis = secondary->x ? *secondary->parent->xaxis : *secondary->parent->yaxis;
    const auto lims = parentAxis.viewInterval();  // get_xlim() / get_ylim()
    const bool order = lims[0] < lims[1];
    std::vector<double> out;
    std::string error;
    if (!secondary->forward->apply({lims[0], lims[1]}, &out, &error)) {
        support::noteRaised(error);
        return;
    }
    const bool neworder = out[0] < out[1];
    if (neworder != order) std::swap(out[0], out[1]);  // the transform takes care of the flipping
    // set_xlim(lims) / set_ylim(lims): _validate_converted_limits
    if (!std::isfinite(out[0]) || !std::isfinite(out[1])) {
        support::noteRaised("Axis limits cannot be NaN or Inf");
        return;
    }
    (secondary->x ? *xaxis : *yaxis).setLim(out[0], out[1], false);
}

void Axes::setSecondaryColor(const colors::Spec& color) {
    Axis& axis = secondary->x ? *xaxis : *yaxis;
    TickParams p;
    p.color = color;
    p.labelcolor = color;
    axis.setTickParams("major", false, p);
    for (auto& sp : spines)
        if (sp->axis == &axis) sp->setEdgecolor(color);  // Spine.set_color
    axis.label.color = *colors::toRgba(color, std::nullopt, rc_, nullptr);
}

void Axes::applyAspect() {
    if (secondary) setSecondaryLims();  // SecondaryAxis.apply_aspect: _set_lims first
    if (aspectFrozen) return;
    // apply_aspect(locator(self, renderer) if locator else None)
    const auto original = locator ? locatorPosition() : originalPosition->points();
    if (!aspect) {  // 'auto': the original position
        position->setPoints(original);
        return;
    }
    // fig_aspect: of Bbox.unit() through transSubfigure
    const auto f = figure.bbox->points();
    const double figAspect = (f[3] - f[1]) / (f[2] - f[0]);
    // get_data_ratio: the bounds through the scales
    const auto bound = [](const Axis& axis) {
        const auto v = axis.viewInterval();
        const double lo = v[0] < v[1] ? v[0] : v[1], hi = v[0] < v[1] ? v[1] : v[0];  // get_xbound
        return std::array<double, 2>{axis.scaleForward(lo), axis.scaleForward(hi)};
    };
    const auto tx = bound(*xaxis), ty = bound(*yaxis);
    const double xsize = std::max(std::fabs(tx[1] - tx[0]), 1e-30), ysize = std::max(std::fabs(ty[1] - ty[0]), 1e-30);
    const double boxAspect = *aspect * (ysize / xsize);
    // Bbox.shrunk_to_aspect(box_aspect, pb, fig_aspect), then anchored(anchor, pb)
    const double w = original[2] - original[0], h = original[3] - original[1];
    double H = w * boxAspect / figAspect, W = w;
    if (!(H <= h)) W = h * figAspect / boxAspect, H = h;
    // the shrunk box holds its corners; anchored reads its bounds back from them
    const double x1 = original[0] + W, y1 = original[1] + H;
    const double dx = (original[0] + anchor[0] * (w - (x1 - original[0]))) - original[0];
    const double dy = (original[1] + anchor[1] * (h - (y1 - original[1]))) - original[1];
    position->setPoints({original[0] + dx, original[1] + dy, x1 + dx, y1 + dy});
}

std::optional<transforms::Bbox> Axes::tightbbox(Renderer& renderer, bool forLayoutOnly) {
    if (!visible) return std::nullopt;
    applyAspect();
    std::vector<transforms::Bbox> bb;
    if (axisOn)
        for (Axis* axis : {xaxis.get(), yaxis.get()})
            if (axis->visible)
                if (auto ba = axis->tightbbox(renderer, forLayoutOnly)) bb.push_back(*ba);
    updateTitlePosition(renderer);
    bb.push_back(transforms::Bbox(bbox->points()));
    for (Text* t : {title.get(), leftTitle.get(), rightTitle.get()}) {
        if (!t->visible) continue;
        auto p = t->windowExtent(renderer).points();
        if (forLayoutOnly && p[2] - p[0] > 0) {
            p[0] = (p[0] + p[2]) / 2 - 0.5;
            p[2] = p[0] + 1.0;
        }
        bb.push_back(transforms::Bbox(p));
    }
    // get_default_bbox_extra_artists: the visible children not fully clipped to the Axes, the spines,
    // the legend and the patch
    const auto extra = [&](const std::optional<transforms::Bbox>& b) {
        if (b && isFiniteBox(*b)) bb.push_back(*b);
    };
    for (auto& c : children) {
        if (!c->visible) continue;
        // The children are clipped by the patch's box (set_clip_path(self.patch): a box, no path).
        // matplotlib leaves out those whose clip box equals the Axes' box (_fully_clipped_to_axes);
        // the two can differ in the last bits, and then it adds the child's window extent cut by the
        // clip box. That lies inside the clip box, which is the patch's window extent, added below:
        // the union is the same either way.
        if (c->clipOn && c->clipBox && !c->clipPath && patch->visible) continue;
        // an inset indicator is clipped by the Axes' patch (add_artist): fully clipped
        if (dynamic_cast<const InsetIndicator*>(c.get())) continue;
        // (without a patch drawn: left out when the clip box is the Axes' box, _fully_clipped_to_axes)
        if (c->clipOn && c->clipBox && !c->clipPath && c->clipBox->points() == bbox->points()) continue;
        // (clip_on without a clip box or path, as an annotation's, clips nothing)
        if (auto* t = dynamic_cast<Text*>(c.get()); t && (!c->clipOn || (!c->clipBox && !c->clipPath))) {
            // Annotation.get_tightbbox: nothing where _check_xy says it is not drawn
            if (auto* a = dynamic_cast<Annotation*>(t); a && !a->checkXY(renderer)) continue;
            // Text.get_tightbbox: nothing for an empty text, nor for one in data coordinates outside the
            // domain of the scales (_point_in_data_domain: val_in_range is finite, and > 0 on a log scale)
            const auto inDomain = [](const Axis& axis, double v) {
                return std::isfinite(v) && (!axis.logScale() || v > 0) && (axis.scale() != "logit" || (0 < v && v < 1));
            };
            if (t->text.empty()) continue;
            if (transforms::equal(*t->transform, *transData) && !(inDomain(*xaxis, t->x) && inDomain(*yaxis, t->y))) continue;
            extra(t->windowExtent(renderer));  // Artist.get_tightbbox of an artist that is not clipped
            continue;
        }
        if (auto* p = dynamic_cast<Patch*>(c.get()); p && (!c->clipOn || (!c->clipBox && !c->clipPath))) {
            extra(p->windowExtent());  // Artist.get_tightbbox: Patch.get_window_extent
            continue;
        }
        renderer.unsupported("tight boxes of artists not clipped to the Axes are not supported yet");
    }
    if (axisOn && frameOn)
        for (auto& s : spines)
            if (s->visible) extra(s->windowExtent());
    for (auto& child : childAxes)  // (the child Axes come before the legend in get_children)
        if (child->visible) extra(child->tightbbox(renderer, false));
    if (legend && legend->visible) extra(static_cast<Legend&>(*legend).tightbbox(renderer));
    if (patch->visible) {
        // Patch.get_window_extent: the path's extents (its MOVETO and LINETO vertices) through its transform
        const Path path = patch->path();
        const auto m = patch->fullTransform()->matrix();
        std::vector<transforms::Bbox> points;
        for (std::size_t k = 0; k < path.codes->size(); ++k) {
            const auto code = (*path.codes)[k];
            if (code != MOVETO && code != LINETO) continue;
            const auto q = transforms::apply(m, (*path.vertices)[2 * k], (*path.vertices)[2 * k + 1]);
            points.push_back(transforms::Bbox({q[0], q[1], q[0], q[1]}));
        }
        extra(unionOf(points));
    }
    std::vector<transforms::Bbox> sized;
    for (const auto& b : bb)
        if (b.width() != 0 || b.height() != 0) sized.push_back(b);
    return unionOf(sized);
}

void Axes::draw(Renderer& renderer) {
    if (!visible) return;
    if (secondary) {  // SecondaryAxis.draw: _set_lims, _set_scale
        setSecondaryLims();
        setSecondaryScale();
    }
    unstaleViewLim();
    renderer.openGroup("axes", gid);
    applyAspect();
    // get_children without the patch: children, spines, axes, titles; sorted by zorder (stable)
    std::vector<Artist*> artists;
    for (auto& c : children) artists.push_back(c.get());
    if (axisOn && frameOn)
        for (auto& s : spines) artists.push_back(s.get());
    if (axisOn) {
        artists.push_back(xaxis.get());
        artists.push_back(yaxis.get());
    }
    updateTitlePosition(renderer);
    for (Text* t : {title.get(), leftTitle.get(), rightTitle.get()}) artists.push_back(t);
    for (auto& child : childAxes) artists.push_back(child.get());
    if (legend) artists.push_back(legend.get());
    std::stable_sort(artists.begin(), artists.end(), [](const Artist* a, const Artist* b) { return a->zorder < b->zorder; });
    if (axisOn && frameOn) artists.insert(artists.begin(), patch.get());
    for (Artist* a : artists) a->draw(renderer);
    renderer.closeGroup("axes");
}

}  // namespace calculate_plot::engine
