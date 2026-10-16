// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (_AnnotationBase, Annotation without arrows).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/text.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Axes;

// A coordinate system of an annotation: one name ('data', 'axes fraction', 'figure pixels', 'offset
// points', ...), or one for x and one for y; or a box given at draw time (a callable returning a
// BboxBase: bar_label's labels in the middle of their bars).
struct AnnotationCoords {
    std::vector<std::string> names;
    std::function<transforms::Bbox(Renderer&)> box;
    bool isData() const { return !box && names.size() == 1 && names[0] == "data"; }  // (xycoords == "data")
};

// Why `name` is not a coordinate system matplotlib takes (or one not supported yet); "" when it is.
// `offset`: whether 'offset ...' may be used (textcoords, not xycoords).
std::string annotationCoordsError(const std::string& name, bool offset);

// Annotation(text, xy, xytext, xycoords, textcoords, annotation_clip): a Text at xytext in the text
// coordinates, which may be relative to xy in its own; drawn only where _check_xy allows.
class Annotation : public Text {
public:
    Annotation(const rc::Params& rc, Axes& axes, std::string s, std::array<double, 2> xy, std::array<double, 2> xytext,
               AnnotationCoords xycoords, AnnotationCoords textcoords, std::optional<bool> clip);
    void draw(Renderer& renderer) override;
    transforms::Bbox windowExtent(Renderer& renderer) override;
    // _check_xy: whether it is drawn (with annotation_clip True, or None and xycoords 'data': the
    // annotated point in the Axes)
    bool checkXY(Renderer& renderer);

private:
    transforms::TransformPtr coordsTransform(Renderer& renderer, const AnnotationCoords& coords);  // _get_xy_transform
    transforms::TransformPtr oneTransform(Renderer& renderer, const std::string& name);
    std::array<double, 2> positionXY(Renderer& renderer);  // _get_position_xy: the annotated point in pixels
    void updatePositions(Renderer& renderer);                // the text's transform: the text coordinates'
    Axes& axes_;
    std::array<double, 2> xy_;
    AnnotationCoords xycoords_, textcoords_;
    std::optional<bool> clip_;
};

}  // namespace calculate_plot::engine
