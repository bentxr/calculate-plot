// Derived from Matplotlib 3.11.2: lib/matplotlib/axes/_base.py (_AxesBase), lib/matplotlib/axis.py
// (Tick, XTick, YTick, Axis, XAxis, YAxis) and lib/matplotlib/spines.py (Spine).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/axes.hpp"

#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

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

// A font size given as a number or a name (Text(fontsize=...)).
double fontSizeOf(const rc::Params& rc, const char* key) {
    const auto& v = rc.value(key);
    if (v.isNumber()) return v.asNumber();
    return *text::fontSize(v.asString(), rc);
}

// mtransforms._interval_contains_close
bool intervalContainsClose(double a, double b, double val, double rtol = 1e-10) {
    if (a > b) std::swap(a, b);
    rtol = (b - a) * rtol;
    return a - rtol <= val && val <= b + rtol;
}

// Bbox.union
transforms::Bbox unionOf(const std::vector<transforms::Bbox>& boxes) {
    double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const auto p = boxes[i].points();
        const double xmin = std::min(p[0], p[2]), xmax = std::max(p[0], p[2]);
        const double ymin = std::min(p[1], p[3]), ymax = std::max(p[1], p[3]);
        if (i == 0 || xmin < x0) x0 = xmin;
        if (i == 0 || xmax > x1) x1 = xmax;
        if (i == 0 || ymin < y0) y0 = ymin;
        if (i == 0 || ymax > y1) y1 = ymax;
    }
    return transforms::Bbox({x0, y0, x1, y1});
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
    const transforms::Matrix m = transforms::compose(patchTransform(), transform)->matrix();
    const std::vector<double>& v = *path_.vertices;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
        const auto q = transforms::apply(m, v[i], v[i + 1]);
        if (i == 0 || q[0] < x0) x0 = q[0];
        if (i == 0 || q[0] > x1) x1 = q[0];
        if (i == 0 || q[1] < y0) y0 = q[1];
        if (i == 0 || q[1] > y1) y1 = q[1];
    }
    const transforms::Bbox bb({x0, y0, x1, y1});
    if (!axis || !axis->visible) return bb;
    std::vector<transforms::Bbox> boxes{bb};
    const std::vector<Tick*> drawn = axis->updateTicks();
    if (!drawn.empty()) {
        const Tick& tick = *drawn.front();
        auto p = bb.points();
        double padout = 0.5, padin = 0.5;
        if (tick.tickdir() == "out") padout = 1, padin = 0;
        else if (tick.tickdir() == "in") padout = 0, padin = 1;
        const double dpi = axes_.figure.dpi;
        padout = padout * tick.size() / 72 * dpi;
        padin = padin * tick.size() / 72 * dpi;
        if (tick.tick1line.visible) {
            if (type == "left") p[0] = p[0] - padout, p[2] = p[2] + padin;
            else if (type == "bottom") p[1] = p[1] - padout, p[3] = p[3] + padin;
        }
        if (tick.tick2line.visible) {
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
      isX_(isX) {
    const std::string name = isX ? "xtick" : "ytick";
    const std::string mm = major ? "major" : "minor";
    size_ = rc.number(name + "." + mm + ".size");
    width_ = rc.number(name + "." + mm + ".width");
    basePad_ = rc.number(name + "." + mm + ".pad");
    const std::string color = rc.text(name + ".color");
    std::string labelcolor = rc.text(name + ".labelcolor");
    if (labelcolor == "inherit") labelcolor = rc.text(name + ".color");
    const double labelsize = fontSizeOf(rc, (name + ".labelsize").c_str());
    zorder = major ? 2 + 0.01 : 2;  // Line2D.zorder (+ 0.01)

    std::string gridColor = mm == "major" ? rcText(rc, {"grid.major.color", "grid.color"}) : rcText(rc, {"grid.minor.color", "grid.color"});
    const std::string gridLinestyle = mm == "major" ? rcText(rc, {"grid.major.linestyle", "grid.linestyle"})
                                                    : rcText(rc, {"grid.minor.linestyle", "grid.linestyle"});
    const double gridLinewidth = mm == "major" ? rcNumber(rc, {"grid.major.linewidth", "grid.linewidth"})
                                               : rcNumber(rc, {"grid.minor.linewidth", "grid.linewidth"});
    std::optional<double> gridAlpha;
    if (gridColor[0] == '#' && (gridColor.size() == 5 || gridColor.size() == 9)) {  // _has_alpha_channel
        const auto rgba = *colors::toRgba(gridColor, std::nullopt, rc, nullptr);
        gridAlpha = rgba[3];
    } else {
        gridAlpha = mm == "major" ? rcNumber(rc, {"grid.major.alpha", "grid.alpha"}) : rcNumber(rc, {"grid.minor.alpha", "grid.alpha"});
    }
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
    gridline.visible = sw.gridOn;
    gridline.setLinestyle(gridLinestyle, &error);
    gridline.setLinewidth(gridLinewidth);
    gridline.setMarker("", &error);
    for (Text* tickLabel : {&label1, &label2}) {
        tickLabel->font.size = labelsize;
        tickLabel->setColor(labelcolor);
        tickLabel->figure = &axes.figure;
    }
    label1.visible = sw.label1On;
    label2.visible = sw.label2On;
    // _apply_tickdir
    tickdir_ = rc.text(name + ".direction");
    if (isX) {
        tick1line.setMarker(tickdir_ == "out" ? "tickdown" : tickdir_ == "in" ? "tickup" : "|", &error);
        tick2line.setMarker(tickdir_ == "out" ? "tickup" : tickdir_ == "in" ? "tickdown" : "|", &error);
    } else {
        tick1line.setMarker(tickdir_ == "out" ? "tickleft" : tickdir_ == "in" ? "tickright" : "_", &error);
        tick2line.setMarker(tickdir_ == "out" ? "tickright" : tickdir_ == "in" ? "tickleft" : "_", &error);
    }
    updatePosition(0);

    // XTick / YTick
    if (isX) {
        tick1line.setData({0}, {0});
        tick1line.transform = axes.spine("bottom").spineTransform();
        tick2line.setData({0}, {1});
        tick2line.transform = axes.spine("top").spineTransform();
        gridline.setData({0, 0}, {0, 1});
        gridline.transform = axes.xaxisTransform;
        const auto t1 = axes.xaxisTextTransform(1, pad());
        label1.x = 0, label1.y = 0, label1.verticalAlignment = t1.va, label1.horizontalAlignment = t1.ha;
        label1.transform = t1.transform;
        const auto t2 = axes.xaxisTextTransform(2, pad());
        label2.x = 0, label2.y = 1, label2.verticalAlignment = t2.va, label2.horizontalAlignment = t2.ha;
        label2.transform = t2.transform;
    } else {
        tick1line.setData({0}, {0});
        tick1line.transform = axes.spine("left").spineTransform();
        tick2line.setData({1}, {0});
        tick2line.transform = axes.spine("right").spineTransform();
        gridline.setData({0, 1}, {0, 0});
        gridline.transform = axes.yaxisTransform;
        const auto t1 = axes.yaxisTextTransform(1, pad());
        label1.x = 0, label1.y = 0, label1.verticalAlignment = t1.va, label1.horizontalAlignment = t1.ha;
        label1.transform = t1.transform;
        const auto t2 = axes.yaxisTextTransform(2, pad());
        label2.x = 1, label2.y = 0, label2.verticalAlignment = t2.va, label2.horizontalAlignment = t2.ha;
        label2.transform = t2.transform;
    }
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
    zorder = 1.5;  // set_axisbelow('line')
    labelpad = rc.number("axes.labelpad");
    labelPosition = isX ? "bottom" : "left";
    locator.roundNumbers = rc.text("axes.autolimit_mode") == "round_numbers";
}

void Axis::setViewInterval(double vmin, double vmax) {
    auto p = axes_.viewLim->points();
    if (isX_) p[0] = vmin, p[2] = vmax;
    else p[1] = vmin, p[3] = vmax;
    axes_.viewLim->setPoints(p);
}

void Axis::setLim(double v0, double v1, bool autoscale) {
    if (scale_ == "log" && (v0 <= 0 || v1 <= 0)) {  // non-positive limits are ignored (with a warning)
        const auto old = viewInterval();
        if (v0 <= 0) v0 = old[0];
        if (v1 <= 0) v1 = old[1];
    }
    const bool reverse = v0 > v1;
    std::tie(v0, v1) = locatorNonsingular(v0, v1);
    if (scale_ == "log") {  // LogScale.limit_range_for_scale
        double mp = minpos();
        if (!std::isfinite(mp)) mp = 1e-300;
        v0 = v0 <= 0 ? mp : v0;
        v1 = v1 <= 0 ? mp : v1;
    }
    // sorted([v0, v1], reverse=reverse)
    if ((v0 > v1) != reverse && v0 != v1) std::swap(v0, v1);
    setViewInterval(v0, v1);
    axes_.clearStale(isX_);  // _stale_viewlims[name] = False
    autoscaleOn = autoscale;
}

void Axis::setScale(const std::string& name) {
    scale_ = name;
    if (name == "log") {  // LogScale.set_default_locators_and_formatters
        logLocator = axes::LogLocator();
        logMinorLocator = axes::LogLocator();
        logMinorLocator.subs.clear();
        logMinorLocator.autoSubs = true;
        logFormatter = axes::LogFormatterSciNotation();
        logMinorFormatter = axes::LogFormatterSciNotation();
    } else {
        locator = axes::MaxNLocator::autoLocator();
        locator.roundNumbers = rc_.text("axes.autolimit_mode") == "round_numbers";
        formatter = axes::ScalarFormatter();
    }
}

transforms::TransformPtr Axis::scaleTransform() const {
    if (scale_ == "log") return std::make_shared<transforms::LogTransform>(10.0);
    return std::make_shared<transforms::IdentityTransform>();
}

double Axis::scaleForward(double v) const { return scale_ == "log" ? transforms::LogTransform(10.0).transform1d(v) : v; }
double Axis::scaleInverse(double v) const { return scale_ == "log" ? transforms::LogTransform(10.0).inverse1d(v) : v; }
double Axis::minpos() const { return axes_.minpos[isX_ ? 0 : 1]; }

std::pair<double, double> Axis::locatorNonsingular(double v0, double v1) {
    if (scale_ == "log") {
        logLocator.minpos = minpos();
        return logLocator.nonsingular(v0, v1);
    }
    return axes::nonsingular(v0, v1, 0.05);  // Locator.nonsingular
}

std::pair<double, double> Axis::locatorViewLimits(double v0, double v1) {
    if (scale_ == "log") {
        logLocator.minpos = minpos();
        return logLocator.viewLimits(v0, v1, rc_.text("axes.autolimit_mode") == "round_numbers");
    }
    locator.tickSpace = tickSpace();  // the locator's axis
    return locator.viewLimits(v0, v1);
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

std::string Axis::offsetString() const { return scale_ == "log" ? std::string() : formatter.offsetText(); }

void Axis::setLabelText(const std::string& t, const std::string& loc) {
    label.text = t;
    if (isX_) label.x = loc == "left" ? 0 : loc == "right" ? 1 : 0.5;
    else label.y = loc == "bottom" ? 0 : loc == "top" ? 1 : 0.5;
    label.horizontalAlignment = isX_ ? loc : (loc == "bottom" ? "left" : loc == "top" ? "right" : "center");
}

std::string Axis::ticksPosition() const {
    // Axis._get_ticks_position over the major ticks' first tick and label
    if (majorTicks_.empty()) return isX_ ? "bottom" : "left";
    const Tick& t = *majorTicks_.front();
    const bool one = t.tick1line.visible || t.label1.visible;
    const bool two = t.tick2line.visible || t.label2.visible;
    if (one && !two) return isX_ ? "bottom" : "left";
    if (two && !one) return isX_ ? "top" : "right";
    if (one && two) return "default";
    return "unknown";
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
        // _tick_position stays 'bottom' unless the ticks are moved (set_ticks_position)
        const double bottom = bboxes.empty() ? std::min(box[1], box[3]) : unionOf(bboxes).points()[1];
        offsetText.y = bottom - kOffsetTextPad * dpi / 72;
        (void)bboxes2;
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
}

std::vector<Tick*> Axis::majorTicks(std::size_t count) {
    while (majorTicks_.size() < count)
        majorTicks_.push_back(std::make_unique<Tick>(rc_, axes_, isX_, true, majorTickKw_));
    std::vector<Tick*> out;
    for (std::size_t i = 0; i < count; ++i) out.push_back(majorTicks_[i].get());
    return out;
}

int Axis::tickSpace() const {
    // Bbox.unit().transformed(transAxes - dpi_scale_trans): transAxes, then the inches' inverse
    const auto t = transforms::compose(axes_.transAxes, axes_.figure.dpiScaleTrans->inverted());
    const auto ll = t->transform(0, 0), ul = t->transform(0, 1), lr = t->transform(1, 0);
    const double length = (isX_ ? lr[0] - ll[0] : ul[1] - ll[1]) * 72;
    // _get_tick_label_size: rcParams[xtick.labelsize] (tick_params(labelsize=) is not supported yet)
    const double size = fontSizeOf(rc_, isX_ ? "xtick.labelsize" : "ytick.labelsize") * (isX_ ? 3 : 2);
    if (size > 0) return int(std::floor(length / size));
    return 2147483647;
}

std::vector<Tick*> Axis::updateTicks() {
    const auto view = viewInterval();
    const auto groupVisible = [](const TickSwitches& kw) {
        return kw.tick1On || kw.tick2On || kw.label1On || kw.label2On || kw.gridOn;
    };
    std::vector<Tick*> ticks;
    if (groupVisible(majorTickKw_)) {
        std::vector<double> locs;
        std::vector<std::string> labels;
        if (scale_ == "log") {
            logLocator.tickSpace = tickSpace();
            logLocator.minpos = minpos();
            locs = logLocator.tickValues(view[0], view[1]);
            logFormatter.setLocs(view[0], view[1]);
            for (double v : locs) labels.push_back(logFormatter(v));
        } else {
            locator.tickSpace = tickSpace();
            locs = locator.tickValues(view[0], view[1]);
            formatter.setViewInterval(view[0], view[1]);
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
    if (groupVisible(minorTickKw_) && scale_ == "log") {
        logMinorLocator.tickSpace = tickSpace();
        logMinorLocator.minpos = minpos();
        logLocator.tickSpace = tickSpace();
        logLocator.minpos = minpos();
        const auto locs = axes::withoutMajorTicks(logMinorLocator.tickValues(view[0], view[1]),
                                                  logLocator.tickValues(view[0], view[1]), view[0], view[1], 10.0);
        logMinorFormatter.setLocs(view[0], view[1]);
        const std::vector<Tick*> minor = minorTicks(locs.size());
        for (std::size_t i = 0; i < locs.size(); ++i) {
            minor[i]->updatePosition(locs[i]);
            minor[i]->label1.text = minor[i]->label2.text = logMinorFormatter(locs[i]);
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

// --- Axes ----------------------------------------------------------------------------------------

Axes::Axes(const rc::Params& rc, Figure& fig, const SubplotSpec& spec)
    : Artist(rc), figure(fig), xmargin(rc.number("axes.xmargin")), ymargin(rc.number("axes.ymargin")) {
    position = spec.position(fig.subplotpars);
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
        s.gridOn = rc.boolean("axes.grid") && (rc.text("axes.grid.which") == "both" || rc.text("axes.grid.which") == "major");
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
        s.gridOn = rc.boolean("axes.grid") && (rc.text("axes.grid.which") == "both" || rc.text("axes.grid.which") == "minor");
        return s;
    };
    updateTransScale();  // the end of clear()
    xaxis->setMinorTickSwitches(minorSw(true));
    yaxis->setMinorTickSwitches(minorSw(false));
    if (rc.boolean("xtick.minor.visible") || rc.boolean("ytick.minor.visible")) minorTicksUnsupported_ = true;
}

void Axes::setTitleOffsetTrans(double points) {
    auto offset = std::make_shared<transforms::ScaledTranslation>(0.0, points / 72, figure.dpiScaleTrans);
    for (Text* t : {title.get(), leftTitle.get(), rightTitle.get()}) {
        t->transform = transforms::compose(transAxes, offset);
        t->clipBox.reset();
    }
}

void Axes::setTitle(const std::string& titleText, const std::string& loc, std::optional<double> pad, std::optional<double> y) {
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
    const auto& size = rc_.value("axes.titlesize");
    t.font.size = size.isNumber() ? size.asNumber() : *text::fontSize(size.asString(), rc_);
    const auto& weight = rc_.value("axes.titleweight");
    t.font.weight = weight.isNumber() ? text::FontValue(int(weight.asNumber())) : text::FontValue(weight.asString());
    t.verticalAlignment = "baseline";
    t.horizontalAlignment = loc;
    const std::string color = rc_.text("axes.titlecolor");
    if (color != "auto" && color != "AUTO" && color != "Auto") t.setColor(color);
    t.y = ty;
}

void Axes::updateTitlePosition(Renderer& renderer) {
    if (!autotitlepos_) return;
    Text* titles[3] = {title.get(), leftTitle.get(), rightTitle.get()};
    if (titles[0]->text.empty() && titles[1]->text.empty() && titles[2]->text.empty()) return;
    // The siblings are this Axes alone (no twins, children or aligned titles yet).
    const std::string ticks = xaxis->ticksPosition();
    if (ticks == "top" || ticks == "unknown" || xaxis->labelPosition == "top") {
        renderer.unsupported("titles over top ticks or labels are not supported yet");
        return;
    }
    // ax.spines.get("outline", ax).get_window_extent(): the Axes' own box
    const auto box = bbox->points();
    const double top = std::max(box[1], box[3]);
    for (Text* t : titles) {
        t->y = 1.0;
        if (!t->text.empty()) {
            yaxis->updateTicks();  // ax.yaxis.get_tightbbox(renderer) updates the offset text
            if (yaxis->offsetText.visible && !yaxis->offsetText.text.empty())
                renderer.unsupported("titles beside a y offset text are not supported yet");
            if (top < 0) return;
            if (t->windowExtent(renderer).points()[1] < top) {
                const auto inv = transAxes->inverted();
                t->y = inv->transform(0, top)[1];
                const double ymin = t->windowExtent(renderer).points()[1];
                if (ymin < top) t->y = inv->transform(0., 2 * top - ymin)[1];
            }
        }
    }
    double ymax = titles[0]->y;
    for (Text* t : titles) ymax = std::max(ymax, t->y);
    for (Text* t : titles) t->y = ymax;
}

Line2D& Axes::addLine(std::unique_ptr<Line2D> line) {
    // _set_artist_props: the data transform unless one was set
    line->transform = transData;
    // set_clip_path(self.patch): a Rectangle clips by its box
    line->clipBox = std::make_shared<transforms::TransformedBbox>(
        std::make_shared<transforms::Bbox>(std::array<double, 4>{0, 0, 1, 1}), patch->fullTransform());
    // _update_line_limits: the transform is transData, so the data path updates both limits
    const Path& path = line->path();
    if (path.size()) {
        auto points = ignoreExistingDataLimits_
                          ? std::array<double, 4>{std::numeric_limits<double>::infinity(),
                                                  std::numeric_limits<double>::infinity(),
                                                  -std::numeric_limits<double>::infinity(),
                                                  -std::numeric_limits<double>::infinity()}
                          : dataLim->points();
        const std::vector<double>& v = *path.vertices;
        const double inf = std::numeric_limits<double>::infinity();
        double minx = inf, maxx = -inf, miny = inf, maxy = -inf, posx = inf, posy = inf;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            if (!std::isfinite(v[i]) || !std::isfinite(v[i + 1])) continue;
            minx = std::min(minx, v[i]), maxx = std::max(maxx, v[i]);
            miny = std::min(miny, v[i + 1]), maxy = std::max(maxy, v[i + 1]);
            if (v[i] > 0) posx = std::min(posx, v[i]);  // the smallest positive values (minpos)
            if (v[i + 1] > 0) posy = std::min(posy, v[i + 1]);
        }
        if (ignoreExistingDataLimits_) minpos = {inf, inf};
        minpos[0] = posx < minpos[0] ? posx : minpos[0];
        minpos[1] = posy < minpos[1] ? posy : minpos[1];
        // Python's min(a, b) / max(a, b): the first unless the second is beyond it
        points[0] = minx < points[0] ? minx : points[0];
        points[2] = maxx > points[2] ? maxx : points[2];
        points[1] = miny < points[1] ? miny : points[1];
        points[3] = maxy > points[3] ? maxy : points[3];
        dataLim->setPoints(points);
        ignoreExistingDataLimits_ = false;
    }
    if (line->label.empty()) line->label = "_child" + std::to_string(children.size());
    children.push_back(std::move(line));
    return static_cast<Line2D&>(*children.back());
}

colors::Spec Axes::nextColor() {
    const auto cycle = rc_.colorCycle();
    if (cycle.empty()) return std::string("k");
    colors::Spec c = cycle[cycleIndex_];
    cycleIndex_ = (cycleIndex_ + 1) % cycle.size();
    return c;
}

colors::Spec Axes::nextPatchColor() {
    const auto cycle = rc_.colorCycle();
    if (cycle.empty()) return std::string("k");
    colors::Spec c = cycle[patchCycleIndex_];
    patchCycleIndex_ = (patchCycleIndex_ + 1) % cycle.size();
    return c;
}

void Axes::updateDatalim(const std::vector<std::array<double, 2>>& xys) {
    // update_datalim: nothing unless some value is finite; then Bbox.update_from_data_xy
    bool anyFinite = false;
    for (const auto& p : xys) anyFinite = anyFinite || std::isfinite(p[0]) || std::isfinite(p[1]);
    if (!anyFinite) return;
    const double inf = std::numeric_limits<double>::infinity();
    auto points = ignoreExistingDataLimits_ ? std::array<double, 4>{inf, inf, -inf, -inf} : dataLim->points();
    if (ignoreExistingDataLimits_) minpos = {inf, inf};
    double minx = inf, maxx = -inf, miny = inf, maxy = -inf, posx = inf, posy = inf;
    for (const auto& p : xys) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1])) continue;
        minx = std::min(minx, p[0]), maxx = std::max(maxx, p[0]);
        miny = std::min(miny, p[1]), maxy = std::max(maxy, p[1]);
        if (p[0] > 0) posx = std::min(posx, p[0]);
        if (p[1] > 0) posy = std::min(posy, p[1]);
    }
    points[0] = minx < points[0] ? minx : points[0];
    points[2] = maxx > points[2] ? maxx : points[2];
    points[1] = miny < points[1] ? miny : points[1];
    points[3] = maxy > points[3] ? maxy : points[3];
    minpos[0] = posx < minpos[0] ? posx : minpos[0];
    minpos[1] = posy < minpos[1] ? posy : minpos[1];
    dataLim->setPoints(points);
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
    const auto d = c->dataLim();
    std::vector<std::array<double, 2>> points{{d.points[0], d.points[1]}, {d.points[2], d.points[3]}};
    if (std::isfinite(d.minpos[0]) || std::isfinite(d.minpos[1])) points.push_back(d.minpos);
    updateDatalim(points);
    requestAutoscaleView(true, true);
    Collection& ref = *c;
    children.push_back(std::move(c));
    return ref;
}

void Axes::requestAutoscaleView(bool x, bool y) {
    staleX_ = staleX_ || x;
    staleY_ = staleY_ || y;
}

void Axes::unstaleViewLim() {
    if (!staleX_ && !staleY_) return;
    const bool x = staleX_, y = staleY_;
    staleX_ = staleY_ = false;
    autoscaleView(x, y);
}

void Axes::autoscaleView(bool scalex, bool scaley) {
    // No artist has sticky edges yet (lines have none).
    const auto handle = [&](bool scale, Axis& axis, bool isX, double margin) {
        if (!(scale && axis.autoscaleOn)) return;
        const auto d = dataLim->points();
        const double a = isX ? d[0] : d[1], b = isX ? d[2] : d[3];
        double x0, x1;
        if (std::isfinite(a) || std::isfinite(b)) {
            // min/max of the finite ones
            if (std::isfinite(a) && std::isfinite(b)) x0 = b < a ? b : a, x1 = b > a ? b : a;
            else x0 = x1 = std::isfinite(a) ? a : b;
        } else {
            const auto v = viewLim->points();
            const bool mutated = isX ? (v[0] != 0 || v[2] != 1) : (v[1] != 0 || v[3] != 1);
            if (mutated) return;
            x0 = -std::numeric_limits<double>::infinity();
            x1 = std::numeric_limits<double>::infinity();
        }
        std::tie(x0, x1) = axis.locatorNonsingular(x0, x1);
        if (axis.scale() == "log") {  // limit_range_for_scale(x0, x1, minimum_minpos)
            double mp = axis.minpos();
            if (!std::isfinite(mp)) mp = 1e-300;
            x0 = x0 <= 0 ? mp : x0;
            x1 = x1 <= 0 ? mp : x1;
        }
        const double x0t = axis.scaleForward(x0), x1t = axis.scaleForward(x1);
        double delta = (x1t - x0t) * margin;
        if (!std::isfinite(delta)) delta = 0;
        x0 = axis.scaleInverse(x0t - delta), x1 = axis.scaleInverse(x1t + delta);
        if (!tight) std::tie(x0, x1) = axis.locatorViewLimits(x0, x1);
        // set_xbound: sorted, reversed if the axis is inverted
        const auto view = axis.viewInterval();
        const bool inverted = view[0] > view[1];
        if ((x0 > x1) != inverted && x0 != x1) std::swap(x0, x1);
        const bool keep = axis.autoscaleOn;
        axis.setLim(x0, x1, keep);  // auto=None keeps the autoscale state
    };
    handle(scalex, *xaxis, true, xmargin);
    handle(scaley, *yaxis, false, ymargin);
}

void Axes::updateTransScale() {
    transScale->set(transforms::blend(xaxis->scaleTransform(), yaxis->scaleTransform()));
}

void Axes::setScale(bool x, const std::string& name) {
    Axis& axis = x ? *xaxis : *yaxis;
    const double inf = std::numeric_limits<double>::infinity();
    const auto oldDefault = axis.locatorNonsingular(-inf, inf);
    axis.setScale(name);
    updateTransScale();
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

void Axes::draw(Renderer& renderer) {
    if (!visible) return;
    unstaleViewLim();
    if (minorTicksUnsupported_) renderer.unsupported("minor ticks on linear scales (AutoMinorLocator) are not supported yet");
    renderer.openGroup("axes", gid);
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
    std::stable_sort(artists.begin(), artists.end(), [](const Artist* a, const Artist* b) { return a->zorder < b->zorder; });
    if (axisOn && frameOn) artists.insert(artists.begin(), patch.get());
    for (Artist* a : artists) a->draw(renderer);
    renderer.closeGroup("axes");
}

}  // namespace calculate_plot::engine
