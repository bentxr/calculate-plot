// Derived from Matplotlib 3.11.2: lib/matplotlib/legend.py (Legend), lib/matplotlib/offsetbox.py
// (TextArea, DrawingArea, HPacker, VPacker and their helpers) and lib/matplotlib/legend_handler.py
// (HandlerLine2D, HandlerPathCollection, HandlerPolyCollection, HandlerPatch, HandlerErrorbar).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/legend.hpp"

#include "agg/path_queries.h"
#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace calculate_plot::engine {

namespace {

using transforms::Bbox;

Bbox fromBounds(double x0, double y0, double w, double h) { return Bbox({x0, y0, x0 + w, y0 + h}); }
Bbox padded(const Bbox& b, double pad) {
    const auto p = b.points();
    return Bbox({p[0] - pad, p[1] - pad, p[2] + pad, p[3] + pad});
}
double pyMax(double a, double b) { return b > a ? b : a; }

// _get_aligned_offsets(yspans, height=None, align)
std::pair<std::array<double, 2>, std::vector<double>> alignedOffsets(const std::vector<std::array<double, 2>>& spans,
                                                                    const std::string& align) {
    double height = spans[0][1] - spans[0][0];
    for (const auto& s : spans) height = pyMax(height, s[1] - s[0]);
    std::vector<double> offsets;
    if (align == "baseline") {
        double lo = spans[0][0], hi = spans[0][1];
        for (const auto& s : spans) lo = s[0] < lo ? s[0] : lo, hi = s[1] > hi ? s[1] : hi;
        offsets.assign(spans.size(), 0.0);
        return {{lo, hi}, offsets};
    }
    for (const auto& s : spans) {
        if (align == "left" || align == "bottom") offsets.push_back(-s[0]);
        else if (align == "right" || align == "top") offsets.push_back(height - s[1]);
        else offsets.push_back((height - (s[1] - s[0])) * .5 - s[0]);  // center
    }
    return {{0.0, height}, offsets};
}

// _get_packed_offsets(widths, total=None, sep, mode='fixed'): np.cumsum([0] + [w + sep ...])
std::pair<double, std::vector<double>> packedOffsets(const std::vector<double>& widths, double sep) {
    std::vector<double> offsets;
    double acc = 0;
    for (double w : widths) {
        offsets.push_back(acc);
        acc += w + sep;
    }
    return {acc - sep, offsets};
}

}  // namespace

// --- TextArea, DrawingArea, packers --------------------------------------------------------------

TextArea::TextArea(const rc::Params& rc, const Figure& figure, const std::string& s, bool multilinebaseline)
    : text(rc, 0, 0, s),
      offsetTransform_(std::make_shared<transforms::Affine2D>()),
      baselineTransform_(std::make_shared<transforms::Affine2D>()),
      multilinebaseline_(multilinebaseline) {
    text.figure = &figure;
    text.transform = transforms::compose(offsetTransform_, baselineTransform_);
}

transforms::Bbox TextArea::bbox(Renderer& renderer) {
    const FontSpec spec{text::findFontsByProps(text.font, renderer.rc()), text.font.size, text.font};
    const TextMetrics lp = renderer.textWidthHeightDescent("lp", spec, false);
    const double hLp = lp.height, dLp = lp.descent;
    const Text::Layout layout = text.layout(renderer);
    double lastDescent = layout.lines.back().descent;
    const double w = layout.bbox.width();
    double h = layout.bbox.height();
    *baselineTransform_ = transforms::Affine2D();
    if (layout.lines.size() > 1 && multilinebaseline_) {
        const double ydNew = 0.5 * h - 0.5 * (hLp - dLp);
        baselineTransform_->translate(0, lastDescent - ydNew);
        lastDescent = ydNew;
    } else {  // single line
        const double hd = pyMax(hLp - dLp, h - lastDescent);
        h = hd + lastDescent;
    }
    const std::string& ha = text.horizontalAlignment;
    const double x0 = ha == "left" ? 0 : ha == "center" ? -w / 2 : -w;
    return fromBounds(x0, -lastDescent, w, h);
}

void TextArea::setOffset(double x, double y) {
    OffsetBox::setOffset(x, y);
    *offsetTransform_ = transforms::Affine2D();
    offsetTransform_->translate(x, y);
}

void TextArea::draw(Renderer& renderer) { text.draw(renderer); }

DrawingArea::DrawingArea(double w, double h, double xd, double yd)
    : width(w), height(h), xdescent(xd), ydescent(yd),
      offsetTransform_(std::make_shared<transforms::Affine2D>()),
      dpiTransform_(std::make_shared<transforms::Affine2D>()) {}

transforms::TransformPtr DrawingArea::transform() const { return transforms::compose(dpiTransform_, offsetTransform_); }

transforms::Bbox DrawingArea::bbox(Renderer& renderer) {
    const double dpiCor = renderer.pointsToPixels(1.);
    return fromBounds(-xdescent * dpiCor, -ydescent * dpiCor, width * dpiCor, height * dpiCor);
}

void DrawingArea::setOffset(double x, double y) {
    OffsetBox::setOffset(x, y);
    *offsetTransform_ = transforms::Affine2D();
    offsetTransform_->translate(x, y);
}

