// Derived from Matplotlib 3.11.2: lib/matplotlib/backend_bases.py (GraphicsContextBase).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/gc.hpp"

#include "rc/rc_params.hpp"

namespace calculate_plot::engine {

GC::GC(const rc::Params& rc) : hatchLinewidth(rc.number("hatch.linewidth")) {}

void GC::setForeground(const std::vector<double>& fg) {
    if (forcedAlpha) rgb = {fg[0], fg[1], fg[2], alpha};
    else rgb = fg;
}

void GC::setAlpha(std::optional<double> a) {
    if (a) {
        alpha = *a;
        forcedAlpha = true;
    } else {
        alpha = 1.0;
        forcedAlpha = false;
    }
    setForeground(rgb);
}

const char* GC::setDashes(double offset, const std::optional<std::vector<double>>& dashList) {
    if (const char* problem = dashListProblem(dashList)) return problem;
    dashOffset = offset;
    dashes = dashList;
    return nullptr;
}

const char* dashListProblem(const std::optional<std::vector<double>>& dashList) {
    if (!dashList) return nullptr;
    bool positive = false;
    for (double d : *dashList) {
        if (d < 0.0) return "All values in the dash list must be non-negative";
        positive = positive || d > 0.0;
    }
    if (!dashList->empty() && !positive) return "At least one value in the dash list must be positive";
    return nullptr;
}

}  // namespace calculate_plot::engine
