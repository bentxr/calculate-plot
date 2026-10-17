#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace calculate_plot::support {

// Writes compact JSON as the oracle tools write traces: doubles spelled as Python's repr (exact),
// non-finite doubles as the strings "nan", "inf" and "-inf".
class JsonWriter {
public:
    JsonWriter& beginObject();
    JsonWriter& endObject();
    JsonWriter& beginArray();
    JsonWriter& endArray();
    JsonWriter& key(std::string_view name);
    JsonWriter& value(double v);
    JsonWriter& value(int v);
    JsonWriter& value(long long v);
    JsonWriter& value(bool v);
    JsonWriter& value(std::string_view s);
    JsonWriter& value(const char* s) { return value(std::string_view(s)); }
    JsonWriter& null();
    JsonWriter& raw(std::string_view json);  // an already written JSON value
    const std::string& text() const { return out_; }

private:
    void separator();
    void writeString(std::string_view s);
    std::string out_;
    std::vector<bool> first_;  // per open container: no element written yet
    bool afterKey_ = false;
};

}  // namespace calculate_plot::support
