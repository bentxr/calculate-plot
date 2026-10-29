// Derived from Matplotlib 3.11.2: lib/matplotlib/streamplot.py (Grid, StreamMask, DomainMap, the integrator,
// interpgrid, _gen_starting_points).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::engine {

// streamplot's grid: x and y equally spaced and increasing (Grid)
struct StreamGrid {
    int nx = 0, ny = 0;
    double dx = 0, dy = 0, xOrigin = 0, yOrigin = 0, width = 0, height = 0;
};

// Grid(x, y) of 1-D x and y (the rows of a 2-D x and the columns of a 2-D y already checked equal). False and
// matplotlib's message when it raises.
bool streamGrid(const std::vector<double>& x, const std::vector<double>& y, StreamGrid* out, std::string* error);

// interpgrid(a, xi, yi) of an ny x nx array (row by row; NaN for masked) at a point: false where a masked value
// takes part (TerminateTrajectory)
bool interpgrid(const std::vector<double>& a, int nx, int ny, double xi, double yi, double* out);

struct StreamOptions {
    double densityX = 1, densityY = 1;
    double minlength = 0.1, maxlength = 4.0;
    std::string direction = "both";  // integration_direction
    bool broken = true;              // broken_streamlines
    double maxStepScale = 1.0, maxErrorScale = 1.0;
    std::optional<std::vector<std::array<double, 2>>> startPoints;  // in data coordinates
};

// The trajectories of the field (u, v: ny x nx, row by row, NaN where masked) in grid coordinates, as streamplot
// integrates them from the spiral of the mask's cells or from the start points. False and matplotlib's message
// when it raises.
bool streamTrajectories(const StreamGrid& grid, const std::vector<double>& u, const std::vector<double>& v,
                        const StreamOptions& options, std::vector<std::vector<std::array<double, 2>>>* out,
                        std::string* error);

}  // namespace calculate_plot::engine
