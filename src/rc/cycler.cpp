// Derived from cycler 0.12.1: cycler/__init__.py (Cycler: +, *, slices, by_key; concat).
// Copyright (c) 2015, matplotlib project. All rights reserved.
// Licence: LICENSES/cycler.txt (BSD-3-Clause). Changes: licensing/changes/cycler.md
#include "rc/cycler.hpp"

#include <algorithm>

namespace calculate_plot::rc {

using support::Json;

namespace {

bool fail(std::string* error, const char* why) {
    *error = why;
    return false;
}

// _process_keys looks at the first item of each side (next(iter(...)) with nothing there breaks),
// and the two sides cannot share a property.
bool composable(const Cycler& a, const Cycler& b, std::string* error) {
    if (a.size() == 0 || b.size() == 0) return fail(error, "an empty cycle cannot be composed");
    for (const std::string& key : a.keys())
        if (b.has(key)) return fail(error, "Can not compose overlapping cycles");
    return true;
}

}  // namespace

Cycler Cycler::of(std::string property, std::vector<Json> values) {
    Cycler c;
    c.properties_.emplace_back(std::move(property), std::move(values));
    return c;
}

std::vector<std::string> Cycler::keys() const {
    std::vector<std::string> out;
    for (const auto& [key, values] : properties_) out.push_back(key);
    return out;
}

bool Cycler::has(const std::string& key) const {
    for (const auto& [name, values] : properties_)
        if (name == key) return true;
    return false;
}

const std::vector<Json>& Cycler::values(const std::string& key) const {
    static const std::vector<Json> none;
    for (const auto& [name, values] : properties_)
        if (name == key) return values;
    return none;
}

std::optional<Cycler> Cycler::add(const Cycler& a, const Cycler& b, std::string* error) {
    if (a.size() != b.size()) {
        *error = "Can only add equal length cycles, not " + std::to_string(a.size()) + " and " + std::to_string(b.size());
        return std::nullopt;
    }
    if (!composable(a, b, error)) return std::nullopt;
    Cycler out = a;
    out.properties_.insert(out.properties_.end(), b.properties_.begin(), b.properties_.end());
    std::sort(out.properties_.begin(), out.properties_.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    return out;
}

std::optional<Cycler> Cycler::multiply(const Cycler& a, const Cycler& b, std::string* error) {
    if (!composable(a, b, error)) return std::nullopt;
    Cycler out;
    for (const auto& [key, values] : a.properties_) {
        std::vector<Json> repeated;
        for (const Json& v : values) repeated.insert(repeated.end(), b.size(), v);
        out.properties_.emplace_back(key, std::move(repeated));
    }
    for (const auto& [key, values] : b.properties_) {
        std::vector<Json> repeated;
        for (std::size_t i = 0; i < a.size(); ++i) repeated.insert(repeated.end(), values.begin(), values.end());
        out.properties_.emplace_back(key, std::move(repeated));
    }
    std::sort(out.properties_.begin(), out.properties_.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    return out;
}

std::optional<Cycler> Cycler::summed(std::vector<std::pair<std::string, std::vector<Json>>> properties, std::string* error) {
    Cycler out = of(properties[0].first, std::move(properties[0].second));
    for (std::size_t i = 1; i < properties.size(); ++i) {
        const auto next = add(out, of(properties[i].first, std::move(properties[i].second)), error);
        if (!next) return std::nullopt;
        out = *next;
    }
    return out;
}

std::optional<Cycler> Cycler::times(long long n, std::string* error) const {
    auto properties = properties_;
    for (auto& [key, values] : properties) {
        const std::vector<Json> once = values;
        values.clear();
        for (long long i = 0; i < n; ++i) values.insert(values.end(), once.begin(), once.end());
    }
    return summed(std::move(properties), error);
}

std::optional<std::vector<std::size_t>> sliceIndices(std::size_t length, std::optional<long long> lower, std::optional<long long> upper,
                                                     std::optional<long long> step) {
    // PySlice_Unpack and PySlice_AdjustIndices
    const long long n = static_cast<long long>(length), by = step ? *step : 1;
    if (by == 0) return std::nullopt;
    // going backwards the bounds are one place lower: from the last item down to before the first
    const bool backwards = by < 0;
    const long long lowest = backwards ? -1 : 0, highest = backwards ? n - 1 : n;
    const auto bound = [&](std::optional<long long> given, long long whenNone) {
        if (!given) return whenNone;
        return std::clamp(*given < 0 ? *given + n : *given, lowest, highest);  // a negative index counts from the end
    };
    const long long start = bound(lower, backwards ? highest : lowest), stop = bound(upper, backwards ? lowest : highest);
    std::vector<std::size_t> out;
    for (long long i = start; backwards ? i > stop : i < stop; i += by) out.push_back(static_cast<std::size_t>(i));
    return out;
}

std::optional<Cycler> Cycler::slice(std::optional<long long> lower, std::optional<long long> upper, std::optional<long long> step,
                                    std::string* error) const {
    const auto indices = sliceIndices(size(), lower, upper, step);
    if (!indices) {
        *error = "slice step cannot be zero";
        return std::nullopt;
    }
    auto properties = properties_;
    for (auto& [key, values] : properties) {
        std::vector<Json> kept;
        for (std::size_t i : *indices) kept.push_back(values[i]);
        values = std::move(kept);
    }
    return summed(std::move(properties), error);
}

std::optional<Cycler> Cycler::concat(const Cycler& a, const Cycler& b, std::string* error) {
    if (a.keys() != b.keys()) {
        *error = "Keys do not match";
        return std::nullopt;
    }
    auto properties = a.properties_;
    for (auto& [key, values] : properties) {
        const auto& more = b.values(key);
        values.insert(values.end(), more.begin(), more.end());
    }
    return summed(std::move(properties), error);
}

Json Cycler::toJson() const {
    std::vector<std::pair<std::string, Json>> members;
    for (const auto& [key, values] : properties_) members.emplace_back(key, Json::array(values));
    return Json::object({{"$cycler", Json::object(std::move(members))}});
}

}  // namespace calculate_plot::rc
