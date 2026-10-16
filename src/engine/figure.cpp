// Derived from Matplotlib 3.11.2: lib/matplotlib/figure.py (Figure), lib/matplotlib/backend_bases.py
// (FigureCanvasBase.print_figure, get_width_height), lib/matplotlib/_tight_bbox.py (adjust_bbox) and
// lib/matplotlib/backends/backend_agg.py (FigureCanvasAgg.draw).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/figure.hpp"

#include "engine/axes.hpp"
#include "engine/text.hpp"

#include <algorithm>
#include <cmath>
#include <optional>


namespace calculate_plot::engine {

Figure::Figure(const rc::Params& rc, const FigureOptions& o) : Artist(rc), subplotpars(SubplotParams::defaults(rc)) {
    std::array<double, 2> figsize;
    if (o.figsize) figsize = *o.figsize;
    else {
        const auto v = rc.numbers("figure.figsize");
        figsize = {v[0], v[1]};
    }
    dpi = o.dpi ? *o.dpi : rc.number("figure.dpi");
    const colors::Spec facecolor = o.facecolor ? *o.facecolor : colors::Spec(rc.text("figure.facecolor"));
    const colors::Spec edgecolor = o.edgecolor ? *o.edgecolor : colors::Spec(rc.text("figure.edgecolor"));
    // (pyplot.figure passes frameon=True unless told otherwise: rcParams["figure.frameon"] is for Figure() alone)
    const bool frameon = o.frameon ? *o.frameon : true;

    bboxInches = transforms::Bbox::fromBounds(0, 0, figsize[0], figsize[1]);
    dpiScaleTrans = std::make_shared<transforms::Affine2D>();
    dpiScaleTrans->scale(dpi);
    bbox = std::make_shared<transforms::TransformedBbox>(bboxInches, dpiScaleTrans);
    transFigure = std::make_shared<transforms::BboxTransformTo>(bbox);
    transSubfigure = transFigure;

    // Rectangle(xy=(0, 0), width=1, height=1, visible=frameon, facecolor=, edgecolor=, linewidth=)
    patch = std::make_unique<Rectangle>(rc, 0.0, 0.0, 1.0, 1.0);
    patch->setEdgecolor(edgecolor);
    patch->setFacecolor(facecolor);
    patch->setLinewidth(o.linewidth);
    patch->visible = frameon;
    patch->transform = transSubfigure;  // _set_artist_props
    patch->antialiased = false;
}

Figure::~Figure() = default;

std::vector<Axes*> Figure::subplots(int nrows, int ncols, const std::string& sharex, const std::string& sharey) {
    gridspecs.push_back(std::make_unique<GridSpec>(nrows, ncols));
    const GridSpec& gs = *gridspecs.back();
    std::vector<Axes*> out;
    // GridSpecBase.subplots: each Axes shares with the first one of the grid, of its row or of its column
    const auto sharedWith = [&](const std::string& share, int row, int col) -> Axes* {
        if (share == "none" || out.empty()) return nullptr;
        const int index = share == "all" ? 0 : share == "row" ? row * ncols : col;
        return std::size_t(index) < out.size() ? out[std::size_t(index)] : nullptr;
    };
    for (int row = 0; row < nrows; ++row)
        for (int col = 0; col < ncols; ++col) {
            Axes* const withX = sharedWith(sharex, row, col);
            Axes* const withY = sharedWith(sharey, row, col);
            axes.push_back(std::make_unique<Axes>(rc_, *this, gs.cell(row, col)));
            if (withX) axes.back()->share(true, *withX);
            if (withY) axes.back()->share(false, *withY);
            out.push_back(axes.back().get());
        }
    // turn off redundant tick labeling
    if (sharex == "col" || sharex == "all")
        for (Axes* ax : out) ax->labelOuter(true);
    if (sharey == "row" || sharey == "all")
        for (Axes* ax : out) ax->labelOuter(false);
    return out;
}

Axes* Figure::addSubplot(int nrows, int ncols, int num1, int num2) {
    const GridSpec* gs = nullptr;
    for (const auto& a : axes)
        if (a->subplotSpec && a->subplotSpec->gridspec->nrows == nrows && a->subplotSpec->gridspec->ncols == ncols) {
            gs = a->subplotSpec->gridspec;
            break;
        }
    if (!gs) {
        gridspecs.push_back(std::make_unique<GridSpec>(nrows, ncols));
        gs = gridspecs.back().get();
    }
    axes.push_back(std::make_unique<Axes>(rc_, *this, SubplotSpec{gs, num1, num2}));
    return axes.back().get();
}

Axes* Figure::addSubplot(const SubplotSpec& spec) {
    axes.push_back(std::make_unique<Axes>(rc_, *this, spec));
    return axes.back().get();
}

Axes* Figure::addAxes(double left, double bottom, double width, double height) {
    axes.push_back(std::make_unique<Axes>(rc_, *this, transforms::Bbox::fromBounds(left, bottom, width, height), std::nullopt));
    return axes.back().get();
}

void Figure::draw(Renderer& renderer) {
    if (!visible) return;
    // _get_draw_artists: the children but the patch, sorted by zorder (stable)
    std::vector<Artist*> artists;
    for (auto& a : axes) artists.push_back(a.get());
    for (auto& t : texts) artists.push_back(t.get());
    std::stable_sort(artists.begin(), artists.end(), [](const Artist* a, const Artist* b) { return a->zorder < b->zorder; });
    renderer.openGroup("figure", gid);
    patch->draw(renderer);
    for (Artist* a : artists) a->draw(renderer);
    renderer.closeGroup("figure");
}

void Figure::setDpi(double value) {
    if (value == dpi) return;
    dpi = value;
    *dpiScaleTrans = transforms::Affine2D();
    dpiScaleTrans->scale(dpi);
}

std::array<double, 2> Figure::canvasSize() const {
    const auto p = bbox->points();
    return {double(static_cast<long long>(std::max(p[0], p[2]) + 1e-8)), double(static_cast<long long>(std::max(p[1], p[3]) + 1e-8))};
}

transforms::Bbox Figure::tightbbox(Renderer& renderer) {
    std::vector<transforms::Bbox> bb;
    // The children that are not Axes, visible and in the layout: the texts (the patch is not in the
    // layout). Artist.get_tightbbox is their window extent: nothing clips them.
    for (auto& t : texts)
        if (t->visible) bb.push_back(static_cast<Text&>(*t).windowExtent(renderer));
    for (auto& ax : axes)
        if (ax->visible)
            if (auto b = ax->tightbbox(renderer, false)) bb.push_back(*b);
    std::vector<transforms::Bbox> sized;
    for (const auto& b : bb)
        if (std::isfinite(b.width()) && std::isfinite(b.height()) && (b.width() != 0 || b.height() != 0)) sized.push_back(b);
    if (sized.empty()) return transforms::Bbox(bboxInches->points());
    // TransformedBbox(Bbox.union(bb), dpi_scale_trans.inverted())
    const auto u = std::make_shared<transforms::Bbox>(transforms::unionOf(sized).points());
    return transforms::Bbox(transforms::TransformedBbox(u, dpiScaleTrans->inverted()).points());
}

namespace {

// What rcParams["savefig.*"] ask of a save (Figure.savefig and FigureCanvasBase.print_figure).
struct SaveOptions {
    double dpi = 0;
    std::optional<std::string> facecolor, edgecolor;  // unset: 'auto', the figure's own
    bool transparent = false, tight = false;
    double pad = 0;  // inches around the tight box
};

bool saveOptions(const Figure& figure, SaveOptions* o, std::string* error) {
    const rc::Params& rc = figure.rc();
    const auto& dpi = rc.value("savefig.dpi");
    o->dpi = dpi.isNumber() ? dpi.asNumber() : figure.dpi;  // or 'figure'
    if (!(o->dpi > 0)) return *error = "rc savefig.dpi must be positive", false;
    for (const bool face : {true, false}) {
        const std::string key = face ? "savefig.facecolor" : "savefig.edgecolor";
        const std::string color = rc.text(key);
        if (color != "auto") (face ? o->facecolor : o->edgecolor) = color;
    }
    o->transparent = rc.boolean("savefig.transparent");
    o->tight = rc.value("savefig.bbox").isString();  // 'tight', or None ('standard' is stored as None)
    o->pad = rc.number("savefig.pad_inches");
    return true;
}

// The figure as the save wants it (its dpi and its colours), put back when the save is done.
class Saving {
public:
    Saving(Figure& figure, const SaveOptions& o) : figure_(figure), dpi_(figure.dpi), patch_(figure.patch->colorState()) {
        figure.setDpi(o.dpi);
        std::optional<std::string> face = o.facecolor, edge = o.edgecolor;
        if (o.transparent) {
            // Figure.savefig(transparent=True): the Axes' patches lose their colours for the save, and
            // so does the figure's (facecolor and edgecolor default to 'none', before the rcParams)
            for (auto& ax : figure.axes) {
                axes_.push_back(ax->patch->colorState());
                ax->patch->setFacecolor(std::string("none"));  // (its edge has no colour as it is)
            }
            face = edge = std::string("none");
        }
        if (face) figure.patch->setFacecolor(face);
        if (edge) figure.patch->setEdgecolor(edge);
    }
    ~Saving() {
        figure_.patch->setColorState(patch_);
        for (std::size_t i = 0; i < axes_.size(); ++i) figure_.axes[i]->patch->setColorState(axes_[i]);
        figure_.setDpi(dpi_);
    }
    Saving(const Saving&) = delete;
    Saving& operator=(const Saving&) = delete;

private:
    Figure& figure_;
    double dpi_;
    Patch::Colors patch_;
    std::vector<Patch::Colors> axes_;
};

// _tight_bbox.adjust_bbox: until it is put back, the figure is the part `inches` of itself (its box
// in inches from the origin, the figure coordinates shifted, the patch covering the new box). The
// fixed dpi matplotlib passes is the figure's own at that moment (the save's for Agg, 72 for SVG),
// so its dpi_scale is 1.
class AdjustedBbox {
public:
    AdjustedBbox(Figure& figure, const std::array<double, 4>& inches)
        : figure_(figure), bboxInches_(figure.bboxInches), bbox_(figure.bbox), boxout_(figure.transFigure->boxout()) {
        // every Axes keeps its active position: ax.apply_aspect = lambda pos=None: None
        for (auto& ax : figure.axes) ax->aspectFrozen = true;
        auto tr = std::make_shared<transforms::Affine2D>();
        tr->scale(figure.dpi);
        figure.bboxInches = transforms::Bbox::fromBounds(0, 0, inches[2] - inches[0], inches[3] - inches[1]);
        const auto p0 = tr->transform(inches[0], inches[1]);
        const auto old = figure.bbox->points();
        const double w1 = old[2] - old[0], h1 = old[3] - old[1];
        figure.transFigure->setBoxout(transforms::Bbox::fromBounds(-p0[0], -p0[1], w1, h1));
        figure.bbox = std::make_shared<transforms::TransformedBbox>(figure.bboxInches, tr);
        Rectangle& patch = *figure.patch;
        patch.x0 = p0[0] / w1, patch.y0 = p0[1] / h1;
        patch.width = figure.bbox->width() / w1, patch.height = figure.bbox->height() / h1;
    }
    ~AdjustedBbox() {  // restore_bbox
        for (auto& ax : figure_.axes) ax->aspectFrozen = false;
        figure_.bbox = bbox_;
        figure_.bboxInches = bboxInches_;
        figure_.transFigure->setBoxout(boxout_);
        Rectangle& patch = *figure_.patch;
        patch.x0 = patch.y0 = 0, patch.width = patch.height = 1;
    }
    AdjustedBbox(const AdjustedBbox&) = delete;
    AdjustedBbox& operator=(const AdjustedBbox&) = delete;

private:
    Figure& figure_;
    std::shared_ptr<transforms::Bbox> bboxInches_;
    std::shared_ptr<transforms::TransformedBbox> bbox_;
    transforms::BboxPtr boxout_;
};

// The tight box of the figure as `renderer` measures it, grown by the pad (Bbox.padded), in inches.
std::array<double, 4> paddedTightBbox(Figure& figure, Renderer& renderer, double pad) {
    const auto p = figure.tightbbox(renderer).points();
    return {p[0] + -pad, p[1] + -pad, p[2] + pad, p[3] + pad};
}

}  // namespace

std::string drawAggTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         std::string* error) {
    SaveOptions o;
    if (!saveOptions(figure, &o, error)) return {};
    const Saving saving(figure, o);
    // FigureCanvasAgg.get_renderer: RendererAgg(w, h, dpi) of the canvas' whole pixels
    const auto renderer = [&] {
        const auto size = figure.canvasSize();
        return AggTraceRenderer(size[0], size[1], figure.dpi, figure.rc(), locale, fontDirectory);
    };
    std::optional<AdjustedBbox> adjusted;
    if (o.tight) {
        // The figure becomes its tight box, measured with the renderer of the save. (matplotlib
        // first draws the figure with drawing disabled to place the artists; get_tightbbox places
        // what it measures, so nothing here depends on that draw.)
        AggTraceRenderer layout = renderer();
        const auto inches = paddedTightBbox(figure, layout, o.pad);
        if (!layout.error().empty()) return *error = layout.error(), std::string();
        adjusted.emplace(figure, inches);
    }
    AggTraceRenderer final = renderer();
    final.clear();
    figure.draw(final);
    *error = final.error();
    return final.trace();
}

