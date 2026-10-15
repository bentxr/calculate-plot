#pragma once

#include "support/json.hpp"

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace calculate_plot::rc { class Params; }

namespace calculate_plot::colors {

using Rgba = std::array<double, 4>;

// matplotlib.colors.to_rgba for a colour specification string: a name (base, tab:, CSS4, xkcd:),
// 'none', 'C0'..'Cn' (the property cycle in `rc`), a grey level such as '0.5', or #rgb, #rgba,
// #rrggbb, #rrggbbaa. nullopt (with *error) where matplotlib raises.
std::optional<Rgba> toRgba(const std::string& spec, std::optional<double> alpha, const rc::Params& rc,
                           std::string* error);
// The same for an RGB or RGBA tuple.
std::optional<Rgba> toRgba(const std::vector<double>& tuple, std::optional<double> alpha, std::string* error);

// A colour as a figure file gives it: a specification string, or an RGB(A) tuple.
using Spec = std::variant<std::string, std::vector<double>>;
std::optional<Rgba> toRgba(const Spec& spec, std::optional<double> alpha, const rc::Params& rc, std::string* error);
// A colour held as JSON (a text, or RGB(A) numbers) as a specification.
Spec specOf(const support::Json& color);
// cbook._str_lower_equal(spec, "none"), false for tuples.
bool isNone(const Spec& spec);
// _has_alpha_channel: '#rgba' and '#rrggbbaa' strings, and tuples of four (for a valid colour).
bool hasAlphaChannel(const Spec& spec);

// matplotlib.color_sequences[name]: the colours of a built-in colour sequence (data/color_sequences.json
// and data/color_sequences_colorbrewer.json) as RGB(A) numbers; nullptr for another name.
const std::vector<support::Json>* colorSequence(const std::string& name);

}  // namespace calculate_plot::colors
