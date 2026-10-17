#pragma once

#include "support/json.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace calculate_plot::rc {

// matplotlib's rcParams: every setting with its default from matplotlib 3.11.2 (data/rcdefaults.json,
// embedded in the library), changed with set().
class Params {
public:
    static Params defaults();

    std::size_t size() const { return values_.size(); }
    bool has(const std::string& key) const { return values_.count(key) != 0; }
    std::vector<std::string> keys() const;  // in alphabetical order
    const support::Json& value(const std::string& key) const;

    double number(const std::string& key) const;
    bool boolean(const std::string& key) const;
    std::string text(const std::string& key) const;
    std::vector<double> numbers(const std::string& key) const;
    std::vector<std::string> strings(const std::string& key) const;
    // rcParams["axes.prop_cycle"] entry by entry (list(cycler)): the properties each one sets, in
    // alphabetical order, with their values (a colour is a text or RGB(A) numbers).
    std::vector<std::vector<std::pair<std::string, support::Json>>> propCycle() const;
    // Its colours, by_key().get("color", ["k"]): what "C0", "C1" ... stand for.
    std::vector<support::Json> colorCycle() const;

    // rcParams[key] = value: the value is checked by matplotlib's validator of the key and stored in
    // the form matplotlib stores it (rc_validate.hpp). What matplotlib takes and calculate-plot does
    // not do yet is refused as "not supported yet", never taken and ignored.
    bool set(const std::string& key, const support::Json& value, std::string* error);
    // rcParams[key] = rcParamsDefault[key].
    void reset(const std::string& key);

    // The keys of what calculate-plot does not draw at all: they are checked and stored, and change
    // nothing. In alphabetical order.
    static const std::vector<std::string>& inertKeys();
    // The keys of what calculate-plot does not do yet: they take their default value and no other.
    static const std::vector<std::string>& unsupportedKeys();
    // For the tests: every key read on this thread from now on is added to `log` (nullptr: to none).
    static void logReads(std::set<std::string>* log);

private:
    static const Params& standard();  // the defaults themselves
    std::map<std::string, support::Json> values_;
};

}  // namespace calculate_plot::rc
