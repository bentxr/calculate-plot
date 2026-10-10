#pragma once

#include <array>
#include <optional>
#include <string>
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

}  // namespace calculate_plot::colors
