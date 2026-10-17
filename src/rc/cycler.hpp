// Derived from cycler 0.12.1: cycler/__init__.py (Cycler: +, *, slices, by_key; concat).
// Copyright (c) 2015, matplotlib project. All rights reserved.
// Licence: LICENSES/cycler.txt (BSD-3-Clause). Changes: licensing/changes/cycler.md
#pragma once

#include "support/json.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::rc {

// A property cycle as the cycler package composes it, held as Cycler.by_key() gives it: the values
// of each property, all lists of one length, the properties in alphabetical order. The operations
// give nothing, with the reason in *error, where the package raises.
class Cycler {
public:
    // cycler(label, values)
    static Cycler of(std::string property, std::vector<support::Json> values);

    std::size_t size() const { return properties_.empty() ? 0 : properties_.front().second.size(); }  // len()
    std::vector<std::string> keys() const;
    bool has(const std::string& key) const;
    const std::vector<support::Json>& values(const std::string& key) const;  // by_key()[key]

    // a + b: the values pair by pair (zip)
    static std::optional<Cycler> add(const Cycler& a, const Cycler& b, std::string* error);
    // a * b: every value of b for each of a (itertools.product)
    static std::optional<Cycler> multiply(const Cycler& a, const Cycler& b, std::string* error);
    // cycle * n: the values n times over
    std::optional<Cycler> times(long long n, std::string* error) const;
    // cycle[lower:upper:step]
    std::optional<Cycler> slice(std::optional<long long> lower, std::optional<long long> upper, std::optional<long long> step,
                                std::string* error) const;
    // concat(a, b): b after a
    static std::optional<Cycler> concat(const Cycler& a, const Cycler& b, std::string* error);

    // {"$cycler": {property: [values]}}
    support::Json toJson() const;

private:
    // reduce(add, (cycler(k, v) for k, v in by_key)): how the package puts changed values together again
    static std::optional<Cycler> summed(std::vector<std::pair<std::string, std::vector<support::Json>>> properties, std::string* error);

    std::vector<std::pair<std::string, std::vector<support::Json>>> properties_;
};

// The indices of Python's sequence[lower:upper:step] for a sequence of that length (nothing when
// the step is zero).
std::optional<std::vector<std::size_t>> sliceIndices(std::size_t length, std::optional<long long> lower, std::optional<long long> upper,
                                                     std::optional<long long> step);

}  // namespace calculate_plot::rc