void DrawingArea::draw(Renderer& renderer) {
    const double dpiCor = renderer.pointsToPixels(1.);
    *dpiTransform_ = transforms::Affine2D();
    dpiTransform_->scale(dpiCor);
    for (auto& c : children_) c->draw(renderer);
}

Packer::Packer(bool vertical, double pad, double sep, std::string align, std::vector<OffsetBox*> children)
    : vertical_(vertical), pad_(pad), sep_(sep), align_(std::move(align)), children_(std::move(children)) {}

std::pair<transforms::Bbox, std::vector<std::array<double, 2>>> Packer::bboxAndChildOffsets(Renderer& renderer) {
    const double dpicor = renderer.pointsToPixels(1.);
    const double pad = pad_ * dpicor, sep = sep_ * dpicor;
    std::vector<Bbox> bboxes;
    for (OffsetBox* c : children_)
        if (c->visible) bboxes.push_back(c->bbox(renderer));
    std::vector<std::array<double, 2>> offsets;
    if (!vertical_) {  // HPacker
        if (bboxes.empty()) return {padded(fromBounds(0, 0, 0, 0), pad), {}};
        std::vector<std::array<double, 2>> spans;
        std::vector<double> widths;
        for (const auto& b : bboxes) {
            const auto p = b.points();
            spans.push_back({p[1], p[3]});
            widths.push_back(b.width());
        }
        const auto [yspan, yoffsets] = alignedOffsets(spans, align_);
        auto [width, xoffsets] = packedOffsets(widths, sep);
        const double x0 = bboxes[0].points()[0];
        for (std::size_t i = 0; i < xoffsets.size(); ++i) xoffsets[i] -= (bboxes[i].points()[0] - x0);
        for (std::size_t i = 0; i < xoffsets.size(); ++i) offsets.push_back({xoffsets[i], yoffsets[i]});
        return {padded(fromBounds(x0, yspan[0], width, yspan[1] - yspan[0]), pad), offsets};
    }
    // VPacker
    std::vector<std::array<double, 2>> spans;
    std::vector<double> heights;
    for (const auto& b : bboxes) {
        const auto p = b.points();
        spans.push_back({p[0], p[2]});
        heights.push_back(b.height());
    }
    const auto [xspan, xoffsets] = alignedOffsets(spans, align_);
    auto [height, yoffsets] = packedOffsets(heights, sep);
    for (std::size_t i = 0; i < yoffsets.size(); ++i) yoffsets[i] = height - (yoffsets[i] + bboxes[i].points()[3]);
    const double ydescent = yoffsets[0];
    for (double& y : yoffsets) y = y - ydescent;
    for (std::size_t i = 0; i < xoffsets.size(); ++i) offsets.push_back({xoffsets[i], yoffsets[i]});
    return {padded(fromBounds(xspan[0], -ydescent, xspan[1] - xspan[0], height), pad), offsets};
}

transforms::Bbox Packer::bbox(Renderer& renderer) { return bboxAndChildOffsets(renderer).first; }

std::array<double, 2> Packer::offset(const transforms::Bbox& b, Renderer& renderer) {
    if (offsetFunction) {
        const auto p = b.points();
        return offsetFunction(b.width(), b.height(), -p[0], -p[1], renderer);
    }
    return offset_;
}

transforms::Bbox Packer::windowExtent(Renderer& renderer) {
    const Bbox b = bbox(renderer);
    const auto o = offset(b, renderer);
    const auto p = b.points();
    return Bbox({p[0] + o[0], p[1] + o[1], p[2] + o[0], p[3] + o[1]});
}

void Packer::draw(Renderer& renderer) {
    const auto [b, offsets] = bboxAndChildOffsets(renderer);
    const auto o = offset(b, renderer);
    std::size_t i = 0;
    for (OffsetBox* c : children_) {
        if (!c->visible) continue;
        c->setOffset(o[0] + offsets[i][0], o[1] + offsets[i][1]);
        c->draw(renderer);
        ++i;
    }
}

// --- Legend --------------------------------------------------------------------------------------

namespace {

int locCode(const std::string& loc) {
    static const char* const codes[] = {"best",        "upper right", "upper left",   "lower left",
                                        "lower right", "right",       "center left", "center right",
                                        "lower center", "upper center", "center"};
    for (int i = 0; i < 11; ++i)
        if (loc == codes[i]) return i;
    return -1;
}

double rcOr(const rc::Params& rc, std::optional<double> v, const char* key) { return v ? *v : rc.number(key); }

// np.linspace(start, stop, num)
std::vector<double> linspace(double start, double stop, int num) {
    std::vector<double> out;
    const double div = num - 1;
    const double delta = stop - start;
    const double step = delta / div;
    for (int i = 0; i < num; ++i) out.push_back(i * step + start);
    out.back() = stop;
    return out;
}

}  // namespace

