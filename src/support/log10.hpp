#pragma once

namespace calculate_plot::support {

// The base-10 logarithm with the last bit glibc's log10 gives, on every target. matplotlib's log
// scales take theirs from the C library of the reference environment (glibc), whose log10 is not
// correctly rounded: another library's differs in the last bit for about 3 % of the numbers, which
// can move a tick or an edge to the next pixel.
double log10(double x);

}  // namespace calculate_plot::support
