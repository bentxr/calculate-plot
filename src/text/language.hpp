// Derived from Matplotlib 3.11.2: lib/matplotlib/text.py (Text.set_language),
// src/ft2font_wrapper.cpp (the languages FT2Font.layout takes).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <string>
#include <vector>

namespace calculate_plot::text {

// Text.get_language(): one language for the whole text, or (language, start, end) ranges of its
// characters, as Text.set_language took them. A bound given as True or False is kept as such (it is
// the integer 1 or 0 to the layout), so that a trace writes it back as matplotlib's does.
struct LanguageRange {
    std::string language;
    int start = 0, end = 0;
    bool startIsBool = false, endIsBool = false;
};

struct Language {
    bool isRanges = false;
    std::string name;                  // when !isRanges
    std::vector<LanguageRange> ranges;  // when isRanges
};

}  // namespace calculate_plot::text
