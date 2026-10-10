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
    for (Text* label : {&label1, &label2}) {
        label->font.size = labelsize;
        label->setColor(labelcolor);
        label->figure = &axes.figure;
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
}

std::array<double, 2> Axis::viewInterval() const {
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

std::vector<Tick*> Axis::updateTicks() {
    const auto view = viewInterval();
    const std::vector<double> locs = locator.tickValues(view[0], view[1]);
    formatter.setViewInterval(view[0], view[1]);
    formatter.setLocs(locs);
    const std::vector<Tick*> ticks = majorTicks(locs.size());
    for (std::size_t i = 0; i < locs.size(); ++i) {
        ticks[i]->updatePosition(locs[i]);
        ticks[i]->label1.text = formatter(locs[i]);
        ticks[i]->label2.text = ticks[i]->label1.text;
    }
    double low = view[0], high = view[1];
    if (low > high) std::swap(low, high);
    std::vector<Tick*> toDraw;  // the linear scale's transform is the identity
    for (Tick* t : ticks)
        if (intervalContainsClose(low, high, t->loc())) toDraw.push_back(t);
    return toDraw;
}

void Axis::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup("matplotlib.axis", gid);
    const std::vector<Tick*> ticks = updateTicks();
    for (Tick* t : ticks) t->draw(renderer);
    if (!label.text.empty()) renderer.unsupported("axis labels are not supported yet");
    offsetText.text = formatter.offsetText();
    if (!offsetText.text.empty()) renderer.unsupported("axis offset texts are not supported yet");
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
    yaxis = std::make_unique<Axis>(rc, *this, false);

    // clear(): the titles and the patch; the view limits (0, 1) stay.
    const double titleSize = fontSizeOf(rc, "axes.titlesize");
    const text::FontValue titleWeight = rc.value("axes.titleweight").isNumber()
                                            ? text::FontValue(int(rc.number("axes.titleweight")))
                                            : text::FontValue(rc.text("axes.titleweight"));
    const double titleY = rc.value("axes.titley").isNull() ? 1.0 : rc.number("axes.titley");
    auto titleOffset = std::make_shared<transforms::ScaledTranslation>(0.0, rc.number("axes.titlepad") / 72, fig.dpiScaleTrans);
    const double xs[3] = {0.5, 0.0, 1.0};
    const char* has[3] = {"center", "left", "right"};
    std::unique_ptr<Text>* titles[3] = {&title, &leftTitle, &rightTitle};
    for (int i = 0; i < 3; ++i) {
        auto t = std::make_unique<Text>(rc, xs[i], titleY, "");
        t->font.size = titleSize;
        t->font.weight = titleWeight;
        t->verticalAlignment = "baseline";
        t->horizontalAlignment = has[i];
        t->transform = transforms::compose(transAxes, titleOffset);
        t->figure = &fig;
        *titles[i] = std::move(t);
    }
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
    renderer.openGroup("axes", gid);
    // get_children without the patch: spines, axes, titles; sorted by zorder (stable)
    std::vector<Artist*> artists;
    if (axisOn && frameOn)
        for (auto& s : spines) artists.push_back(s.get());
    if (axisOn) {
        artists.push_back(xaxis.get());
        artists.push_back(yaxis.get());
    }
    for (Text* t : {title.get(), leftTitle.get(), rightTitle.get()}) {
        if (!t->text.empty()) renderer.unsupported("titles are not supported yet");
        artists.push_back(t);
    }
    std::stable_sort(artists.begin(), artists.end(), [](const Artist* a, const Artist* b) { return a->zorder < b->zorder; });
    if (axisOn && frameOn) artists.insert(artists.begin(), patch.get());
    for (Artist* a : artists) a->draw(renderer);
    renderer.closeGroup("axes");
}

}  // namespace calculate_plot::engine
