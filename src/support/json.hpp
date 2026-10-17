#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculate_plot::support {

// A parsed JSON value (RFC 8259). Numbers keep their source text, so integers wider than a double's
// 53 bits and exact decimal spellings survive; objects keep their members in source order.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;  // null
    // Values made in code. A real number is written as Python's repr writes a float ("1.0", "1e-05",
    // "inf", "nan"), so that it stays apart from the integer of the same value.
    static Json boolean(bool value);
    static Json integer(long long value);
    static Json real(double value);
    static Json string(std::string value);
    static Json array(std::vector<Json> items);
    static Json object(std::vector<std::pair<std::string, Json>> members);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    // A number written without a fraction or an exponent: an int to Python, where the others are floats.
    bool isInteger() const { return type_ == Type::Number && text_.find_first_of(".eEn") == std::string::npos; }

    bool asBool() const { return bool_; }
    double asNumber() const { return number_; }
    long long asInteger() const;               // the number's text read as an integer
    const std::string& numberText() const { return text_; }
    const std::string& asString() const { return text_; }

    std::size_t size() const { return items_.size(); }  // array elements
    const Json& operator[](std::size_t i) const { return items_[i]; }
    const std::vector<Json>& items() const { return items_; }
    const std::vector<std::pair<std::string, Json>>& members() const { return members_; }
    const Json* find(std::string_view key) const;  // nullptr if absent or not an object

private:
    friend class JsonParser;
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0;
    std::string text_;
    std::vector<Json> items_;
    std::vector<std::pair<std::string, Json>> members_;
};

struct JsonError {
    std::size_t offset = 0;  // byte offset of the problem
    std::string message;
};

std::optional<Json> parseJson(std::string_view text, JsonError* error);

}  // namespace calculate_plot::support
