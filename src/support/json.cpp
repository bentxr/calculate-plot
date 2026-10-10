#include "support/json.hpp"
#include "support/pyformat.hpp"

#include <charconv>
#include <cstdlib>

namespace calculate_plot::support {

namespace {

constexpr int kMaxDepth = 1000;


void appendUtf8(std::string& out, unsigned long cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}

}  // namespace

long long Json::asInteger() const {
    long long value = 0;
    std::from_chars(text_.data(), text_.data() + text_.size(), value);
    return value;
}

const Json* Json::find(std::string_view key) const {
    for (const auto& [name, value] : members_)
        if (name == key) return &value;
    return nullptr;
}

class JsonParser {
public:
    JsonParser(std::string_view text, JsonError* error) : text_(text), error_(error) {}

    std::optional<Json> document() {
        Json value;
        skipSpace();
        if (!parseValue(value, 0)) return std::nullopt;
        skipSpace();
        if (at_ != text_.size()) return fail("unexpected text after the value");
        return value;
    }

private:
    std::nullopt_t fail(const char* message) {
        if (error_ && !failed_) {
            error_->offset = at_;
            error_->message = message;
        }
        failed_ = true;
        return std::nullopt;
    }
    bool failBool(const char* message) {
        fail(message);
        return false;
    }

    bool atEnd() const { return at_ >= text_.size(); }
    char peek() const { return atEnd() ? '\0' : text_[at_]; }

    void skipSpace() {
        while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) ++at_;
    }

    bool literal(std::string_view word) {
        if (text_.substr(at_, word.size()) != word) return failBool("unknown literal");
        at_ += word.size();
        return true;
    }

    bool parseValue(Json& out, int depth) {
        if (depth > kMaxDepth) return failBool("nested too deeply");
        switch (peek()) {
        case 'n':
            out.type_ = Json::Type::Null;
            return literal("null");
        case 't':
            out.type_ = Json::Type::Bool;
            out.bool_ = true;
            return literal("true");
        case 'f':
            out.type_ = Json::Type::Bool;
            out.bool_ = false;
            return literal("false");
        case '"':
            out.type_ = Json::Type::String;
            return parseString(out.text_);
        case '[':
            out.type_ = Json::Type::Array;
            return parseArray(out, depth);
        case '{':
            out.type_ = Json::Type::Object;
            return parseObject(out, depth);
        default:
            if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return parseNumber(out);
            return failBool(atEnd() ? "expected a value, found the end" : "expected a value");
        }
    }

    bool digits() {
        const std::size_t start = at_;
        while (!atEnd() && peek() >= '0' && peek() <= '9') ++at_;
        return at_ > start;
    }

    bool parseNumber(Json& out) {
        const std::size_t start = at_;
        if (peek() == '-') ++at_;
        if (peek() == '0') {
            ++at_;
        } else if (!digits()) {
            return failBool("expected digits");
        }
        if (peek() == '.') {
            ++at_;
            if (!digits()) return failBool("expected digits after the decimal point");
        }
        if (peek() == 'e' || peek() == 'E') {
            ++at_;
            if (peek() == '+' || peek() == '-') ++at_;
            if (!digits()) return failBool("expected exponent digits");
        }
        if (peek() >= '0' && peek() <= '9') return failBool("leading zeros are not allowed");
        out.type_ = Json::Type::Number;
        out.text_ = std::string(text_.substr(start, at_ - start));
        if (!readDouble(out.text_, out.number_) || !(std::abs(out.number_) <= 1.7976931348623157e308))
            return failBool("number out of range");
        return true;
    }

    bool hex4(unsigned long& value) {
        value = 0;
        for (int i = 0; i < 4; ++i, ++at_) {
            const char c = peek();
            value <<= 4;
            if (c >= '0' && c <= '9') value |= unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') value |= unsigned(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= unsigned(c - 'A' + 10);
            else return failBool("expected four hexadecimal digits");
        }
        return true;
    }