namespace {

// A colour of a figure file: a string, or a list or tuple of three or four numbers.
std::optional<colors::Rgba> oneColor(const support::Json& c, const rc::Params& rc) {
    if (c.isString()) return colors::toRgba(c.asString(), std::nullopt, rc, nullptr);
    const support::Json* list = c.isArray() ? &c : c.isObject() ? c.find("$tuple") : nullptr;
    if (!list) return std::nullopt;
    std::vector<double> rgb;
    for (const support::Json& x : list->items()) {
        if (x.isBool()) rgb.push_back(x.asBool() ? 1.0 : 0.0);
        else if (x.isNumber()) rgb.push_back(x.asNumber());
        else return std::nullopt;
    }
    return colors::toRgba(rgb, std::nullopt, nullptr);
}

// colors.to_rgba_array(c): one colour, or a list of them; 'none' (in any case) is no colour at all.
// nullopt where matplotlib raises.
std::optional<std::vector<colors::Rgba>> colorArray(const support::Json& c, const rc::Params& rc) {
    if (c.isString() && colors::isNone(colors::Spec(c.asString()))) return std::vector<colors::Rgba>{};
    if (const auto one = oneColor(c, rc)) return std::vector<colors::Rgba>{*one};
    const support::Json* list = c.isArray() ? &c : c.isObject() ? c.find("$tuple") : nullptr;
    if (!list) return std::nullopt;
    std::vector<colors::Rgba> rows;
    for (const support::Json& item : list->items()) {
        const auto one = oneColor(item, rc);
        if (!one) return std::nullopt;
        rows.push_back(*one);
    }
    return rows;
}

// What a legend handle answers to get_markerfacecolor, get_facecolor, get_markeredgecolor,
// get_edgecolor and get_color: nothing where it has no such method, a colour, or the rows of a
// collection's colours.
struct HandleColors {
    // (a line made for a LineCollection has the first row of its colours as its colour: an array of
    // four numbers, which matplotlib then reads as four rows of one number)
    struct Row {
        colors::Rgba rgba;
    };
    using Value = std::variant<colors::Spec, std::vector<colors::Rgba>, Row>;
    bool mapped = false;  // get_array() is not None
    std::optional<Value> markerfacecolor, facecolor, markeredgecolor, edgecolor, color;
};

HandleColors colorsOf(const Line2D& line) {
    HandleColors c;
    // (_str_lower_equal(fc, 'auto') for the face, _str_equal(mec, 'auto') for the edge)
    std::string face = std::holds_alternative<std::string>(line.markerfacecolor) ? std::get<std::string>(line.markerfacecolor) : std::string();
    for (char& ch : face) ch = ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch;
    const bool autoFace = face == "auto";
    c.markerfacecolor = line.markerStyle.fillstyle() == "none" ? colors::Spec(std::string("none")) : autoFace ? line.color : line.markerfacecolor;
    const bool autoEdge = std::holds_alternative<std::string>(line.markeredgecolor) && std::get<std::string>(line.markeredgecolor) == "auto";
    c.markeredgecolor = autoEdge ? line.color : line.markeredgecolor;
    c.color = line.color;
    return c;
}

HandleColors colorsOf(const Collection& collection) {
    HandleColors c;
    c.mapped = collection.array.has_value();
    c.facecolor = collection.facecolors();
    c.edgecolor = collection.edgecolors();
    return c;
}

HandleColors colorsOf(const Patch& patch) {
    HandleColors c;
    const auto state = patch.colorState();
    c.facecolor = colors::Spec(std::vector<double>(state.face.begin(), state.face.end()));
    c.edgecolor = colors::Spec(std::vector<double>(state.edge.begin(), state.edge.end()));
    return c;
}

// The text colour a handle gives for a way of asking ('linecolor', 'markerfacecolor', 'markeredgecolor'):
// the first of its colours that is one and is not transparent.
std::optional<colors::Rgba> labelColorFrom(const HandleColors& handle, const std::string& way, const rc::Params& rc,
                                           std::string* error) {
    const auto close = [](double a, double b) { return std::abs(a - b) <= 1e-8 + 1e-5 * std::abs(b); };  // np.isclose
    if (handle.mapped) return std::nullopt;
    using Getter = const std::optional<HandleColors::Value>*;
    const std::array<Getter, 5> getters =
        way == "linecolor" ? std::array<Getter, 5>{&handle.markerfacecolor, &handle.facecolor, &handle.markeredgecolor, &handle.edgecolor, &handle.color}
        : way == "markerfacecolor" ? std::array<Getter, 5>{&handle.markerfacecolor, &handle.facecolor, nullptr, nullptr, nullptr}
                                   : std::array<Getter, 5>{&handle.markeredgecolor, &handle.edgecolor, nullptr, nullptr, nullptr};
    for (const Getter getter : getters) {
        if (!getter || !*getter) continue;  // (no such way to ask this handle: AttributeError)
        if (const auto* rows = std::get_if<std::vector<colors::Rgba>>(&**getter)) {
            if (rows->empty()) continue;
            // one row, or all of them close to the first (np.isclose)
            bool same = true;
            for (const colors::Rgba& row : *rows)
                for (std::size_t k = 0; k < 4; ++k) same = same && close(row[k], (*rows)[0][k]);
            return same ? std::optional<colors::Rgba>((*rows)[0]) : std::nullopt;
        }
        if (const auto* row = std::get_if<HandleColors::Row>(&**getter)) {
            // four "rows": no colour unless the four numbers are the same, and then the first number
            // is given to Text.set_color, which refuses a number
            bool same = true;
            for (double v : row->rgba) same = same && close(v, row->rgba[0]);
            if (same) *error = "labelcolor: the colour of the lines of this entry cannot be read (matplotlib breaks on it)";
            return std::nullopt;
        }
        // ('none' and whatever else is fully transparent is passed over)
        const colors::Rgba rgba = *colors::toRgba(std::get<colors::Spec>(**getter), std::nullopt, rc, nullptr);
        if (rgba[3] == 0) continue;
        return rgba;
    }
    return std::nullopt;
}

}  // namespace

