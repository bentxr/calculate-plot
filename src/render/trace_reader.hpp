#pragma once

// Reading recorded traces (tools/oracle/record.py) safely: every field, type and size is checked
// before anything reaches a renderer.

#include "support/json.hpp"
#include "text/text_render.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace calculate_plot::render {

using support::Json;

// Reads trace values, checking every field, type and size before anything reaches the renderer.
// The first problem is kept in `error`; after it, readers return harmless defaults.
class Reader {
public:
    std::string error;

    void fail(const std::string& message) {
        if (error.empty()) error = message;
    }

    const Json& field(const Json& object, const char* key) {
        static const Json null;
        const Json* value = object.isObject() ? object.find(key) : nullptr;
        if (!value) {
            fail(std::string("missing \"") + key + "\"");
            return null;
        }
        return *value;
    }

    double number(const Json& j) {
        if (j.isNumber()) return j.asNumber();
        if (j.isString()) {
            if (j.asString() == "nan") return std::numeric_limits<double>::quiet_NaN();
            if (j.asString() == "inf") return std::numeric_limits<double>::infinity();
            if (j.asString() == "-inf") return -std::numeric_limits<double>::infinity();
        }
        fail("expected a number");
        return 0.0;
    }

    bool boolean(const Json& j) {
        if (!j.isBool()) fail("expected true or false");
        return j.asBool();
    }

    std::string text(const Json& j) {
        if (!j.isString()) fail("expected a string");
        return j.asString();
    }

    long long integer(const Json& j) {
        const double v = number(j);
        if (!(v >= -9.0e15 && v <= 9.0e15) || v != std::floor(v)) fail("expected an integer");
        return error.empty() ? static_cast<long long>(v) : 0;
    }

    // A text's "language" (Text.get_language()): null, a name, or a list of [name, start, end] whose
    // bounds are C ints (True and False among them, kept as such).
    std::optional<text::Language> language(const Json& j) {
        if (j.isNull()) return std::nullopt;
        text::Language out;
        if (j.isString()) {
            out.name = j.asString();
            return out;
        }
        out.isRanges = true;
        if (!j.isArray()) {
            fail("expected a language or a list of language ranges");
            return out;
        }
        const auto bound = [&](const Json& b, int* value, bool* isBool) {
            *isBool = b.isBool();
            if (*isBool) return void(*value = b.asBool() ? 1 : 0);
            const long long v = integer(b);
            if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) fail("expected a C int");
            *value = error.empty() ? int(v) : 0;
        };
        for (const Json& r : j.items()) {
            if (!r.isArray() || r.size() != 3) {
                fail("expected [language, start, end]");
                return out;
            }
            text::LanguageRange range;
            range.language = text(r.items()[0]);
            bound(r.items()[1], &range.start, &range.startIsBool);
            bound(r.items()[2], &range.end, &range.endIsBool);
            out.ranges.push_back(range);
        }
        return out;
    }

    // An array of numbers; `count` (when not 0) is the exact length required.
    std::vector<double> numbers(const Json& j, std::size_t count = 0) {
        std::vector<double> out;
        if (!j.isArray()) {
            fail("expected an array of numbers");
            return std::vector<double>(count, 0.0);
        }
        if (count && j.size() != count) {
            fail("expected " + std::to_string(count) + " numbers");
            return std::vector<double>(count, 0.0);
        }
        out.reserve(j.size());
        for (const Json& item : j.items()) out.push_back(number(item));
        return out;
    }

    std::size_t dimension(const Json& j) {
        const long long v = integer(j);
        if (v < 0 || v > (1LL << 31)) fail("a dimension is out of range");
        return error.empty() ? std::size_t(v) : 0;
    }

};


}  // namespace calculate_plot::render
