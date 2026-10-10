#include "rc/rc_params.hpp"

#include "support/embedded.hpp"

namespace calculate_plot::rc {

using support::Json;

namespace {

// The kind of a value, for checking a new value against the default's.
enum class Kind { Null, Bool, Number, String, List, Other };

Kind kindOf(const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: return Kind::Null;
    case Json::Type::Bool: return Kind::Bool;
    case Json::Type::Number: return Kind::Number;
    case Json::Type::String: return Kind::String;
    case Json::Type::Array: return Kind::List;
    default: return Kind::Other;
    }
}

}  // namespace

Params Params::defaults() {
    static const Params loaded = [] {
        Params p;
        const auto doc = support::parseJson(embedded::file("data/rcdefaults.json"), nullptr);
        if (doc)
            if (const Json* values = doc->find("rcParams"))
                for (const auto& [key, value] : values->members()) p.values_[key] = value;
        return p;
    }();
    return loaded;
}

const Json& Params::value(const std::string& key) const {
    static const Json null;
    auto it = values_.find(key);
    return it == values_.end() ? null : it->second;
}

double Params::number(const std::string& key) const { return value(key).asNumber(); }
bool Params::boolean(const std::string& key) const { return value(key).asBool(); }
std::string Params::text(const std::string& key) const { return value(key).asString(); }

std::vector<double> Params::numbers(const std::string& key) const {
    std::vector<double> out;
    for (const Json& v : value(key).items()) out.push_back(v.asNumber());
    return out;
}

std::vector<std::string> Params::strings(const std::string& key) const {
    std::vector<std::string> out;
    for (const Json& v : value(key).items()) out.push_back(v.asString());
    return out;
}

std::vector<std::vector<double>> Params::colorCycle() const {
    std::vector<std::vector<double>> out;
    const Json* cycler = value("axes.prop_cycle").find("$cycler");
    const Json* colors = cycler ? cycler->find("color") : nullptr;
    if (!colors) return out;
    for (const Json& c : colors->items()) {
        std::vector<double> rgb;
        for (const Json& x : c.items()) rgb.push_back(x.asNumber());
        out.push_back(rgb);
    }
    return out;
}

bool Params::set(const std::string& key, const Json& v, std::string* error) {
    auto it = values_.find(key);
    if (it == values_.end()) {
        if (error) *error = key + " is not a valid rc parameter";
        return false;
    }
    const Kind want = kindOf(it->second), got = kindOf(v);
    // A default of None accepts anything matplotlib would validate; otherwise the kinds must agree.
    // savefig.dpi is 'figure' or a number.
    const bool numberForName = key == "savefig.dpi" && got == Kind::Number;
    if (want != Kind::Null && got != want && !numberForName) {
        if (error) *error = "the value given for " + key + " is not of the right kind";
        return false;
    }
    it->second = v;
    return true;
}

}  // namespace calculate_plot::rc
