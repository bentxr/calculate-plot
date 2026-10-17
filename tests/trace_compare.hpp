#pragma once
// Compares traces (parsed JSON) the way the figure tests need: numbers as doubles with the sign of
// zero, objects by member, and names the first difference by its path.
#include "support/json.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace calculate_plot::testing_support {

using support::Json;

// The references come from glibc's libm (log10, sin, cos, pow, exp). Other libms (WASM's musl)
// differ in the last bits of some results, as matplotlib does from platform to platform (see the
// decision on libms): there the draw calls' numbers are compared within 1e-12, relative; pixels
// and SVG bytes are compared exactly everywhere.
#ifdef __GLIBC__
inline constexpr bool kReferenceLibm = true;
#else
inline constexpr bool kReferenceLibm = false;
#endif

// The oracles' C library is glibc 2.44. Earlier glibc versions (2.39 on the CI machines) give other last bits for
// sinh, cosh, asinh and atanh on some inputs; only figure files' functions ({"$lambda"}) use them.
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 44))
inline constexpr bool kReferenceHyperbolic = true;
#else
inline constexpr bool kReferenceHyperbolic = false;
#endif

// Equal with the sign of zero; within 1e-12 (relative) when the libm is not the reference (`exact` false).
inline bool sameNumber(double a, double b, bool exact = kReferenceLibm) {
    if (a == b) return std::signbit(a) == std::signbit(b);
    if (exact) return false;
    return std::fabs(a - b) <= 1e-12 * std::max({1.0, std::fabs(a), std::fabs(b)});
}

inline std::string describe(const Json& v) {
    switch (v.type()) {
        case Json::Type::Null: return "null";
        case Json::Type::Bool: return v.asBool() ? "true" : "false";
        case Json::Type::Number: return v.numberText();
        case Json::Type::String: return '"' + v.asString() + '"';
        case Json::Type::Array: return "array of " + std::to_string(v.size());
        case Json::Type::Object: return "object";
    }
    return {};
}

// Where two traces first differ ("events[3].gc.linewidth: 1.5 instead of 1.25"); empty when equal.
// Numbers compare as doubles, the sign of zero included (it reaches the SVG).
// `ignored` names object members left out of the comparison.
inline std::string firstDifference(const Json& got, const Json& want, const std::string& where,
                            const std::vector<std::string>& ignored = {}) {
    if (got.type() != want.type()) return where + ": " + describe(got) + " instead of " + describe(want);
    switch (want.type()) {
        case Json::Type::Null: return {};
        case Json::Type::Bool:
            return got.asBool() == want.asBool() ? std::string() : where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::Number:
            if (sameNumber(got.asNumber(), want.asNumber())) return {};
            return where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::String:
            return got.asString() == want.asString() ? std::string() : where + ": " + describe(got) + " instead of " + describe(want);
        case Json::Type::Array:
            for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
                const std::string d = firstDifference(got[i], want[i], where + "[" + std::to_string(i) + "]", ignored);
                if (!d.empty()) return d;
            }
            if (got.size() != want.size())
                return where + ": " + std::to_string(got.size()) + " elements instead of " + std::to_string(want.size());
            return {};
        case Json::Type::Object:
            for (const auto& [key, value] : want.members()) {
                if (std::find(ignored.begin(), ignored.end(), key) != ignored.end()) continue;
                const Json* mine = got.find(key);
                if (!mine) return where + "." + key + ": missing";
                const std::string d = firstDifference(*mine, value, where + "." + key, ignored);
                if (!d.empty()) return d;
            }
            for (const auto& [key, value] : got.members())
                if (!want.find(key)) return where + "." + key + ": not in matplotlib's trace";
            return {};
    }
    return {};
}

}  // namespace calculate_plot::testing_support
