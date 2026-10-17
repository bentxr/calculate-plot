#include "support/json_writer.hpp"

#include "support/json.hpp"
#include "support/pyformat.hpp"

#include <cmath>
#include <cstdio>

namespace calculate_plot::support {

void JsonWriter::separator() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (!first_.empty()) {
        if (!first_.back()) out_ += ',';
        first_.back() = false;
    }
}

JsonWriter& JsonWriter::beginObject() {
    separator();
    out_ += '{';
    first_.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::endObject() {
    out_ += '}';
    first_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::beginArray() {
    separator();
    out_ += '[';
    first_.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::endArray() {
    out_ += ']';
    first_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::key(std::string_view name) {
    separator();
    writeString(name);
    out_ += ':';
    afterKey_ = true;  // the value that follows needs no separator
    return *this;
}

JsonWriter& JsonWriter::value(double v) {
    if (!std::isfinite(v)) return value(std::string_view(std::isnan(v) ? "nan" : (v > 0 ? "inf" : "-inf")));
    separator();
    out_ += pythonRepr(v);
    return *this;
}

JsonWriter& JsonWriter::value(int v) { return value(static_cast<long long>(v)); }

JsonWriter& JsonWriter::value(long long v) {
    separator();
    out_ += std::to_string(v);
    return *this;
}

JsonWriter& JsonWriter::value(bool v) {
    separator();
    out_ += v ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::value(std::string_view s) {
    separator();
    writeString(s);
    return *this;
}

void JsonWriter::writeString(std::string_view s) {
    out_ += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out_ += "\\\""; break;
        case '\\': out_ += "\\\\"; break;
        case '\n': out_ += "\\n"; break;
        case '\r': out_ += "\\r"; break;
        case '\t': out_ += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                out_ += buffer;
            } else {
                out_ += c;
            }
        }
    }
    out_ += '"';
}

JsonWriter& JsonWriter::null() {
    separator();
    out_ += "null";
    return *this;
}

JsonWriter& JsonWriter::raw(std::string_view json) {
    separator();
    out_ += json;
    return *this;
}

void writeJson(JsonWriter& w, const Json& v) {
    switch (v.type()) {
    case Json::Type::Null: w.null(); break;
    case Json::Type::Bool: w.value(v.asBool()); break;
    case Json::Type::Number: w.raw(v.numberText()); break;
    case Json::Type::String: w.value(v.asString()); break;
    case Json::Type::Array:
        w.beginArray();
        for (const Json& x : v.items()) writeJson(w, x);
        w.endArray();
        break;
    case Json::Type::Object:
        w.beginObject();
        for (const auto& [k, x] : v.members()) {
            w.key(k);
            writeJson(w, x);
        }
        w.endObject();
        break;
    }
}

std::string toJsonText(const Json& v) {
    JsonWriter w;
    writeJson(w, v);
    return w.text();
}

}  // namespace calculate_plot::support
