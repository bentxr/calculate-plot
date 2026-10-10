#pragma once

#include "support/json.hpp"

#include <map>
#include <string>
#include <vector>

namespace calculate_plot::rc {

// matplotlib's rcParams: every setting with its default from matplotlib 3.11.2 (data/rcdefaults.json,
// embedded in the library), changed with set(), which checks the value against its kind.
class Params {
public:
    static Params defaults();

    std::size_t size() const { return values_.size(); }
    bool has(const std::string& key) const { return values_.count(key) != 0; }
    const support::Json& value(const std::string& key) const;

    double number(const std::string& key) const;
    bool boolean(const std::string& key) const;
    std::string text(const std::string& key) const;
    std::vector<double> numbers(const std::string& key) const;
    std::vector<std::string> strings(const std::string& key) const;
    std::vector<std::vector<double>> colorCycle() const;  // axes.prop_cycle's colours, RGB

    bool set(const std::string& key, const support::Json& value, std::string* error);

private:
    std::map<std::string, support::Json> values_;
};

}  // namespace calculate_plot::rc
