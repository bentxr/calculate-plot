// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py (NavigationToolbar2: press_pan, drag_pan,
// press_zoom, release_zoom, _start_event_axes_interaction, _mouse_event_to_message, LocationEvent,
// FigureCanvasBase.inaxes), lib/matplotlib/axes/_base.py (format_coord, start_pan,
// _get_pan_points, drag_pan, _prepare_view_from_bbox, _set_view_from_bbox, set_xbound / set_ybound) and
// lib/matplotlib/backends/backend_qt.py (FigureCanvasQT.mouseEventCoords).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/axes.hpp"
#include "engine/figure.hpp"

#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

// A toolbar gesture in logical pixels from the canvas' top-left (Qt's widget coordinates).
struct NavigationGesture {
    bool zoom = false;  // the zoom-to-rectangle tool; else pan
    int button = 1;     // MouseButton: 1 left, 3 right
    std::string key;    // event.key
    double fromX = 0, fromY = 0, toX = 0, toY = 0;
};

// What the toolbar sets on one Axes: set_xlim / set_ylim with these bounds (nullopt: None, the old bound stays).
struct LimitChange {
    Axes* axes = nullptr;
    bool x = true;
    std::optional<double> lo, hi;
};

// The limits the gesture sets, in the order the toolbar sets them, on a figure drawn on a canvas at `ratio`
// device pixels per logical pixel (its layout and aspects applied). Nothing: the toolbar does nothing.
std::vector<LimitChange> navigate(Figure& figure, double ratio, const NavigationGesture& gesture);

// NavigationToolbar2._mouse_event_to_message for the pointer at (x, y), logical pixels from the top-left, on the
// same drawn figure: the format_coord of the topmost Axes under it ("" outside every Axes, or where matplotlib
// raises while formatting).
std::string pointerText(Figure& figure, double ratio, double x, double y);

}  // namespace calculate_plot::engine
