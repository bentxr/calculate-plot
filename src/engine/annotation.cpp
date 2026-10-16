// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (_AnnotationBase, Annotation without arrows).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/annotation.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"

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

void Annotation::updatePositions(Renderer& renderer) { transform = coordsTransform(renderer, textcoords_); }

void Annotation::draw(Renderer& renderer) {
    if (!visible || !checkXY(renderer)) return;
    updatePositions(renderer);
    Text::draw(renderer);
}

transforms::Bbox Annotation::windowExtent(Renderer& renderer) {
    if (!visible || !checkXY(renderer)) return transforms::Bbox({0, 0, 1, 1});  // Bbox.unit()
    updatePositions(renderer);
    return Text::windowExtent(renderer);
}

}  // namespace calculate_plot::engine