Legend::Legend(const rc::Params& rc, Axes& parent, const std::vector<Artist*>& handles,
               const std::vector<const ErrorbarContainer*>& containers, const std::vector<std::string>& labels,
               const LegendOptions& o, std::string* error)
    : Artist(rc), parent_(parent) {
    zorder = 5;
    // FontProperties(size=fontsize or rcParams["legend.fontsize"]).get_size_in_points()
    fontsize_ = o.fontsize ? text::atLeastOnePoint(*o.fontsize) : text::fontSizeOf(rc.value("legend.fontsize"), rc);
    borderpad_ = rcOr(rc, o.borderpad, "legend.borderpad");
    labelspacing_ = rcOr(rc, o.labelspacing, "legend.labelspacing");
    handlelength_ = rcOr(rc, o.handlelength, "legend.handlelength");
    handleheight_ = rc.number("legend.handleheight");
    handletextpad_ = rcOr(rc, o.handletextpad, "legend.handletextpad");
    borderaxespad_ = rcOr(rc, o.borderaxespad, "legend.borderaxespad");
    columnspacing_ = rcOr(rc, o.columnspacing, "legend.columnspacing");
    shadow_ = o.shadow ? *o.shadow : rc.boolean("legend.shadow");
    const int numpoints = o.numpoints ? *o.numpoints : int(rc.number("legend.numpoints"));
    const int scatterpoints = o.scatterpoints ? *o.scatterpoints : int(rc.number("legend.scatterpoints"));
    const double markerscale = o.markerscale ? *o.markerscale : rc.number("legend.markerscale");
    if (numpoints <= 0) *error = "numpoints must be > 0; it was " + std::to_string(numpoints);
    if (scatterpoints <= 0) *error = "scatterpoints below 1 are not supported yet";
    // scatteryoffsets: [3/8, 4/8, 2.5/8] over and over, one for each scatter point
    std::vector<double> scatterYOffsets;
    for (int k = 0; k < scatterpoints; ++k) scatterYOffsets.push_back(std::array<double, 3>{3. / 8., 4. / 8., 2.5 / 8.}[std::size_t(k % 3)]);
    // HandlerNpoints.get_xdata: where the line of an entry goes and where its markers; one marker is
    // in the middle of a line from end to end, several are spread with a pad at both ends
    const auto xdataOf = [&](int points, double xdescent, double width, std::vector<double>* markers) {
        if (points > 1) {
            const double pad = 0.3 * fontsize_;
            *markers = linspace(-xdescent + pad, -xdescent + width - pad, points);
            return *markers;
        }
        *markers = {-xdescent + 0.5 * width};
        return std::vector<double>{-xdescent, -xdescent + width};
    };

    // the frame
    colors::Spec face = o.facecolor ? *o.facecolor : colors::Spec(rc.text("legend.facecolor"));
    if (const auto* s = std::get_if<std::string>(&face); s && *s == "inherit") face = rc.text("axes.facecolor");
    colors::Spec edge = o.edgecolor ? *o.edgecolor : colors::Spec(rc.text("legend.edgecolor"));
    if (const auto* s = std::get_if<std::string>(&edge); s && *s == "inherit") edge = rc.text("axes.edgecolor");
    const bool fancybox = o.fancybox ? *o.fancybox : rc.boolean("legend.fancybox");
    patch_ = std::make_unique<FancyBboxPatch>(rc, 0, 0, 1, 1, fancybox ? "round" : "square", 0.0,
                                              fancybox ? std::optional<double>(0.2) : std::nullopt, fontsize_);
    patch_->setEdgecolor(edge);
    patch_->setFacecolor(face);
    if (!rc.value("legend.linewidth").isNull()) patch_->setLinewidth(rc.number("legend.linewidth"));
    // alpha=framealpha, else 1 with a shadow asked for by the keyword, else rcParams["legend.framealpha"]:
    // None there leaves the colours their own alpha
    std::optional<double> framealpha = o.framealpha;
    if (!framealpha && o.shadow && *o.shadow) framealpha = 1.0;
    if (!framealpha && !rc.value("legend.framealpha").isNull()) framealpha = rc.number("legend.framealpha");
    if (framealpha && !(0 <= *framealpha && *framealpha <= 1)) {  // Artist.set_alpha
        *error = "framealpha is outside the 0-1 range";
        return;
    }
    patch_->setAlpha(framealpha);
    patch_->snap = true;
    patch_->visible = o.frameon ? *o.frameon : rc.boolean("legend.frameon");
    patch_->transform = std::make_shared<transforms::IdentityTransform>();  // the legend's transform

    // _init_legend_box
    const double fontsize = fontsize_;
    const double descent = 0.35 * fontsize * (handleheight_ - 0.7);
    const double height = fontsize * handleheight_ - descent;
    text::FontProperties prop = text::FontProperties::defaults(rc);
    prop.size = fontsize_;
    std::vector<std::pair<OffsetBox*, OffsetBox*>> items;
    std::vector<HandleColors> handleColors;  // of the first artist of each entry (the legend handle)
    std::vector<Text*> texts;
    for (std::size_t i = 0; i < handles.size(); ++i) {
        auto textbox = std::make_unique<TextArea>(rc, parent.figure, labels[i], true);
        textbox->text.verticalAlignment = "baseline";
        textbox->text.horizontalAlignment = "left";
        textbox->text.font = prop;
        textbox->text.setColor(rc.text("text.color"));  // labelcolor: rcParams["text.color"]
        auto handlebox = std::make_unique<DrawingArea>(handlelength_ * fontsize, height, 0., descent);
        const double xd = handlebox->xdescent, yd = handlebox->ydescent, w = handlebox->width, h = handlebox->height;
        const transforms::TransformPtr trans = handlebox->transform();
        if (const ErrorbarContainer* barContainer = containers[i]; barContainer && barContainer->isBars) {
            // HandlerPatch(update_func=update_from_first_child): a Rectangle like the first bar
            auto rect = std::make_unique<Rectangle>(rc, -xd, -yd, w, h);
            if (!barContainer->bars.empty()) rect->updateFrom(*barContainer->bars[0]);
            rect->transform = trans;
            handleColors.push_back(colorsOf(*rect));
            handlebox->addArtist(std::move(rect));
        } else if (const ErrorbarContainer* bars = containers[i]) {  // HandlerErrorbar
            std::vector<double> xmarker;
            const std::vector<double> xdata = xdataOf(numpoints, xd, w, &xmarker), ydata(xdata.size(), (h - yd) / 2);
            const std::vector<double> ymarker(xmarker.size(), ydata[0]);
            const double errSize = 0.5 * fontsize;  // xerr_size, and yerr_size with it
            std::string ignored;
            auto legline = std::make_unique<Line2D>(rc, xdata, ydata);
            auto marker = std::make_unique<Line2D>(rc, xmarker, ymarker);
            if (!bars->dataLine) {
                legline->visible = false;
                marker->visible = false;
            } else {
                for (Line2D* l : {legline.get(), marker.get()}) {
                    l->updateFrom(*bars->dataLine);
                    l->clipBox.reset();
                    l->clipPath.reset();
                }
                legline->setDrawstyle("default");
                legline->setMarker("none", &ignored);
                marker->setLinestyle("None", &ignored);
                if (markerscale != 1) marker->markersize = marker->markersize * markerscale;
            }
            std::vector<std::unique_ptr<Artist>> made;
            const auto side = [&](bool horizontal) {
                // a bar at each marker like those of the first collection, and two lines of caps like the first
                const double dx = horizontal ? errSize : 0, dy = horizontal ? 0 : errSize;
                std::vector<std::vector<std::array<double, 2>>> verts;
                for (std::size_t k = 0; k < xmarker.size(); ++k)
                    verts.push_back({{xmarker[k] - dx, ymarker[k] - dy}, {xmarker[k] + dx, ymarker[k] + dy}});
                auto bar = std::make_unique<LineCollection>(rc, verts);
                bar->updateFrom(*bars->barlinecols[0]);
                bar->figure = &parent.figure;  // (_set_artist_props; a bar this short never asks for it)
                bar->clipBox.reset();
                bar->clipPath.reset();
                bar->transform = trans;
                if (handleColors.size() == i) handleColors.push_back(colorsOf(*bar));
                handlebox->addArtist(std::move(bar));
                if (bars->caplines.empty()) return;
                for (const double sign : {-1.0, 1.0}) {
                    std::vector<double> capx, capy;
                    for (std::size_t k = 0; k < xmarker.size(); ++k) capx.push_back(xmarker[k] + sign * dx), capy.push_back(ymarker[k] + sign * dy);
                    auto cap = std::make_unique<Line2D>(rc, capx, capy);
                    cap->updateFrom(*bars->caplines[0]);
                    cap->clipBox.reset();
                    cap->clipPath.reset();
                    cap->setMarker(horizontal ? "|" : "_", &ignored);
                    cap->transform = trans;
                    made.push_back(std::move(cap));
                }
            };
            if (bars->hasXerr) side(true);
            if (bars->hasYerr) side(false);
            // the bars first, then the caps, the line and its marker
            for (auto& cap : made) handlebox->addArtist(std::move(cap));
            legline->transform = trans;
            marker->transform = trans;
            handlebox->addArtist(std::move(legline));
            handlebox->addArtist(std::move(marker));
        } else if (auto* line = dynamic_cast<Line2D*>(handles[i])) {  // HandlerLine2D
            // one point: three along the line, the marker on the middle one; several: a marker on each
            std::vector<double> xmarker;
            auto xdata = xdataOf(numpoints, xd, w, &xmarker);
            if (numpoints == 1) xdata = linspace(xdata.front(), xdata.back(), 3);
            std::vector<double> ydata(xdata.size(), (h - yd) / 2);
            auto legline = std::make_unique<Line2D>(rc, xdata, ydata);
            if (numpoints == 1) legline->markevery = std::vector<long long>{1};
            legline->updateFrom(*line);
            if (markerscale != 1) legline->markersize = legline->markersize * markerscale;
            legline->clipBox.reset();
            legline->clipPath.reset();
            legline->transform = trans;
            handleColors.push_back(colorsOf(*legline));
            handlebox->addArtist(std::move(legline));
        } else if (auto* lines = dynamic_cast<LineCollection*>(handles[i])) {  // HandlerLineCollection
            // (its points are the scatter points: the line is shorter with several)
            std::vector<double> xmarker;
            const auto xdata = xdataOf(scatterpoints, xd, w, &xmarker);
            auto legline = std::make_unique<Line2D>(rc, xdata, std::vector<double>(xdata.size(), (h - yd) / 2));
            // _default_update_prop: the colour, the dashes and the width of the first line
            const auto edges = lines->edgecolors();
            if (edges.empty()) {
                *error = "a legend entry for lines without a colour cannot be made";  // (matplotlib: IndexError)
            } else {
                legline->color = colors::Spec(std::vector<double>(edges[0].begin(), edges[0].end()));
                legline->setDashPattern(lines->unscaledLinestyles()[0].first, lines->unscaledLinestyles()[0].second);
                legline->setLinewidth(lines->linewidths()[0]);
            }
            legline->transform = trans;
            HandleColors own;
            if (!edges.empty()) own.markerfacecolor = own.markeredgecolor = own.color = HandleColors::Row{edges[0]};
            handleColors.push_back(own);
            handlebox->addArtist(std::move(legline));
        } else if (auto* pc = dynamic_cast<PathCollection*>(handles[i])) {  // HandlerPathCollection
            const auto sizesOf = pc->sizes();
            double smax = sizesOf.empty() ? 1 : sizesOf[0], smin = smax;
            for (double s : sizesOf) smax = pyMax(smax, s), smin = s < smin ? s : smin;
            smax = smax * std::pow(markerscale, 2), smin = smin * std::pow(markerscale, 2);
            // get_sizes: the mean, the largest and the smallest for up to three points; evenly from the
            // smallest to the largest for more
            std::vector<double> sizes{.5 * (smax + smin), smax, smin};
            sizes.resize(std::size_t(std::min(scatterpoints, 3)));
            if (scatterpoints >= 4) {
                sizes.clear();
                for (double t : linspace(0, 1, scatterpoints)) sizes.push_back((smax - smin) * t + smin);
            }
            std::vector<double> xmarker;
            xdataOf(scatterpoints, xd, w, &xmarker);
            auto handle = std::make_unique<PathCollection>(rc, std::vector<Path>{pc->paths[0]}, sizes);
            for (std::size_t k = 0; k < xmarker.size(); ++k) handle->offsets.push_back({xmarker[k], h * scatterYOffsets[k]});
            handle->updateFrom(*pc);
            handle->figure = &parent.figure;
            handle->clipBox.reset();
            handle->clipPath.reset();
            handle->offsetTransform = trans;
            handleColors.push_back(colorsOf(*handle));
            handlebox->addArtist(std::move(handle));
        } else if (auto* poly = dynamic_cast<PolyCollection*>(handles[i])) {  // HandlerPolyCollection
            auto rect = std::make_unique<Rectangle>(rc, -xd, -yd, w, h);
            const auto first = [](const std::vector<colors::Rgba>& c) { return c.empty() ? colors::Rgba{0, 0, 0, 0} : c[0]; };
            rect->setResolvedColors(first(poly->facecolors()), first(poly->edgecolors()));
            rect->setResolvedHatch(poly->hatch, first(poly->hatchcolors()));
            rect->setLinewidth(poly->linewidths().empty() ? std::optional<double>() : poly->linewidths()[0]);
            rect->transform = trans;
            handleColors.push_back(colorsOf(*rect));
            handlebox->addArtist(std::move(rect));
        } else if (auto* patch = dynamic_cast<Patch*>(handles[i])) {  // HandlerPatch (a Rectangle like the patch)
            auto rect = std::make_unique<Rectangle>(rc, -xd, -yd, w, h);
            rect->updateFrom(*patch);
            rect->transform = trans;
            handleColors.push_back(colorsOf(*rect));
            handlebox->addArtist(std::move(rect));
        } else {
            *error = "legend entries for this kind of artist are not supported yet";
        }
        if (handleColors.size() == i) handleColors.emplace_back();
        texts.push_back(&textbox->text);
        items.push_back({handlebox.get(), textbox.get()});
        boxes_.push_back(std::move(handlebox));
        boxes_.push_back(std::move(textbox));
    }
    // The text colour: labelcolor, else rcParams["legend.labelcolor"], else rcParams["text.color"]
    const support::Json& labelcolor = o.labelcolor ? *o.labelcolor
                                      : !rc.value("legend.labelcolor").isNull() ? rc.value("legend.labelcolor") : rc.value("text.color");
    const std::string way = labelcolor.isString() ? labelcolor.asString() : std::string();
    if (way == "linecolor" || way == "markerfacecolor" || way == "mfc" || way == "markeredgecolor" || way == "mec") {
        const std::string getter = way == "mfc" ? "markerfacecolor" : way == "mec" ? "markeredgecolor" : way;
        for (std::size_t i = 0; i < texts.size(); ++i)
            if (const auto color = labelColorFrom(handleColors[i], getter, rc, error)) texts[i]->color = *color;
    } else if (way == "none") {
        for (Text* text : texts) text->color = colors::Rgba{0.0, 0.0, 0.0, 0.0};
    } else if (const auto rows = colorArray(labelcolor, rc)) {
        // zip(self.texts, itertools.cycle(colors)): with no colours, no text takes one
        for (std::size_t i = 0; i < texts.size() && !rows->empty(); ++i) texts[i]->color = (*rows)[i % rows->size()];
    } else {
        *error = "Invalid labelcolor";
    }
    // np.array_split(handles_and_labels, ncols), empty columns dropped
    const int ncols = o.ncols;
    std::vector<OffsetBox*> columns;
    const std::size_t n = items.size();
    std::size_t at = 0;
    for (int c = 0; c < ncols; ++c) {
        const std::size_t count = n / std::size_t(ncols) + (std::size_t(c) < n % std::size_t(ncols) ? 1 : 0);
        if (!count) continue;
        std::vector<OffsetBox*> itemboxes;
        for (std::size_t k = at; k < at + count; ++k) {
            std::vector<OffsetBox*> pair = o.markerfirst ? std::vector<OffsetBox*>{items[k].first, items[k].second}
                                                         : std::vector<OffsetBox*>{items[k].second, items[k].first};
            boxes_.push_back(std::make_unique<Packer>(false, 0, handletextpad_ * fontsize, "baseline", pair));
            itemboxes.push_back(boxes_.back().get());
        }
        at += count;
        boxes_.push_back(std::make_unique<Packer>(true, 0, labelspacing_ * fontsize, o.markerfirst ? "baseline" : "right",
                                                  itemboxes));
        columns.push_back(boxes_.back().get());
    }
    boxes_.push_back(std::make_unique<Packer>(false, 0, columnspacing_ * fontsize, "baseline", columns));
    OffsetBox* handleBox = boxes_.back().get();
    auto title = std::make_unique<TextArea>(rc, parent.figure, "", false);
    titleBox_ = title.get();
    boxes_.push_back(std::move(title));
    auto legendBox = std::make_unique<Packer>(true, borderpad_ * fontsize, labelspacing_ * fontsize, "center",
                                              std::vector<OffsetBox*>{titleBox_, handleBox});
    legendBox_ = legendBox.get();
    boxes_.push_back(std::move(legendBox));

    // set_loc
    if (o.locCode) {
        loc_ = *o.locCode;
    } else if (o.locXY) {
        locXY_ = o.locXY;
    } else {
        const std::string loc = o.loc ? *o.loc : rc.text("legend.loc");
        loc_ = locCode(loc);
        if (loc_ < 0) *error = "unknown legend location '" + loc + "'";
    }
    legendBox_->offsetFunction = [this](double w, double h, double xd, double yd, Renderer& r) {
        return findOffset(w, h, xd, yd, r);
    };
    // set_title (font size: title_fontsize, rcParams["legend.title_fontsize"], else the default size)
    titleBox_->text.text = o.title ? *o.title : "";
    titleBox_->visible = titleBox_->text.visible = !titleBox_->text.text.empty();
    text::FontProperties titleProp = text::FontProperties::defaults(rc);
    titleProp.size = o.titleFontsize ? text::atLeastOnePoint(*o.titleFontsize) : text::fontSizeOf(rc.value("legend.title_fontsize"), rc);
    titleBox_->text.font = titleProp;
}

