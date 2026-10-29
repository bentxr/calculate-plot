// Derived from Matplotlib 3.11.2: lib/matplotlib/table.py (Cell, Table).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/artist.hpp"
#include "engine/text.hpp"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::engine {

class Axes;

// table.Cell: a Rectangle (edge 'k', face 'w', not clipped) with a Text in display coordinates, the edges drawn
// those of visible_edges (a substring of 'BRTL')
class Cell : public Rectangle {
public:
    Cell(const rc::Params& rc, double width, double height, const std::string& edges, const std::string& text,
         const colors::Spec& facecolor, const std::string& loc);
    Path path() const override;  // get_path: a MOVETO where an edge is not drawn
    void draw(Renderer& renderer) override;
    double requiredWidth(Renderer& renderer);   // get_required_width: the text's width and its padding, in axes units
    double autoSetFontSize(Renderer& renderer);  // auto_set_font_size: smaller by a point until the text fits
    Text text;
    std::string visibleEdges;
};

// table.Table: cells at (row, column) in axes coordinates, aligned in rows and columns, their font shrunk to fit
// (auto_set_font_size), columns sized to their text (auto_set_column_width), placed by loc or in bbox
class Table : public Artist {
public:
    // Table(ax, loc, bbox): false and matplotlib's message for an unknown location
    static std::unique_ptr<Table> make(const rc::Params& rc, Axes& ax, const std::string& loc,
                                       std::optional<std::array<double, 4>> bbox, std::string* error);
    Cell& addCell(int row, int col, double width, double height, const std::string& text, const colors::Spec& face,
                  const std::string& loc);
    double approxTextHeight() const;  // _approx_text_height
    void setFontsize(double size);
    void draw(Renderer& renderer) override;
    std::string edges = "closed";
    std::vector<int> autoColumns;
    bool autoFontsize = true;

private:
    Table(const rc::Params& rc, Axes& ax) : Artist(rc), ax_(ax) {}
    void updatePositions(Renderer& renderer);
    void doCellAlignment();
    Axes& ax_;
    int loc_ = 0;
    std::optional<std::array<double, 4>> bbox_;
    std::vector<std::pair<int, int>> order_;  // the cells as they were added (a dict's order)
    std::map<std::pair<int, int>, std::unique_ptr<Cell>> cells_;
};

}  // namespace calculate_plot::engine
