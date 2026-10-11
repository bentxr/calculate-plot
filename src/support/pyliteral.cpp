#include "support/pyliteral.hpp"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "support/pyparse.hpp"

namespace calculate_plot::support {

namespace {

using State = PythonLiteral::State;

bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isNameStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isNameChar(char c) { return isNameStart(c) || isDigit(c); }

Json tuple(std::vector<Json> items) { return Json::object({{"$tuple", Json::array(std::move(items))}}); }

Json real(double x) {
    if (std::isfinite(x)) return Json::real(x);
    return Json::object({{"$float", Json::string(std::isnan(x) ? "nan" : x > 0 ? "inf" : "-inf")}});
}

// A value of the expression grammar, and whether it is a number written without a sign (the only
// thing a sign may stand before; parentheses around it change nothing).
struct Node {
    Json value;
    bool plainNumber = false;
    bool isFloat = false;
    double number = 0;
    long long integer = 0;
};

class Reader {
public:
    explicit Reader(const std::string& text) : s_(text) {}

    PythonLiteral read() {
        // (literal_eval strips the spaces and tabs in front; the parser takes those at the end)
        Node node;
        std::vector<Json> items;
        bool trailing = false;
        if (!sequence('\0', &items, &trailing, &node)) return {state_, Json()};
        if (items.empty()) return {State::NotOne, Json()};
        // a comma makes a tuple, also without parentheses
        if (items.size() == 1 && !trailing) return {State::Value, node.value};
        return {State::Value, tuple(std::move(items))};
    }

private:
    bool fail(State state) {
        state_ = state;
        return false;
    }

    // The next character that is not a space or a tab; '\0' at the end (and at a comment, which
    // runs to the end as there are no line breaks). False where the text cannot be read here.
    bool peek(char* c) {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t')) ++i_;
        if (i_ < s_.size() && s_[i_] == '#') {
            // a comment ends with its line, and line breaks are not read here
            if (s_.find_first_of("\n\r", i_) != std::string::npos) return fail(State::Unsupported);
            i_ = s_.size();
        }
        if (i_ >= s_.size()) {
            *c = '\0';
            return true;
        }
        const unsigned char u = static_cast<unsigned char>(s_[i_]);
        // a line break (allowed inside brackets), a line joined with a backslash, and what is not ASCII
        if (u < 0x20 || u == 0x7f || u >= 0x80 || u == '\\') return fail(State::Unsupported);
        *c = s_[i_];
        return true;
    }

    // [expr (',' expr)* [',']] up to `close`, which is left unread. `first` is the first expression.
    bool sequence(char close, std::vector<Json>* items, bool* trailing, Node* first) {
        for (;;) {
            char c;
            if (!peek(&c)) return false;
            if (c == close) return true;
            Node item;
            if (!expression(&item)) return false;
            if (items->empty()) *first = item;
            items->push_back(item.value);
            if (!peek(&c)) return false;
            *trailing = c == ',';
            if (c == ',') {
                ++i_;
                continue;
            }
            if (c == close) return true;
            return fail(State::NotOne);  // two expressions with no comma between, a bracket of the other kind
        }
    }

    bool expression(Node* out) {
        if (++depth_ > 100) return fail(State::Unsupported);
        char c;
        if (!peek(&c)) return false;
        bool ok = false;
        if (c == '+' || c == '-') {
            ++i_;
            Node operand;
            if (!expression(&operand)) return false;
            // _convert_signed_num: one sign, before a number
            if (!operand.plainNumber) return fail(State::NotOne);
            *out = operand;
            out->plainNumber = false;
            if (c == '-') out->value = operand.isFloat ? real(-operand.number) : Json::integer(-operand.integer);
            ok = true;
        } else {
            ok = atom(out);
        }
        --depth_;
        return ok;
    }

