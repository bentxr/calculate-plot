// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (Text: layout, drawing, window extent).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/arrows.hpp"
#include "engine/artist.hpp"
#include "support/pyformat.hpp"
#include "text/font_manager.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

class Figure;

// cbook._auto_format_str(fmt, value): fmt % (value,), or fmt.format(value) where that raises a TypeError
// or a ValueError. `value` is a float, or an int when `isInt`. Raised: the exception matplotlib lets
// through; Unsupported: a form not ported.
support::Formatted autoFormatStr(const std::string& fmt, double value, bool isInt);

class Text : public Artist {
public:
    // Text(x, y, text) with the rcParams' defaults (text.color, the font, text.antialiased, ...).
    Text(const rc::Params& rc, double x, double y, std::string text);

    void setRotation(double degrees);           // a number: kept modulo 360
    void setRotation(const std::string& name);  // "horizontal" or "vertical"
    double rotation() const { return rotation_; }
    void setColor(const std::string& color);

    // One line of the layout: its text, width, ascent and descent, and where its origin goes
    // relative to the anchor (rotated).
    struct Line {
        std::string text;
        double width, ascent, descent, x, y;
    };
    // Text._get_layout: the rotated box around the text, the lines, and the text-aligned box
    // (its lower-left corner and size).
    struct Layout {
        transforms::Bbox bbox{{0, 0, 0, 0}};
        std::vector<Line> lines;
        double cornerX = 0, cornerY = 0, boxWidth = 0, boxHeight = 0;
    };
    Layout layout(Renderer& renderer) const;
    // get_window_extent (an Annotation first places itself, hence not const)
    virtual transforms::Bbox windowExtent(Renderer& renderer);
    void draw(Renderer& renderer) override;

    double x, y;
    std::string text;
    colors::Rgba color{};
    text::FontProperties font;
    std::string horizontalAlignment = "left";
    std::string verticalAlignment = "baseline";
    std::optional<std::string> multialignment;
    std::optional<double> linespacing;  // nullopt: 'normal' (the font's own line metrics)
    std::string rotationMode = "default";
    bool antialiased;
    bool parseMath;
    std::optional<std::vector<std::string>> features;
    std::optional<std::string> language;
    const Figure* figure = nullptr;  // the root figure, whose dpi the layout uses
    // set_bbox: the box drawn behind the text (FancyBboxPatch), placed around its layout when drawn
    std::unique_ptr<FancyBboxPatch> bboxPatch;
    void updateBboxPositionSize(Renderer& renderer);

private:
    double rotation_ = 0;
};

}  // namespace calculate_plot::engine
