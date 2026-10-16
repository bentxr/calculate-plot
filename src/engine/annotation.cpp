// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (_AnnotationBase, Annotation).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/annotation.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"

#include <algorithm>
#include <cmath>

namespace calculate_plot::engine {

namespace {

// coords.split(): the two words of a coordinate name, or false if it has not two
bool split(const std::string& name, std::string* bbox, std::string* unit) {
    std::vector<std::string> words;
    std::size_t i = 0;
    while (i < name.size()) {
        while (i < name.size() && std::isspace(static_cast<unsigned char>(name[i]))) ++i;
        std::size_t j = i;
        while (j < name.size() && !std::isspace(static_cast<unsigned char>(name[j]))) ++j;
        if (j > i) words.push_back(name.substr(i, j - i));
        i = j;
    }
    if (words.size() != 2) return false;
    *bbox = words[0], *unit = words[1];
    return true;
}

}  // namespace

std::string annotationCoordsError(const std::string& name, bool offset) {
    if (name == "data") return "";
    if (name == "polar") return "polar coordinates are not supported yet";
    std::string bbox, unit;
    if (!split(name, &bbox, &unit) || (bbox != "figure" && bbox != "subfigure" && bbox != "axes" && bbox != "offset"))
        return "'" + name + "' is not a valid coordinate";
    if (bbox == "offset" && !offset) return "xycoords cannot be an offset coordinate";
    if (unit != "points" && unit != "pixels" && unit != "fontsize" && unit != "fraction")
        return "'" + unit + "' is not a recognized unit";
    if (bbox == "offset" && unit == "fraction") return "'offset fraction' is not a valid coordinate";  // (no box to be a fraction of)
    return "";
}

Annotation::Annotation(const rc::Params& rc, Axes& axes, std::string s, std::array<double, 2> xy,
                       std::array<double, 2> xytext, AnnotationCoords xycoords, AnnotationCoords textcoords,
                       std::optional<bool> clip)
    : Text(rc, xytext[0], xytext[1], std::move(s)), axes_(axes), xy_(xy), xycoords_(std::move(xycoords)),
      textcoords_(std::move(textcoords)), clip_(clip) {
    transform = std::make_shared<transforms::IdentityTransform>();  // (Axes.annotate sets it)
    transformSet = true;
}

transforms::TransformPtr Annotation::oneTransform(Renderer& renderer, const std::string& name) {
    if (name == "data") return axes_.transData;
    std::string bbox, unit;
    split(name, &bbox, &unit);
    std::array<double, 2> xy0{0, 0};
    std::array<double, 4> box{0, 0, 0, 0};
    if (bbox == "figure" || bbox == "subfigure") box = axes_.figure.bbox->points();  // (figbbox; no subfigures)
    else if (bbox == "axes") box = axes_.bbox->points();
    if (bbox == "offset") xy0 = positionXY(renderer);
    else xy0 = {box[0], box[1]};  // bbox0.p0
    auto tr = std::make_shared<transforms::Affine2D>();
    const double dpi = axes_.figure.dpi;
    if (unit == "points") tr->scale(dpi / 72);
    else if (unit == "fontsize") tr->scale(font.size * dpi / 72);
    else if (unit == "fraction") tr->scale(box[2] - box[0], box[3] - box[1]);  // *bbox0.size
    tr->translate(xy0[0], xy0[1]);
    return tr;
}

transforms::TransformPtr Annotation::coordsTransform(Renderer& renderer, const AnnotationCoords& coords) {
    if (coords.box) return std::make_shared<transforms::BboxTransformTo>(std::make_shared<transforms::Bbox>(coords.box(renderer)));
    if (coords.names.size() == 2) return transforms::blend(oneTransform(renderer, coords.names[0]), oneTransform(renderer, coords.names[1]));
    return oneTransform(renderer, coords.names[0]);
}

std::array<double, 2> Annotation::positionXY(Renderer& renderer) {
    return coordsTransform(renderer, xycoords_)->transform(xy_[0], xy_[1]);
}

bool Annotation::checkXY(Renderer& renderer) {
    if ((clip_ && *clip_) || (!clip_ && xycoords_.isData())) {
        const auto p = positionXY(renderer);
        return axes_.containsPoint(p[0], p[1]);
    }
    return true;
}

void Annotation::setArrow(std::unique_ptr<FancyArrowPatch> arrow, const ArrowProps& props, std::array<double, 2> relpos) {
    arrow_ = std::move(arrow);
    arrowProps_ = props;
    arrowRelpos_ = relpos;
}

void Annotation::updatePositions(Renderer& renderer) {
    transform = coordsTransform(renderer, textcoords_);
    if (!arrow_) return;
    const auto bbox = Text::windowExtent(renderer).points();
    const auto end = positionXY(renderer);  // the annotated point
    const double x1 = end[0], y1 = end[1];
    const double ms = arrowProps_.mutationScale ? *arrowProps_.mutationScale : font.size;
    arrow_->mutationScale = ms;
    if (!arrowProps_.hasArrowstyle) {
        // the modified YAArrow API: a 'simple' arrow from the side or middle of the text nearest xy
        ArrowStyle::make("simple", {{"head_length", arrowProps_.headlength / ms}, {"head_width", arrowProps_.headwidth / ms},
                                    {"tail_width", arrowProps_.width / ms}},
                         &arrow_->arrowStyle, nullptr);
        const auto nearest = [](const std::array<std::pair<double, double>, 3>& pos, double target) {
            std::size_t best = 0;  // min(pos, key=...): the first of equal distances
            for (std::size_t i = 1; i < 3; ++i)
                if (std::fabs(pos[i].first - target) < std::fabs(pos[best].first - target)) best = i;
            return pos[best];
        };
        const auto [nx, relposx] = nearest({{{bbox[0], 0}, {(bbox[0] + bbox[2]) / 2, 0.5}, {bbox[2], 1}}}, x1);
        const auto [ny, relposy] = nearest({{{bbox[1], 0}, {(bbox[1] + bbox[3]) / 2, 0.5}, {bbox[3], 1}}}, y1);
        arrowRelpos_ = {relposx, relposy};
        const double r = std::hypot(ny - y1, nx - x1);
        const double shrinkPts = arrowProps_.shrink * r / renderer.pointsToPixels(1);
        arrow_->shrinkA = arrow_->shrinkB = shrinkPts;
    }
    // arrow_begin = bbox.p0 + bbox.size * relpos
    const double w = bbox[2] - bbox[0], h = bbox[3] - bbox[1];
    arrow_->setPositions({bbox[0] + w * arrowRelpos_[0], bbox[1] + h * arrowRelpos_[1]}, {x1, y1});
    // patchA: none when given as None or the text is empty; else a box 4 points larger than the text's
    if (arrowProps_.patchAGiven || (!bboxPatch && text.empty())) {
        patchA_.reset();
        arrow_->patchA = nullptr;
        return;
    }
    if (bboxPatch) {  // the text's box
        const FancyBboxPatch* box = bboxPatch.get();
        arrow_->patchA = [box](const bezier::Point& p) { return box->containsPoint(p[0], p[1]); };
        return;
    }
    const double pad = renderer.pointsToPixels(4);
    patchA_ = std::make_unique<Rectangle>(rc_, bbox[0] - pad / 2, bbox[1] - pad / 2, w + pad, h + pad);
    patchA_->transform = std::make_shared<transforms::IdentityTransform>();
    patchA_->transformSet = true;
    patchA_->clipOn = false;
    const Rectangle* box = patchA_.get();
    arrow_->patchA = [box](const bezier::Point& p) { return box->containsPoint(p[0], p[1]); };
}

void Annotation::draw(Renderer& renderer) {
    if (!visible || !checkXY(renderer)) return;
    updatePositions(renderer);
    updateBboxPositionSize(renderer);
    if (arrow_) arrow_->draw(renderer);
    Text::draw(renderer);
}

transforms::Bbox Annotation::windowExtent(Renderer& renderer) {
    if (!visible || !checkXY(renderer)) return transforms::Bbox({0, 0, 1, 1});  // Bbox.unit()
    updatePositions(renderer);
    const auto textBox = Text::windowExtent(renderer);
    if (!arrow_) return textBox;
    // Bbox.union([text, arrow]): the least and greatest of each box's least and greatest coordinates
    const auto a = textBox.points(), b = arrow_->windowExtent().points();
    const auto lo = [](double p, double q) { return std::min(p, q); };
    const auto hi = [](double p, double q) { return std::max(p, q); };
    return transforms::Bbox({lo(std::min(a[0], a[2]), std::min(b[0], b[2])), lo(std::min(a[1], a[3]), std::min(b[1], b[3])),
                             hi(std::max(a[0], a[2]), std::max(b[0], b[2])), hi(std::max(a[1], a[3]), std::max(b[1], b[3]))});
}

}  // namespace calculate_plot::engine
