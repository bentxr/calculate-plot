// Derived from Matplotlib 3.11.2: lib/matplotlib/inset.py (InsetIndicator) and lib/matplotlib/patches.py
// (ConnectionPatch, as the indicator uses it).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "engine/arrows.hpp"
#include "engine/artist.hpp"

#include <array>
#include <memory>
#include <optional>

namespace calculate_plot::engine {

class Axes;

// indicate_inset / indicate_inset_zoom: a rectangle in the parent's data (the inset's limits when bounds are
// not given), and four connectors from the inset's corners to the rectangle's, of which two are shown (chosen
// at the first draw). Connectors drawn in the rectangle's style make one compound path with it.
class InsetIndicator : public Artist {
public:
    InsetIndicator(const rc::Params& rc, Axes& parent, Axes* inset, std::optional<std::array<double, 4>> bounds);
    void draw(Renderer& renderer) override;
    Rectangle rectangle;

private:
    Axes& parent_;
    Axes* inset_;
    bool autoBounds_;
    std::optional<std::array<bool, 4>> connectorVisible_;  // set when the connectors are first made
};

}  // namespace calculate_plot::engine
