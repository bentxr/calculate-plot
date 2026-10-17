#pragma once

#include <string>

#include "support/json.hpp"

namespace calculate_plot::support {

// ast.literal_eval(text), as far as the rcParams need it: for the literals made of None, booleans,
// integers, floats, strings, tuples and lists. The value comes as a figure file writes it: a tuple
// is {"$tuple": [...]} and a float that is not finite {"$float": "inf" | "-inf" | "nan"}.
struct PythonLiteral {
    enum class State {
        Value,       // the text is such a literal
        NotOne,      // Python raises, or the literal has something else in it (bytes, a set, 1j, ...)
        Unsupported  // not read here: string prefixes and escapes, triple quotes, 0x10, line breaks,
                     // integers beyond 64 bits, characters outside ASCII that are not in a string
    };
    State state = State::NotOne;
    Json value;
};
PythonLiteral pythonLiteral(const std::string& text);

}  // namespace calculate_plot::support
