// Derived from Matplotlib 3.11.2: lib/matplotlib/_fontconfig_pattern.py (parse_fontconfig_pattern).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::support {

// What parse_fontconfig_pattern returns: the properties with their values, in the order the names
// first appear. "family" comes first and is always there (a family may be empty); "size" follows
// if the pattern has sizes after a hyphen; a constant (bold, italic, condensed, ...) is the
// property it stands for: weight, slant or width.
struct FontconfigPattern {
    std::vector<std::pair<std::string, std::vector<std::string>>> properties;
};

// str.expandtabs() of UTF-8 text: what pyparsing does to a string before it parses it.
std::string expandTabs(const std::string& text);

// nullopt for what is not a pattern (matplotlib raises ValueError). The pattern is read as pyparsing
// reads a string: with its tabs expanded to spaces.
std::optional<FontconfigPattern> parseFontconfigPattern(const std::string& pattern);

}  // namespace calculate_plot::support