std::string drawScreenTrace(Figure& figure, double ratio, const std::string& locale, const std::string& fontDirectory,
                            std::string* error) {
    figure.setDpi(figure.dpi * ratio);  // _set_dpi(dpi * device_pixel_ratio)
    const auto size = figure.canvasSize();
    AggTraceRenderer renderer(size[0], size[1], figure.dpi, figure.rc(), locale, fontDirectory);
    renderer.clear();
    figure.draw(renderer);
    *error = renderer.error();
    return renderer.trace();
}

std::string drawSvgTrace(Figure& figure, const std::string& locale, const std::string& fontDirectory,
                         const SvgMetadata& metadata,
                         const std::string& hashsalt, std::string* error) {
    SaveOptions o;
    if (!saveOptions(figure, &o, error)) return {};
    const Saving saving(figure, o);
    // print_svg: the figure at 72 dpi, drawn on a canvas of its size in points; its images at the
    // dpi it had
    const auto renderer = [&] {
        const double dpi = figure.dpi;
        figure.setDpi(72);
        const auto inches = figure.bboxInches->points();
        return SvgTraceRenderer(inches[2] * 72, inches[3] * 72, dpi, figure.rc(), locale, fontDirectory, metadata, hashsalt);
    };
    std::optional<AdjustedBbox> adjusted;
    if (o.tight) {
        // print_figure gets its renderer from print_svg, which leaves the figure at 72 dpi: the
        // artists are measured there (FigureCanvasSVG.fixed_dpi is 72)
        SvgTraceRenderer layout = renderer();
        const auto inches = paddedTightBbox(figure, layout, o.pad);
        if (!layout.error().empty()) return *error = layout.error(), std::string();
        adjusted.emplace(figure, inches);
        figure.setDpi(o.dpi);  // print_figure sets the dpi again before printing
    }
    SvgTraceRenderer final = renderer();
    figure.draw(final);
    *error = final.error();
    return final.trace();
}

}  // namespace calculate_plot::engine
