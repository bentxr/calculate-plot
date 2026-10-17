// The public API's editing of figure files (include/calculate-plot/calculate-plot.hpp).
#include "calculate-plot/calculate-plot.hpp"

#include "engine/engine.hpp"
#include "model/figure_model.hpp"
#include "support/json.hpp"
#include "support/json_writer.hpp"

#include <algorithm>
#include <set>

namespace calculate_plot {

namespace {

using support::Json;

std::optional<Json> parseValue(const std::string& text, std::string* error) {
    support::JsonError e;
    auto v = support::parseJson(text, &e);
    if (!v) *error = "the value is not valid JSON: " + e.message;
    return v;
}

// The document and its calls, or nothing and why
struct Document {
    Json root;
    std::vector<Json> calls;
};

std::optional<Document> open(std::string_view figureFile, std::string* error) {
    std::string why;
    if (!model::parseFigure(figureFile, &why)) return *error = why, std::nullopt;
    Document d;
    d.root = *support::parseJson(figureFile, nullptr);
    d.calls = d.root.find("calls")->items();
    return d;
}

// The root of an object a call names: "ax" of "ax", "axs" of "axs[0, 1]", "ax" of "ax.yaxis"
std::string rootName(const std::string& on) { return on.substr(0, on.find_first_of("[.")); }

void namesOf(const Json& value, std::vector<std::string>* refs) {  // the {"$ref": name} values within
    if (value.isObject()) {
        if (const Json* r = value.find("$ref"); r && r->isString() && value.members().size() == 1) refs->push_back(r->asString());
        for (const auto& m : value.members()) namesOf(m.second, refs);
    } else if (value.isArray()) {
        for (const Json& x : value.items()) namesOf(x, refs);
    }
}

// The document with these calls, checked: a figure file whose calls use only objects earlier calls named
std::string close(const Document& d, const std::vector<Json>& calls, std::string* error) {
    std::vector<std::pair<std::string, Json>> members;
    for (const auto& [key, value] : d.root.members()) members.emplace_back(key, key == "calls" ? Json::array(calls) : value);
    const std::string text = support::toJsonText(Json::object(std::move(members)));
    std::string why;
    if (!model::parseFigure(text, &why)) return *error = why, std::string();
    std::set<std::string> named;
    for (std::size_t i = 0; i < calls.size(); ++i) {
        const Json* on = calls[i].find("on");
        std::vector<std::string> used;
        if (on && on->isString() && !on->asString().empty()) used.push_back(rootName(on->asString()));
        if (const Json* a = calls[i].find("args")) namesOf(*a, &used);
        if (const Json* k = calls[i].find("kwargs")) namesOf(*k, &used);
        for (const std::string& name : used)
            if (!named.count(name)) return *error = "call " + std::to_string(i) + " uses " + name + ", which no earlier call names", std::string();
        if (const Json* as = calls[i].find("as")) {
            if (as->isString()) named.insert(as->asString());
            for (const Json& n : as->items()) named.insert(n.asString());
        }
    }
    error->clear();
    return text;
}

// The call's object with one member replaced, added (last) or removed (nullopt)
Json withMember(const Json& call, const std::string& key, std::optional<Json> value) {
    std::vector<std::pair<std::string, Json>> members;
    bool done = false;
    for (const auto& m : call.members()) {
        if (m.first != key) {
            members.push_back(m);
        } else if (value) {
            members.emplace_back(key, *value);
            done = true;
        }
    }
    if (value && !done) members.emplace_back(key, *value);
    return Json::object(std::move(members));
}

}  // namespace

std::vector<FigureCall> figureCalls(std::string_view figureFile, std::string* error) {
    const auto d = open(figureFile, error);
    if (!d) return {};
    std::vector<FigureCall> out;
    for (const Json& c : d->calls) {
        FigureCall f;
        if (const Json* on = c.find("on")) f.on = on->asString();
        f.method = c.find("call")->asString();
        if (const Json* a = c.find("args"))
            for (const Json& x : a->items()) f.arguments.push_back(support::toJsonText(x));
        if (const Json* k = c.find("kwargs"))
            for (const auto& [name, x] : k->members()) f.keywords.emplace_back(name, support::toJsonText(x));
        if (const Json* as = c.find("as")) {
            if (as->isString()) f.as.push_back(as->asString());
            for (const Json& n : as->items()) f.as.push_back(n.asString());
        }
        out.push_back(std::move(f));
    }
    error->clear();
    return out;
}

std::string setKeyword(std::string_view figureFile, std::size_t call, const std::string& name, const std::string& value,
                       const std::vector<std::string>& aliases, std::string* error) {
    auto d = open(figureFile, error);
    if (!d) return std::string();
    if (call >= d->calls.size()) return *error = "the figure file has no call " + std::to_string(call), std::string();
    std::optional<Json> v;
    if (!value.empty() && !(v = parseValue(value, error))) return std::string();
    std::vector<std::pair<std::string, Json>> kwargs;
    bool placed = false;
    if (const Json* k = d->calls[call].find("kwargs")) {
        for (const auto& [key, x] : k->members()) {
            const bool same = key == name || std::find(aliases.begin(), aliases.end(), key) != aliases.end();
            if (!same) kwargs.emplace_back(key, x);
            else if (v && !placed) kwargs.emplace_back(name, *v), placed = true;
        }
    }
    if (v && !placed) kwargs.emplace_back(name, *v);
    auto calls = d->calls;
    calls[call] = withMember(calls[call], "kwargs", kwargs.empty() ? std::nullopt : std::optional<Json>(Json::object(std::move(kwargs))));
    // (the members keep the figure file's order: on, call, args, kwargs, as)
    if (const Json* as = calls[call].find("as")) {
        const Json name_ = *as;
        calls[call] = withMember(withMember(calls[call], "as", std::nullopt), "as", name_);
    }
    return close(*d, calls, error);
}

std::string setArgument(std::string_view figureFile, std::size_t call, std::size_t position, const std::string& value, std::string* error) {
    auto d = open(figureFile, error);
    if (!d) return std::string();
    if (call >= d->calls.size()) return *error = "the figure file has no call " + std::to_string(call), std::string();
    const auto v = parseValue(value, error);
    if (!v) return std::string();
    std::vector<Json> args;
    if (const Json* a = d->calls[call].find("args")) args = a->items();
    if (position > args.size())
        return *error = "call " + std::to_string(call) + " has " + std::to_string(args.size()) + " arguments: argument " + std::to_string(position) +
                        " cannot be set",
               std::string();
    if (position == args.size()) args.push_back(*v);
    else args[position] = *v;
    auto calls = d->calls;
    const bool had = calls[call].find("args") != nullptr;
    calls[call] = withMember(calls[call], "args", Json::array(std::move(args)));
    if (!had) {  // args go after the method's name
        std::vector<std::pair<std::string, Json>> members;
        for (const auto& m : calls[call].members())
            if (m.first != "args") members.push_back(m);
        std::vector<std::pair<std::string, Json>> ordered;
        for (const auto& m : members) {
            ordered.push_back(m);
            if (m.first == "call") ordered.emplace_back("args", *calls[call].find("args"));
        }
        calls[call] = Json::object(std::move(ordered));
    }
    return close(*d, calls, error);
}

std::string appendCall(std::string_view figureFile, const std::string& callText, std::string* error) {
    auto d = open(figureFile, error);
    if (!d) return std::string();
    const auto c = parseValue(callText, error);
    if (!c) return std::string();
    auto calls = d->calls;
    calls.push_back(*c);
    return close(*d, calls, error);
}

std::string removeCall(std::string_view figureFile, std::size_t call, std::string* error) {
    auto d = open(figureFile, error);
    if (!d) return std::string();
    if (call >= d->calls.size()) return *error = "the figure file has no call " + std::to_string(call), std::string();
    auto calls = d->calls;
    calls.erase(calls.begin() + std::ptrdiff_t(call));
    return close(*d, calls, error);
}

std::vector<std::string> axesNames(std::string_view figureFile, const std::string& fontDirectory, std::string* error) {
    std::string why;
    const auto figure = model::parseFigure(figureFile, &why);
    if (!figure) return *error = why, std::vector<std::string>();
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    auto names = engine::axesNames(*figure, options, error);
    if (!error->empty()) return {};
    return names;
}

std::string checkFigure(std::string_view figureFile, const std::string& fontDirectory) {
    std::string why;
    const auto figure = model::parseFigure(figureFile, &why);
    if (!figure) return why;
    engine::DrawOptions options;
    options.fontDirectory = fontDirectory;
    return engine::runCalls(*figure, options);
}

}  // namespace calculate_plot
