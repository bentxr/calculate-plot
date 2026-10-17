#include "model/figure_model.hpp"

#include "support/pylambda.hpp"

#include "axes/dates.hpp"

#include "support/pyformat.hpp"
#include "svg/svg_renderer.h"

#include <cctype>
#include <set>

namespace calculate_plot::model {

using support::Json;

namespace {

bool isIdentifier(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return s.rfind("__", 0) != 0;  // no dunder names: nothing but plain plotting calls
}

// "ax", "axs[1]", "axs[0, 1]", "ax.xaxis", "axs[1].yaxis": a name, optionally indexed by integers, then
// optionally attributes of it.
bool isObjectReference(const std::string& s, std::string& name) {
    const auto end = s.find_first_of("[.");
    name = s.substr(0, end);
    if (!isIdentifier(name)) return false;
    std::size_t at = end;
    if (at != std::string::npos && s[at] == '[') {
        const auto close = s.find(']', at);
        if (close == std::string::npos) return false;
        for (char c : s.substr(at + 1, close - at - 1))
            if (!(std::isdigit(static_cast<unsigned char>(c)) || c == ',' || c == ' ' || c == '-')) return false;
        at = close + 1 == s.size() ? std::string::npos : close + 1;
    }
    while (at != std::string::npos) {
        if (s[at] != '.') return false;
        const auto next = s.find('.', at + 1);
        if (!isIdentifier(s.substr(at + 1, next == std::string::npos ? std::string::npos : next - at - 1))) return false;
        at = next;
    }
    return true;
}

// `known`: the names earlier calls gave their results, which {"$ref": name} may use
bool validValue(const Json& v, const std::set<std::string>& known, std::string* error) {
    if (v.isArray()) {
        for (const Json& item : v.items())
            if (!validValue(item, known, error)) return false;
        return true;
    }
    if (v.isObject()) {
        if (v.members().empty()) {
            if (error) *error = "an object value must be {\"$tuple\": [...]}, {\"$dict\": {...}}, {\"$float\": ...}, {\"$new\": ...} or {\"$ref\": ...}, not {}";
            return false;
        }
        if (v.members().size() == 1 && v.members()[0].first == "$float") {
            const Json& f = v.members()[0].second;
            if (f.isString() && (f.asString() == "nan" || f.asString() == "inf" || f.asString() == "-inf")) return true;
            if (error) *error = "{\"$float\": ...} must be \"nan\", \"inf\" or \"-inf\"";
            return false;
        }
        if (v.members()[0].first == "$new") {
            const Json& cls = v.members()[0].second;
            if (v.members().size() == 1 && cls.isString() && isIdentifier(cls.asString())) return true;
            if (error) *error = "{\"$new\": ...} must name a class pyplot holds, and nothing else";
            return false;
        }
        if (v.members()[0].first == "$ref") {
            const Json& name = v.members()[0].second;
            if (v.members().size() == 1 && name.isString() && isIdentifier(name.asString()) && known.count(name.asString()))
                return true;
            if (error) *error = "{\"$ref\": ...} must name the result of an earlier call, and nothing else";
            return false;
        }
        if (v.members()[0].first == "$datetime") {
            const Json& d = v.members()[0].second;
            axes::dates::DateTime dt;
            if (v.members().size() == 1 && d.isString() && axes::dates::parseIso(d.asString(), &dt)) return true;
            if (error) *error = "{\"$datetime\": ...} must be a date and time as YYYY-MM-DDTHH:MM:SS (a year from 1 to 9999)";
            return false;
        }
        if (v.members()[0].first == "$lambda") {
            const Json& body = v.members()[0].second;
            std::string why = "it must be text";
            if (v.members().size() == 1 && body.isString() && support::PyLambda::parse(body.asString(), &why)) return true;
            if (error) *error = "{\"$lambda\": ...} must be the body of a function of x (" + why + ")";
            return false;
        }
        if (v.members()[0].first == "$dict") {
            const Json& d = v.members()[0].second;
            if (v.members().size() != 1 || !d.isObject()) {
                if (error) *error = "{\"$dict\": ...} must hold one JSON object, and nothing else";
                return false;
            }
            for (const auto& member : d.members())
                if (!validValue(member.second, known, error)) return false;
            return true;
        }
        if (v.members().size() != 1 || v.members()[0].first != "$tuple" || !v.members()[0].second.isArray()) {
            if (error)
                *error = "an object value must be {\"$tuple\": [...]}, {\"$dict\": {...}}, {\"$float\": ...}, {\"$new\": ...}, {\"$ref\": ...}, {\"$datetime\": ...} or {\"$lambda\": ...}, not " +
                         (v.members().empty() ? std::string("{}") : v.members()[0].first);
            return false;
        }
        return validValue(v.members()[0].second, known, error);
    }
    return true;
}

std::string python(const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: return "None";
    case Json::Type::Bool: return v.asBool() ? "True" : "False";
    case Json::Type::Number: {
        const std::string& t = v.numberText();
        if (t.find_first_of(".eE") == std::string::npos) return t;  // a Python int
        return support::pythonRepr(v.asNumber());
    }
    case Json::Type::String: return svg::pythonStrRepr(v.asString());
    case Json::Type::Array: {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + python(v[i]);
        return out + "]";
    }
    case Json::Type::Object: {
        if (v.members()[0].first == "$float") return "float('" + v.members()[0].second.asString() + "')";
        if (v.members()[0].first == "$new") return "plt." + v.members()[0].second.asString() + "()";
        if (v.members()[0].first == "$ref") return v.members()[0].second.asString();
        if (v.members()[0].first == "$lambda") return "lambda x: " + v.members()[0].second.asString();
        if (v.members()[0].first == "$datetime") {  // repr(datetime.datetime): the seconds only when not 0
            axes::dates::DateTime dt;
            axes::dates::parseIso(v.members()[0].second.asString(), &dt);
            std::string out = "datetime.datetime(" + std::to_string(dt.year) + ", " + std::to_string(dt.month) + ", " +
                              std::to_string(dt.day) + ", " + std::to_string(dt.hour) + ", " + std::to_string(dt.minute);
            if (dt.second || dt.microsecond) out += ", " + std::to_string(dt.second);
            if (dt.microsecond) out += ", " + std::to_string(dt.microsecond);
            return out + ")";
        }
        if (v.members()[0].first == "$dict") {
            std::string out = "{";
            bool first = true;
            for (const auto& [key, value] : v.members()[0].second.members()) {
                out += (first ? "" : ", ") + svg::pythonStrRepr(key) + ": " + python(value);
                first = false;
            }
            return out + "}";
        }
        const Json& items = v.members()[0].second;
        std::string out = "(";
        for (std::size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + python(items[i]);
        return out + (items.size() == 1 ? ",)" : ")");
    }
    }
    return "None";
}

}  // namespace

std::optional<Figure> parseFigure(std::string_view json, std::string* error) {
    support::JsonError jsonError;
    const auto doc = support::parseJson(json, &jsonError);
    auto fail = [&](const std::string& message) -> std::optional<Figure> {
        if (error) *error = message;
        return std::nullopt;
    };
    if (!doc) return fail("not valid JSON: " + jsonError.message);
    if (!doc->isObject()) return fail("a figure file is a JSON object");
    const Json* version = doc->find("version");
    if (!version || !version->isNumber() || version->asNumber() != 1) return fail("unsupported figure file version");
    Figure figure;
    if (const Json* style = doc->find("style")) {
        bool names = style->isString() || style->isArray();
        if (style->isArray())
            for (const Json& name : style->items()) names = names && name.isString();
        if (!names) return fail("\"style\" must be a style name or a list of style names");
        figure.style = *style;
    }
    if (const Json* rc = doc->find("rc")) {
        if (!rc->isObject()) return fail("\"rc\" must be an object");
        for (const auto& [key, value] : rc->members()) {
            std::string valueError;
            if (!validValue(value, {}, &valueError)) return fail("rc " + key + ": " + valueError);
            figure.rc.emplace_back(key, value);
        }
    }
    const Json* calls = doc->find("calls");
    if (!calls || !calls->isArray()) return fail("\"calls\" must be a list");
    std::set<std::string> known;
    for (const Json& c : calls->items()) {
        if (!c.isObject()) return fail("each call is an object");
        Call call;
        const Json* method = c.find("call");
        if (!method || !method->isString() || !isIdentifier(method->asString()))
            return fail("a call needs a method name, a plain identifier: " +
                        (method && method->isString() ? method->asString() : std::string("(none)")));
        call.method = method->asString();
        if (const Json* on = c.find("on")) {
            std::string name;
            if (!on->isString() || !isObjectReference(on->asString(), name))
                return fail("\"on\" must name an object, e.g. ax or axs[1]");
            if (!known.count(name)) return fail("unknown object " + on->asString() + " in a call to " + call.method);
            call.on = on->asString();
        }
        if (const Json* args = c.find("args")) {
            if (!args->isArray()) return fail("\"args\" must be a list");
            for (const Json& a : args->items()) {
                std::string valueError;
                if (!validValue(a, known, &valueError)) return fail(call.method + ": " + valueError);
                call.args.push_back(a);
            }
        }
        if (const Json* kwargs = c.find("kwargs")) {
            if (!kwargs->isObject()) return fail("\"kwargs\" must be an object");
            for (const auto& [key, value] : kwargs->members()) {
                if (!isIdentifier(key)) return fail("a keyword argument needs a plain name, not " + key);
                std::string valueError;
                if (!validValue(value, known, &valueError)) return fail(call.method + " " + key + ": " + valueError);
                call.kwargs.emplace_back(key, value);
            }
        }
        if (const Json* as = c.find("as")) {
            std::vector<std::string> names;
            if (as->isString()) names.push_back(as->asString());
            else if (as->isArray())
                for (const Json& n : as->items()) names.push_back(n.isString() ? n.asString() : "");
            for (const auto& n : names) {
                if (!isIdentifier(n)) return fail("\"as\" needs plain names, not " + n);
                known.insert(n);
            }
            call.as = names;
        }
        figure.calls.push_back(std::move(call));
    }
    return figure;
}

// Whether a value holds a {"$datetime": ...} anywhere; or a {"$lambda": ...} that uses NumPy
bool uses(const Json& v, bool datetime) {
    if (v.isArray()) {
        for (const Json& item : v.items())
            if (uses(item, datetime)) return true;
        return false;
    }
    if (!v.isObject()) return false;
    for (const auto& [key, value] : v.members()) {
        if (datetime && key == "$datetime") return true;
        if (!datetime && key == "$lambda" && value.isString()) {
            std::string why;
            const auto f = support::PyLambda::parse(value.asString(), &why);
            if (f && f->usesNumpy()) return true;
        }
        if (uses(value, datetime)) return true;
    }
    return false;
}

std::string exportPython(const Figure& figure, std::string_view version) {
    bool datetimes = false, numpy = false;
    for (const auto& call : figure.calls) {
        for (const auto& a : call.args) datetimes = datetimes || uses(a, true), numpy = numpy || uses(a, false);
        for (const auto& kw : call.kwargs) datetimes = datetimes || uses(kw.second, true), numpy = numpy || uses(kw.second, false);
    }
    std::string out = "# Written by calculate-plot " + std::string(version) +
                      ": this script draws the same figure with Matplotlib 3.11.2.\n" +
                      std::string(datetimes ? "import datetime\n" : "") +
                      "import locale\n\n"
                      "import matplotlib.pyplot as plt\n" + std::string(numpy ? "import numpy as np\n" : "") + "\n"
                      // HarfBuzz takes the language of a text that sets none from LC_CTYPE, the first
                      // time it shapes one; the measuring of a text never uses the text's own language,
                      // so rcParams["text.language"] alone would not do.
                      "# Text is shaped for the language of the locale: this is the one the figure was made with.\n"
                      "try:\n"
                      "    locale.setlocale(locale.LC_CTYPE, \"C.UTF-8\")\n"
                      "except locale.Error:\n"
                      "    locale.setlocale(locale.LC_CTYPE, \"C\")\n\n";
    if (!figure.style.isNull()) out += "plt.style.use(" + python(figure.style) + ")\n";
    if (!figure.rc.empty()) {
        out += "plt.rcParams.update({\n";
        for (const auto& [key, value] : figure.rc) out += "    " + svg::pythonStrRepr(key) + ": " + python(value) + ",\n";
        out += "})\n";
    }
    for (const auto& call : figure.calls) {
        std::string line;
        for (std::size_t i = 0; i < call.as.size(); ++i) line += (i ? ", " : "") + call.as[i];
        if (!line.empty()) line += " = ";
        line += (call.on.empty() ? std::string("plt") : call.on) + "." + call.method + "(";
        bool first = true;
        for (const auto& a : call.args) {
            line += (first ? "" : ", ") + python(a);
            first = false;
        }
        for (const auto& [key, value] : call.kwargs) {
            line += (first ? "" : ", ") + key + "=" + python(value);
            first = false;
        }
        out += line + ")\n";
    }
    out += "\nif __name__ == \"__main__\":\n    plt.show()\n";
    return out;
}

}  // namespace calculate_plot::model
