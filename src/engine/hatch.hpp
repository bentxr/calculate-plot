// Derived from Matplotlib 3.11.2: lib/matplotlib/hatch.py (get_path and the hatch pattern classes).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/path.hpp"

#include <optional>
#include <string>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::engine {

// hatch.get_path(hatchpattern, density): the hatch in a unit square, as Path.hatch gives it to the
// renderers (GraphicsContextBase.get_hatch_path, density 6). Characters that are no pattern count for
// nothing (matplotlib warns that they are deprecated). nullopt where matplotlib raises: a pattern of
// shapes in an odd number of rows (its arrays have room for an even number only).
std::optional<Path> hatchPath(const std::string& pattern, const rc::Params& rc, int density = 6);

}  // namespace calculate_plot::engine
