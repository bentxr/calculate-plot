// Derived from Matplotlib 3.11.2: lib/matplotlib/style/__init__.py (use, _STYLE_BLACKLIST).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "rc/rc_params.hpp"
#include "support/json.hpp"

#include <string>
#include <vector>

namespace calculate_plot::rc {

// The names of the style sheets matplotlib bundles (style.library: data/styles.json and
// data/styles_solarized.json, embedded), in alphabetical order.
const std::vector<std::string>& styleNames();

// plt.style.use(spec): `spec` is a style name or a list of them, applied in turn to `params`. A style
// is one of styleNames(), "default" (the defaults themselves) or an alias of either (mpl20, mpl15);
// the settings that are not about style (_STYLE_BLACKLIST) are left alone. Each setting goes through
// Params::set as the text the style sheet gives, as rc_params_from_file sends it.
bool useStyle(Params& params, const support::Json& spec, std::string* error);

}  // namespace calculate_plot::rc