    // Copies one raw UTF-8 sequence, refusing malformed, overlong and surrogate encodings.
    bool rawUtf8(std::string& out) {
        const auto lead = static_cast<unsigned char>(peek());
        int extra = 0;
        unsigned long cp = 0;
        if (lead < 0x80) {
            extra = 0, cp = lead;
        } else if ((lead & 0xe0) == 0xc0) {
            extra = 1, cp = lead & 0x1f;
        } else if ((lead & 0xf0) == 0xe0) {
            extra = 2, cp = lead & 0x0f;
        } else if ((lead & 0xf8) == 0xf0) {
            extra = 3, cp = lead & 0x07;
        } else {
            return failBool("invalid UTF-8");
        }
        const std::size_t start = at_++;
        for (int i = 0; i < extra; ++i, ++at_) {
            const auto c = static_cast<unsigned char>(peek());
            if (atEnd() || (c & 0xc0) != 0x80) return failBool("invalid UTF-8");
            cp = (cp << 6) | (c & 0x3f);
        }
        static const unsigned long smallest[] = {0, 0x80, 0x800, 0x10000};
        if (cp < smallest[extra] || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return failBool("invalid UTF-8");
        out.append(text_.substr(start, at_ - start));
        return true;
    }

    bool parseString(std::string& out) {
        ++at_;  // the opening quote
        while (true) {
            if (atEnd()) return failBool("unterminated string");
            const char c = peek();
            if (c == '"') {
                ++at_;
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) return failBool("control character in a string");
            if (c != '\\') {
                if (!rawUtf8(out)) return false;
                continue;
            }
            ++at_;
            const char e = peek();
            ++at_;
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned long cp = 0;
                if (!hex4(cp)) return false;
                if (cp >= 0xdc00 && cp <= 0xdfff) return failBool("unpaired low surrogate");
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    unsigned long low = 0;
                    if (peek() != '\\' || text_.substr(at_ + 1, 1) != "u") return failBool("unpaired high surrogate");
                    at_ += 2;
                    if (!hex4(low)) return false;
                    if (low < 0xdc00 || low > 0xdfff) return failBool("unpaired high surrogate");
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                }
                appendUtf8(out, cp);
                break;
            }
            default:
                --at_;
                return failBool("unknown escape");
            }
        }
    }

    bool parseArray(Json& out, int depth) {
        ++at_;
        skipSpace();
        if (peek() == ']') {
            ++at_;
            return true;
        }
        while (true) {
            skipSpace();
            Json item;
            if (!parseValue(item, depth + 1)) return false;
            out.items_.push_back(std::move(item));
            skipSpace();
            if (peek() == ',') {
                ++at_;
                skipSpace();
                if (peek() == ']') return failBool("trailing comma");
                continue;
            }
            if (peek() == ']') {
                ++at_;
                return true;
            }
            return failBool("expected ',' or ']'");
        }
    }

    bool parseObject(Json& out, int depth) {
        ++at_;
        skipSpace();
        if (peek() == '}') {
            ++at_;
            return true;
        }
        while (true) {
            skipSpace();
            if (peek() != '"') return failBool("expected a member name");
            const std::size_t nameAt = at_;
            std::string name;
            if (!parseString(name)) return false;
            if (out.find(name)) {
                at_ = nameAt;
                return failBool("duplicate member name");
            }
            skipSpace();
            if (peek() != ':') return failBool("expected ':'");
            ++at_;
            skipSpace();
            Json value;
            if (!parseValue(value, depth + 1)) return false;
            out.members_.emplace_back(std::move(name), std::move(value));
            skipSpace();
            if (peek() == ',') {
                ++at_;
                skipSpace();
                if (peek() == '}') return failBool("trailing comma");
                continue;
            }
            if (peek() == '}') {
                ++at_;
                return true;
            }
            return failBool("expected ',' or '}'");
        }
    }

    std::string_view text_;
    JsonError* error_;
    std::size_t at_ = 0;
    bool failed_ = false;
};

std::optional<Json> parseJson(std::string_view text, JsonError* error) {
    return JsonParser(text, error).document();
}

}  // namespace calculate_plot::support
