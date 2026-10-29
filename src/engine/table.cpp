// Derived from Matplotlib 3.11.2: lib/matplotlib/table.py (Cell, Table, table) and lib/matplotlib/axes/_base.py
// (add_table).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/table.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "rc/rc_params.hpp"

#include <algorithm>
#include <limits>

namespace calculate_plot::engine {

namespace {

const double kPad = 0.1;       // Cell.PAD
const double kAxesPad = 0.02;  // Table.AXESPAD
const double kFontsize = 10;   // Table.FONTSIZE

const char* const kCodes[18] = {"best", "upper right", "upper left", "lower left", "lower right", "center left", "center right",
                                "lower center", "upper center", "center", "top right", "top left", "bottom left", "bottom right",
                                "right", "left", "top", "bottom"};

// Bbox.transformed(t): the lower-left, upper-left and lower-right corners through t
std::array<double, 4> transformed(const transforms::Bbox& b, const transforms::Transform& t) {
    const auto p = b.points();
    const auto ll = t.transform(p[0], p[1]), ul = t.transform(p[0], p[3]), lr = t.transform(p[2], p[1]);
    return {ll[0], ll[1], lr[0], ul[1]};
}

}  // namespace

Cell::Cell(const rc::Params& rc, double w, double h, const std::string& edges, const std::string& txt,
           const colors::Spec& facecolor, const std::string& loc)
    : Rectangle(rc, 0, 0, w, h), text(rc, 0, 0, txt) {
    setFill(true);
    setEdgecolor(colors::Spec(std::string("k")));
    setFacecolor(facecolor);
    clipOn = false;
    // visible_edges
    if (edges == "open") visibleEdges = "";
    else if (edges == "closed") visibleEdges = "BRTL";
    else if (edges == "horizontal") visibleEdges = "BT";
    else if (edges == "vertical") visibleEdges = "RL";
    else visibleEdges = edges;
    text.clipOn = false;
    text.horizontalAlignment = loc;
    text.verticalAlignment = "center";
}

Path Cell::path() const {
    static const char edges[4] = {'B', 'R', 'T', 'L'};
    std::vector<unsigned char> codes{MOVETO};
    for (char e : edges) codes.push_back(visibleEdges.find(e) != std::string::npos ? LINETO : MOVETO);
    if (std::find(codes.begin() + 1, codes.end(), MOVETO) == codes.end()) codes.back() = CLOSEPOLY;
    return Path::make({0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0}, std::move(codes), rc_);
}

void Cell::draw(Renderer& renderer) {
    if (!visible) return;
    Rectangle::draw(renderer);
    // _set_text_position: centred vertically, and by the alignment across
    const auto b = windowExtent().points();
    const double w = b[2] - b[0], h = b[3] - b[1];
    text.y = b[1] + h / 2;
    if (text.horizontalAlignment == "center") text.x = b[0] + w / 2;
    else if (text.horizontalAlignment == "left") text.x = b[0] + w * kPad;
    else text.x = b[0] + w * (1 - kPad);
    text.draw(renderer);
}

double Cell::requiredWidth(Renderer& renderer) {
    // get_text_bounds: the text's window extent through the inverse of the cell's data transform (transAxes)
    const auto b = transformed(text.windowExtent(renderer), *transform->inverted());
    return (b[2] - b[0]) * (1.0 + (2.0 * kPad));
}

double Cell::autoSetFontSize(Renderer& renderer) {
    double fontsize = text.font.size;
    double required = requiredWidth(renderer);
    while (fontsize > 1 && required > width) {
        fontsize -= 1;
        text.font.size = fontsize;
        required = requiredWidth(renderer);
    }
    return fontsize;
}

std::unique_ptr<Table> Table::make(const rc::Params& rc, Axes& ax, const std::string& loc, std::optional<std::array<double, 4>> bbox,
                                   std::string* error) {
    std::unique_ptr<Table> t(new Table(rc, ax));
    int code = -1;
    for (int i = 0; i < 18; ++i)
        if (loc == kCodes[i]) code = i;
    if (code < 0) {
        std::string valid;
        for (const char* c : kCodes) valid += std::string("\n\t") + c;
        return *error = "Unrecognized location '" + loc + "'. Valid locations are" + valid, nullptr;
    }
    t->loc_ = code;
    t->bbox_ = bbox;
    ax.unstaleViewLim();
    t->transform = ax.transAxes;
    t->transformSet = true;
    t->clipOn = false;
    return t;
}

Cell& Table::addCell(int row, int col, double width, double height, const std::string& txt, const colors::Spec& face,
                     const std::string& loc) {
    auto cell = std::make_unique<Cell>(rc_, width, height, edges, txt, face, loc);
    cell->text.figure = &ax_.figure;
    cell->transform = transform;
    cell->clipOn = false;
    const std::pair<int, int> key{row, col};
    if (!cells_.count(key)) order_.push_back(key);
    cells_[key] = std::move(cell);
    return *cells_[key];
}

double Table::approxTextHeight() const {
    const auto b = ax_.bbox->points();
    return kFontsize / 72.0 * ax_.figure.dpi / (b[3] - b[1]) * 1.2;
}

void Table::setFontsize(double size) {
    for (const auto& key : order_) cells_[key]->text.font.size = size;
}

void Table::doCellAlignment() {
    std::map<int, double> widths, heights;
    for (const auto& key : order_) {
        const Cell& c = *cells_[key];
        double& h = heights.emplace(key.first, 0.0).first->second;
        h = std::max(h, c.height);
        double& w = widths.emplace(key.second, 0.0).first->second;
        w = std::max(w, c.width);
    }
    std::map<int, double> lefts, bottoms;
    double xpos = 0;
    for (const auto& [col, w] : widths) lefts[col] = xpos, xpos += w;
    double ypos = 0;
    for (auto it = heights.rbegin(); it != heights.rend(); ++it) bottoms[it->first] = ypos, ypos += it->second;
    for (const auto& key : order_) {
        cells_[key]->x0 = lefts[key.second];
        cells_[key]->y0 = bottoms[key.first];
    }
}

void Table::updatePositions(Renderer& renderer) {
    for (int col : autoColumns) {  // _auto_set_column_width
        double maxWidth = 0;
        std::vector<Cell*> column;
        for (const auto& key : order_)
            if (key.second == col) column.push_back(cells_[key].get());
        for (Cell* c : column) maxWidth = std::max(maxWidth, c->requiredWidth(renderer));
        for (Cell* c : column) c->width = maxWidth;
    }
    if (autoFontsize && !order_.empty()) {  // _auto_set_font_size
        double fontsize = cells_[order_.front()]->text.font.size;
        for (const auto& key : order_) {
            if (std::find(autoColumns.begin(), autoColumns.end(), key.second) != autoColumns.end()) continue;
            fontsize = std::min(fontsize, cells_[key]->autoSetFontSize(renderer));
        }
        setFontsize(fontsize);
    }
    doCellAlignment();
    // _get_grid_bbox: the cells of row and column >= 0, in axes coordinates
    std::vector<transforms::Bbox> boxes;
    for (const auto& key : order_)
        if (key.first >= 0 && key.second >= 0) boxes.push_back(cells_[key]->windowExtent());
    const auto g = transformed(transforms::unionOf(boxes), *transform->inverted());
    const double l = g[0], b = g[1], w = g[2] - g[0], h = g[3] - g[1];
    double ox = 0, oy = 0;
    if (bbox_) {
        const auto& r = *bbox_;
        for (const auto& key : order_) {  // scale(rw / w, rh / h)
            cells_[key]->width = cells_[key]->width * (r[2] / w);
            cells_[key]->height = cells_[key]->height * (r[3] / h);
        }
        ox = r[0] - l;
        oy = r[1] - b;
        doCellAlignment();
    } else {
        enum { BEST, UR, UL, LL, LR, CL, CR, LC, UC, C, TR, TL, BL, BR, R, L, T, B };
        const int c = loc_;
        ox = (0.5 - w / 2) - l;
        oy = (0.5 - h / 2) - b;
        if (c == UL || c == LL || c == CL) ox = kAxesPad - l;
        if (c == BEST || c == UR || c == LR || c == R || c == CR) ox = 1 - (l + w + kAxesPad);
        if (c == BEST || c == UR || c == UL || c == UC) oy = 1 - (b + h + kAxesPad);
        if (c == LL || c == LR || c == LC) oy = kAxesPad - b;
        if (c == LC || c == UC || c == C) ox = (0.5 - w / 2) - l;
        if (c == CL || c == CR || c == C) oy = (0.5 - h / 2) - b;
        if (c == TL || c == BL || c == L) ox = -(l + w);
        if (c == TR || c == BR || c == R) ox = 1.0 - l;
        if (c == TR || c == TL || c == T) oy = 1.0 - b;
        if (c == BL || c == BR || c == B) oy = -(b + h);
    }
    for (const auto& key : order_) {  // _offset
        cells_[key]->x0 = cells_[key]->x0 + ox;
        cells_[key]->y0 = cells_[key]->y0 + oy;
    }
}

void Table::draw(Renderer& renderer) {
    if (!visible) return;
    renderer.openGroup("table", gid);
    updatePositions(renderer);
    for (auto& [key, cell] : cells_) cell->draw(renderer);  // (sorted by row, then column)
    renderer.closeGroup("table");
}

}  // namespace calculate_plot::engine
