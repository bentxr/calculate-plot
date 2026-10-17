#pragma once
// Where matplotlib raises in the middle of a drawing (a ValueError from deep inside a locator, say), the
// port, which cannot raise there, notes matplotlib's message here (per thread; the first one counts) for
// whoever runs the drawing to ask and clear.
#include <string>

namespace calculate_plot::support {

inline std::string& raised() {
    thread_local std::string message;
    return message;
}

inline void noteRaised(const std::string& message) {
    if (raised().empty()) raised() = message;
}

}  // namespace calculate_plot::support