    bool atom(Node* out) {
        char c;
        if (!peek(&c)) return false;
        if (c == '(' || c == '[') {
            ++i_;
            Node first;
            std::vector<Json> items;
            bool trailing = false;
            if (!sequence(c == '(' ? ')' : ']', &items, &trailing, &first)) return false;
            ++i_;  // the closing bracket
            if (c == '[') return out->value = Json::array(std::move(items)), true;
            if (items.size() == 1 && !trailing) return *out = first, true;  // an expression in parentheses
            return out->value = tuple(std::move(items)), true;
        }
        if (c == '\'' || c == '"') {
            std::string text;
            for (;;) {
                if (!string(&text)) return false;
                if (!peek(&c)) return false;
                if (c != '\'' && c != '"') break;  // strings side by side are one
            }
            out->value = Json::string(text);
            return true;
        }
        if (isDigit(c) || c == '.') return number(out);
        if (isNameStart(c)) {
            std::size_t j = i_;
            while (j < s_.size() && isNameChar(s_[j])) ++j;
            const std::string name = s_.substr(i_, j - i_);
            // r'...', b'...' and the like
            if (j < s_.size() && (s_[j] == '\'' || s_[j] == '"')) return fail(State::Unsupported);
            if (j < s_.size() && static_cast<unsigned char>(s_[j]) >= 0x80) return fail(State::Unsupported);
            i_ = j;
            if (name == "None") return out->value = Json(), true;
            if (name == "True" || name == "False") return out->value = Json::boolean(name == "True"), true;
            return fail(State::NotOne);  // a name
        }
        return fail(State::NotOne);  // an operator, a brace, a closing bracket out of place, the end
    }

    // A string in single or double quotes, appended to `text`.
    bool string(std::string* text) {
        const char quote = s_[i_];
        if (s_.compare(i_, 3, std::string(3, quote)) == 0) return fail(State::Unsupported);  // triple quotes
        std::size_t j = i_ + 1;
        for (; j < s_.size() && s_[j] != quote; ++j) {
            const unsigned char u = static_cast<unsigned char>(s_[j]);
            if (s_[j] == '\\' || (u < 0x20 && u != '\t') || u == 0x7f) return fail(State::Unsupported);
        }
        if (j >= s_.size()) return fail(State::NotOne);  // no closing quote
        text->append(s_, i_ + 1, j - i_ - 1);
        i_ = j + 1;
        return true;
    }

    // digitpart: digit (["_"] digit)*
    bool digitPart(std::size_t* j) const {
        if (*j >= s_.size() || !isDigit(s_[*j])) return false;
        for (;;) {
            ++*j;
            if (*j < s_.size() && isDigit(s_[*j])) continue;
            if (*j + 1 < s_.size() && s_[*j] == '_' && isDigit(s_[*j + 1])) {
                ++*j;
                continue;
            }
            return true;
        }
    }

    // decinteger | pointfloat | exponentfloat
    bool number(Node* out) {
        std::size_t j = i_;
        const bool whole = digitPart(&j);
        bool isFloat = false;
        if (j < s_.size() && s_[j] == '.') {
            ++j;
            const bool fraction = digitPart(&j);
            if (!whole && !fraction) return fail(State::NotOne);  // a lone dot (or an Ellipsis)
            isFloat = true;
        }
        if (j < s_.size() && (s_[j] == 'e' || s_[j] == 'E')) {
            std::size_t k = j + 1;
            if (k < s_.size() && (s_[k] == '+' || s_[k] == '-')) ++k;
            if (digitPart(&k)) {
                j = k;
                isFloat = true;
            }
        }
        const std::string token = s_.substr(i_, j - i_);
        if (j < s_.size() && (isNameChar(s_[j]) || static_cast<unsigned char>(s_[j]) >= 0x80)) {
            // 0x10, 0o7, 0b1 are integers written otherwise; 1j is not a number we read, and the rest
            // ("1e", "1_", "1x") is no number at all
            const char next = s_[j];
            if (token == "0" && (next == 'x' || next == 'X' || next == 'o' || next == 'O' || next == 'b' || next == 'B'))
                return fail(State::Unsupported);
            if (static_cast<unsigned char>(next) >= 0x80) return fail(State::Unsupported);
            return fail(State::NotOne);
        }
        i_ = j;
        out->plainNumber = true;
        out->isFloat = isFloat;
        if (isFloat) {
            out->number = *pythonFloat(token);
            out->value = real(out->number);
            return true;
        }
        // an integer starts with a digit that is not zero, or is made of zeros only
        std::string digits;
        for (char d : token)
            if (d != '_') digits += d;
        if (digits[0] == '0' && digits.find_first_not_of('0') != std::string::npos) return fail(State::NotOne);
        if (digits.size() > 18) return fail(State::Unsupported);  // (may not fit 64 bits)
        out->integer = *pythonInt(digits);
        out->value = Json::integer(out->integer);
        return true;
    }

    const std::string& s_;
    std::size_t i_ = 0;
    int depth_ = 0;
    State state_ = State::NotOne;
};

}  // namespace

PythonLiteral pythonLiteral(const std::string& text) { return Reader(text).read(); }

}  // namespace calculate_plot::support