std::array<double, 2> Legend::anchoredBbox(int loc, const transforms::Bbox& b, Renderer& renderer) {
    // offsetbox._get_anchored_bbox with pad = borderaxespad * points_to_pixels(fontsize)
    const double pad = borderaxespad_ * renderer.pointsToPixels(fontsize_);
    static const double coefs[11][2] = {{0, 0},   {1.0, 1.0}, {0, 1.0},   {0, 0},   {1.0, 0}, {1.0, 0.5},
                                        {0, 0.5}, {1.0, 0.5}, {0.5, 0},   {0.5, 1.0}, {0.5, 0.5}};
    // "NE", "NW", "SW", "SE", "E", "W", "E", "S", "N", "C"
    const auto parent = parent_.bbox->points();
    const Bbox container({parent[0] + pad, parent[1] + pad, parent[2] - pad, parent[3] - pad});  // padded(-pad)
    const auto [l, bb, w, h] = container.bounds();
    const auto [L, B, W, H] = b.bounds();
    const auto p = b.points();
    return {p[0] + ((l + coefs[loc][0] * (w - W)) - L), p[1] + ((bb + coefs[loc][1] * (h - H)) - B)};
}

std::array<double, 2> Legend::findBestPosition(double width, double height, Renderer& renderer) {
    // _auto_legend_data
    std::vector<std::pair<std::vector<double>, std::vector<unsigned char>>> lines;
    std::vector<std::array<double, 2>> offsets;
    std::vector<std::array<double, 4>> bboxes;
    for (const auto& child : parent_.children) {
        if (auto* line = dynamic_cast<Line2D*>(child.get())) {
            const Path p = line->path();
            std::vector<double> v = p.vertices ? *p.vertices : std::vector<double>{};
            for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
                const auto q = line->transform->transform(v[i], v[i + 1]);
                v[i] = q[0], v[i + 1] = q[1];
            }
            lines.push_back({v, p.codes ? *p.codes : std::vector<unsigned char>{}});
        } else if (auto* poly = dynamic_cast<PolyCollection*>(child.get())) {
            for (const Path& p : poly->paths) {
                std::vector<double> v = p.vertices ? *p.vertices : std::vector<double>{};
                for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
                    const auto q = poly->transform->transform(v[i], v[i + 1]);
                    v[i] = q[0], v[i + 1] = q[1];
                }
                lines.push_back({v, p.codes ? *p.codes : std::vector<unsigned char>{}});
            }
        } else if (auto* c = dynamic_cast<Collection*>(child.get())) {
            for (const auto& o : c->offsets) offsets.push_back(c->offsetTransform->transform(o[0], o[1]));
        } else if (auto* rectangle = dynamic_cast<Rectangle*>(child.get())) {
            // artist.get_bbox().transformed(artist.get_data_transform()): Bbox.transformed takes three
            // corners through the transform
            const double x0 = rectangle->x0, y0 = rectangle->y0, x1 = x0 + rectangle->width, y1 = y0 + rectangle->height;
            const auto ll = rectangle->transform->transform(x0, y0);
            const auto ul = rectangle->transform->transform(x0, y1);
            const auto lr = rectangle->transform->transform(x1, y0);
            bboxes.push_back({ll[0], ll[1], lr[0], ul[1]});
        } else if (auto* other = dynamic_cast<Patch*>(child.get())) {
            // another Patch: its path through get_transform(), as a line's
            const Path p = other->path();
            const auto trans = other->fullTransform();
            std::vector<double> v = p.vertices ? *p.vertices : std::vector<double>{};
            for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
                const auto q = trans->transform(v[i], v[i + 1]);
                v[i] = q[0], v[i + 1] = q[1];
            }
            lines.push_back({v, p.codes ? *p.codes : std::vector<unsigned char>{}});
        } else if (auto* text = dynamic_cast<Text*>(child.get())) {
            bboxes.push_back(text->windowExtent(renderer).points());
        } else {
            renderer.unsupported("'best' legend placement around this kind of artist is not supported yet");
        }
    }
    const auto countContains = [](const std::array<double, 4>& box, const std::vector<double>& v) {
        // BboxBase.count_contains: strictly inside (NaN never is)
        const double x0 = std::min(box[0], box[2]), x1 = std::max(box[0], box[2]);
        const double y0 = std::min(box[1], box[3]), y1 = std::max(box[1], box[3]);
        int n = 0;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) n += (x0 < v[i] && v[i] < x1 && y0 < v[i + 1] && v[i + 1] < y1);
        return n;
    };
    std::tuple<int, int, double, double> best{0, 0, 0, 0};
    bool first = true;
    for (int idx = 1; idx < 11; ++idx) {
        const auto lb = anchoredBbox(idx, fromBounds(0, 0, width, height), renderer);
        const std::array<double, 4> box{lb[0], lb[1], lb[0] + width, lb[1] + height};
        int badness = 0;
        for (const auto& l : lines) badness += countContains(box, l.first);
        std::vector<double> flat;
        for (const auto& o : offsets) flat.push_back(o[0]), flat.push_back(o[1]);
        badness += countContains(box, flat);
        badness += mpl::count_bboxes_overlapping_bbox(box, bboxes);
        for (const auto& l : lines) {
            auto v = std::make_shared<std::vector<double>>(l.first);
            auto codes = l.second.empty() ? nullptr : std::make_shared<std::vector<uint8_t>>(l.second);
            mpl::PathIterator path(v, codes, false, 0.111111111111);
            badness += mpl::path_intersects_rectangle(path, box[0], box[1], box[2], box[3]) ? 1 : 0;
        }
        const std::tuple<int, int, double, double> candidate{badness, idx, lb[0], lb[1]};
        if (first || candidate < best) best = candidate, first = false;
        if (badness == 0) break;
    }
    return {std::get<2>(best), std::get<3>(best)};
}

