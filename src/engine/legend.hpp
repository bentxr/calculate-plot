// Derived from Matplotlib 3.11.2: lib/matplotlib/legend.py (Legend), lib/matplotlib/offsetbox.py
// (TextArea, DrawingArea, HPacker, VPacker and their helpers) and lib/matplotlib/legend_handler.py
// (HandlerLine2D, HandlerPathCollection, HandlerPolyCollection).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "support/json.hpp"
#include "engine/artist.hpp"
#include "engine/collections.hpp"
#include "engine/lines.hpp"
#include "engine/text.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;
class Figure;

// An OffsetBox: a box laid out relative to its offset, drawn there.
class OffsetBox {
public:
    virtual ~OffsetBox() = default;
    virtual transforms::Bbox bbox(Renderer& renderer) = 0;  // get_bbox
    virtual void setOffset(double x, double y) { offset_ = {x, y}; }
    virtual void draw(Renderer& renderer) = 0;
    bool visible = true;

protected:
    std::array<double, 2> offset_{0, 0};
};

class TextArea : public OffsetBox {
public:
    TextArea(const rc::Params& rc, const Figure& figure, const std::string& s, bool multilinebaseline);
    transforms::Bbox bbox(Renderer& renderer) override;
    void setOffset(double x, double y) override;
    void draw(Renderer& renderer) override;
    Text text;

private:
    std::shared_ptr<transforms::Affine2D> offsetTransform_, baselineTransform_;
    bool multilinebaseline_;
};

class DrawingArea : public OffsetBox {
public:
    DrawingArea(double width, double height, double xdescent, double ydescent);
    transforms::Bbox bbox(Renderer& renderer) override;
    void setOffset(double x, double y) override;
    void draw(Renderer& renderer) override;
    transforms::TransformPtr transform() const;  // dpi_transform + offset_transform
    void addArtist(std::unique_ptr<Artist> artist) { children_.push_back(std::move(artist)); }
    double width, height, xdescent, ydescent;

private:
    std::shared_ptr<transforms::Affine2D> offsetTransform_, dpiTransform_;
    std::vector<std::unique_ptr<Artist>> children_;
};

// HPacker and VPacker
class Packer : public OffsetBox {
public:
    Packer(bool vertical, double pad, double sep, std::string align, std::vector<OffsetBox*> children);
    transforms::Bbox bbox(Renderer& renderer) override;
    void draw(Renderer& renderer) override;
    // set_offset with a callable (the legend's box): (width, height, xdescent, ydescent) -> offset
    std::function<std::array<double, 2>(double, double, double, double, Renderer&)> offsetFunction;
    transforms::Bbox windowExtent(Renderer& renderer);

private:
    std::pair<transforms::Bbox, std::vector<std::array<double, 2>>> bboxAndChildOffsets(Renderer& renderer);
    std::array<double, 2> offset(const transforms::Bbox& bbox, Renderer& renderer);
    bool vertical_;
    double pad_, sep_;
    std::string align_;
    std::vector<OffsetBox*> children_;
};

struct LegendOptions {
    std::optional<std::string> loc;  // None: rcParams["legend.loc"]
    std::optional<std::string> title;
    std::optional<double> fontsize, titleFontsize;  // in points
    std::optional<bool> frameon, fancybox;
    std::optional<double> framealpha;
    std::optional<colors::Spec> facecolor, edgecolor;
    std::optional<support::Json> labelcolor;  // as given: a way to take it from the handle, a colour or a list of them
    int ncols = 1;  // the columns to split the entries into (no more than there are entries)
    bool markerfirst = true;
    std::optional<double> borderpad, labelspacing, handlelength, handletextpad, borderaxespad, columnspacing;
};

class Legend : public Artist {
public:
    // Legend(axes, handles, labels, **options); `handles` are the Axes' children that get entries,
    // or null where the entry is the errorbar container at the same place in `containers`.
    Legend(const rc::Params& rc, Axes& parent, const std::vector<Artist*>& handles,
           const std::vector<const ErrorbarContainer*>& containers, const std::vector<std::string>& labels,
           const LegendOptions& options, std::string* error);
    void draw(Renderer& renderer) override;
    transforms::Bbox tightbbox(Renderer& renderer);  // get_tightbbox: the legend box's window extent

private:
    std::array<double, 2> findOffset(double width, double height, double xdescent, double ydescent,
                                     Renderer& renderer);
    std::array<double, 2> anchoredBbox(int loc, const transforms::Bbox& bbox, Renderer& renderer);
    std::array<double, 2> findBestPosition(double width, double height, Renderer& renderer);

    Axes& parent_;
    double fontsize_;  // points
    double borderpad_, labelspacing_, handlelength_, handleheight_, handletextpad_, borderaxespad_, columnspacing_;
    int loc_ = 0;
    std::unique_ptr<FancyBboxPatch> patch_;
    std::vector<std::unique_ptr<OffsetBox>> boxes_;  // owns every box below
    TextArea* titleBox_ = nullptr;
    Packer* legendBox_ = nullptr;
};

}  // namespace calculate_plot::engine
