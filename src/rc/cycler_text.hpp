// Derived from Matplotlib 3.11.2: lib/matplotlib/rcsetup.py (_parse_cycler_string,
// _eval_cycler_expr, cycler, validate_cycler).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "rc/rc_validate.hpp"

#include <functional>
#include <string>

namespace calculate_plot::rc {

// The validator of the values of one property of a cycle (rcsetup._prop_validators), called with the
// property's full name: Taken with the list of values as they are stored.
using CycleValues = std::function<Validated(const std::string& property, const support::Json& values)>;

// validate_cycler(text): the cycle that a text makes, as a style sheet writes
// rcParams["axes.prop_cycle"] ("cycler('color', ['r', 'g'])", "cycler(c='rgb') + cycler(lw=[1, 2, 3])").
// Taken is {"$cycler": {property: [values]}}, the properties in alphabetical order. matplotlib reads
// the text with Python's parser: what Python writes in ways that are not read here (characters
// outside ASCII, arithmetic between numbers, sets, bytes ...) is Unsupported.
Validated cyclerFromText(const std::string& text, const CycleValues& values);

}  // namespace calculate_plot::rc