std::array<double, 2> Legend::findOffset(double width, double height, double xdescent, double ydescent,
                                         Renderer& renderer) {
    std::array<double, 2> xy;
    if (locXY_) {  // axes coordinates: the corner at those fractions of the anchor box (the Axes' box)
        const auto [x0, y0, w, h] = parent_.bbox->bounds();
        xy = {x0 + w * (*locXY_)[0], y0 + h * (*locXY_)[1]};
    } else if (loc_ == 0) {
        xy = findBestPosition(width, height, renderer);
    } else {
        xy = anchoredBbox(loc_, fromBounds(0, 0, width, height), renderer);
    }
    return {xy[0] + xdescent, xy[1] + ydescent};
}

void Legend::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup("legend", gid);
    const double fontsize = renderer.pointsToPixels(fontsize_);
    const auto b = legendBox_->windowExtent(renderer).bounds();
    patch_->setBounds(b[0], b[1], b[2], b[3]);
    patch_->mutationScale = fontsize;
    if (shadow_) Shadow(patch_->rc(), *patch_, 2, -2).draw(renderer);
    patch_->draw(renderer);
    legendBox_->draw(renderer);
    renderer.closeGroup("legend");
}

transforms::Bbox Legend::tightbbox(Renderer& renderer) { return legendBox_->windowExtent(renderer); }

}  // namespace calculate_plot::engine
