#pragma once

#include <optional>
#include <string>

namespace calculate_plot::support {

// float(text) and int(text) as Python reads a string: blanks around it, a sign, digits with single
// underscores between them, and for a float a fraction, an exponent, "inf", "infinity" and "nan" in
// any case. nullopt where Python raises ValueError, and for text that is not ASCII (Python also
// reads the digits and blanks of other scripts). An integer beyond 64 bits is the nearest end of
// their range.
std::optional<double> pythonFloat(const std::string& text);
std::optional<long long> pythonInt(const std::string& text);

}  // namespace calculate_plot::support
