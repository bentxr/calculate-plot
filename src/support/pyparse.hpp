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

// text.strip() of a Python str (UTF-8 here): without the white space at its ends, which for Python
// is what str.isspace says: the blanks of ASCII (with the separators 0x1c to 0x1f, which float() and
// int() do not take as blanks) and those of
// Unicode (U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F, U+3000).
std::string pythonStrip(const std::string& text);

// Whether every character is ASCII.
bool isAscii(const std::string& text);

}  // namespace calculate_plot::support
