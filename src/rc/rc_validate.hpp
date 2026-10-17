// Derived from Matplotlib 3.11.2: lib/matplotlib/rcsetup.py (the validators of the rcParams).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <string>

#include "support/json.hpp"

namespace calculate_plot::rc {

// What rcParams[key] = value does in matplotlib: the validator of the key takes the value and stores
// it (often in another form: "yes" as True, "5" as 5.0, a tuple as a list here), or raises.
struct Validated {
    enum class State {
        Taken,       // `value` is what matplotlib stores
        Refused,     // matplotlib raises
        Unsupported  // not known here: a cycler, a date, a backend, text that only Python reads
    };
    State state = State::Refused;
    support::Json value;
    std::string error;  // why not, when it is not taken
};

// `value` is as a figure file writes it: a tuple is {"$tuple": [...]}, a float that is not finite
// {"$float": "nan" | "inf" | "-inf"}. In what is stored a tuple is a list, and such a float a
// number that is not finite. An unknown key is refused.
Validated validate(const std::string& key, const support::Json& value);

}  // namespace calculate_plot::rc
