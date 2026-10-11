// Derived from Matplotlib 3.11.2: lib/matplotlib/rcsetup.py (_parse_cycler_string,
// _eval_cycler_expr, cycler, validate_cycler).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "rc/cycler_text.hpp"

#include "rc/cycler.hpp"
#include "support/pyliteral.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace calculate_plot::rc {

namespace {

using support::Json;
using State = Validated::State;

// A cycle longer than this is not made here (a text can ask for any length with a product).
constexpr std::size_t kLongest = 10000;

// --- The words of the text ------------------------------------------------------------------------

struct Token {
    enum class Kind { Name, Number, String, Operator, End } kind = Kind::End;
    std::string text;
    std::size_t begin = 0, end = 0;  // in the source
};

// What reading gives: a cycle or a Python literal; or why not.
struct Result {
    State state = State::Taken;
    bool isCycle = false;
    Cycler cycle;
    Json literal;
    std::string error;
};

Result cycleOf(Cycler c) {
    Result r;
    r.isCycle = true;
    r.cycle = std::move(c);
    return r;
}
Result literalOf(Json v) {
    Result r;
    r.literal = std::move(v);
    return r;
}
Result refusal(std::string why) {
    Result r;
    r.state = State::Refused;
    r.error = std::move(why);
    return r;
}
Result notYet(const std::string& what) {
    Result r;
    r.state = State::Unsupported;
    r.error = what + " is not supported yet";
    return r;
}

bool isTuple(const Json& v) { return v.isObject() && v.members().size() == 1 && v.members()[0].first == "$tuple"; }
bool isList(const Json& v) { return v.isArray(); }
// (a bool is an int to Python)
bool isInt(const Json& v) { return v.isBool() || (v.isNumber() && v.isInteger()); }
long long intOf(const Json& v) { return v.isBool() ? (v.asBool() ? 1 : 0) : v.asInteger(); }
bool isNumber(const Json& v) { return v.isBool() || v.isNumber() || (v.isObject() && v.members().size() == 1 && v.members()[0].first == "$float"); }
const std::vector<Json>& elements(const Json& v) { return isTuple(v) ? v.members()[0].second.items() : v.items(); }
Json sequenceLike(const Json& like, std::vector<Json> items) {
    return isTuple(like) ? Json::object({{"$tuple", Json::array(std::move(items))}}) : Json::array(std::move(items));
}

// The tokens of a Python expression, as far as a cycle needs them. `*problem` is set where Python
// would not read the text at all.
std::vector<Token> tokens(const std::string& s, std::string* problem) {
    std::vector<Token> out;
    int depth = 0;
    bool ended = false;  // a line break after the expression: only blanks and comments may follow
    std::size_t i = 0;
    // blank lines and comment lines first; then the expression starts its line without indentation
    while (i < s.size()) {
        std::size_t j = i;
        while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
        if (j < s.size() && s[j] == '#')
            while (j < s.size() && s[j] != '\n') ++j;
        if (j >= s.size()) return out;  // nothing but blanks and comments
        if (s[j] != '\n') {
            if (j != i) *problem = "unexpected indent";
            break;
        }
        i = j + 1;
    }
    const auto isNameStart = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    while (problem->empty() && i < s.size()) {
        const char c = s[i];
        if (c == ' ' || c == '\t') {
            ++i;
        } else if (c == '\\') {
            if (i + 1 < s.size() && s[i + 1] == '\n') i += 2;  // the line goes on
            else *problem = "unexpected character after line continuation character";
        } else if (c == '#') {
            while (i < s.size() && s[i] != '\n') ++i;
        } else if (c == '\n') {
            if (depth == 0) ended = true;
            ++i;
        } else if (ended) {
            *problem = "invalid syntax";
        } else if (isNameStart(c) || c == '\'' || c == '"') {
            Token t;
            t.begin = i;
            while (i < s.size() && (isNameStart(s[i]) || isDigit(s[i]))) ++i;
            if (i < s.size() && (s[i] == '\'' || s[i] == '"') && i - t.begin <= 2) {
                // a string, with its prefix if it has one
                const char quote = s[i];
                const bool triple = s.compare(i, 3, std::string(3, quote)) == 0;
                i += triple ? 3 : 1;
                bool closed = false;
                while (i < s.size() && !closed) {
                    if (s[i] == '\\') i += 2;
                    else if (triple && s.compare(i, 3, std::string(3, quote)) == 0) i += 3, closed = true;
                    else if (!triple && s[i] == quote) ++i, closed = true;
                    else if (!triple && s[i] == '\n') break;
                    else ++i;
                }
                if (!closed) *problem = "unterminated string literal";
                t.kind = Token::Kind::String;
            } else {
                t.kind = Token::Kind::Name;
            }
            t.end = std::min(i, s.size());
            t.text = s.substr(t.begin, t.end - t.begin);
            out.push_back(t);
        } else if (isDigit(c) || (c == '.' && i + 1 < s.size() && isDigit(s[i + 1]))) {
            Token t;
            t.kind = Token::Kind::Number;
            t.begin = i;
            // (the sign of an exponent is a token of its own: a number is read from its place in the text)
            while (i < s.size() && (isNameStart(s[i]) || isDigit(s[i]) || s[i] == '.')) ++i;
            t.end = i;
            t.text = s.substr(t.begin, t.end - t.begin);
            out.push_back(t);
        } else {
            // (every operator is read as its characters: those of several are not allowed anyway)
            Token t;
            t.kind = Token::Kind::Operator;
            t.begin = i;
            t.text = std::string(1, c);
            if (std::string("()[]{},:+-*/%@&|^~<>=.;!").find(c) == std::string::npos) *problem = "invalid character";
            if (c == '(' || c == '[' || c == '{') ++depth;
            if (c == ')' || c == ']' || c == '}') --depth;
            t.end = ++i;
            out.push_back(t);
        }
    }
    return out;
}

// --- Python's + and * between literals, and slices -------------------------------------------------

Result sliced(const Result& value, const Json& lower, const Json& upper, const Json& step) {
    // slice(lower, upper, step): None or integers
    std::optional<long long> bounds[3];
    const Json* given[3] = {&lower, &upper, &step};
    for (int i = 0; i < 3; ++i) {
        if (given[i]->isNull()) continue;
        if (!isInt(*given[i])) return refusal("slice indices must be integers or None");
        bounds[i] = intOf(*given[i]);
    }
    if (value.isCycle) {
        std::string error;
        const auto made = value.cycle.slice(bounds[0], bounds[1], bounds[2], &error);
        return made ? cycleOf(*made) : refusal(error);
    }
    const Json& v = value.literal;
    if (!v.isString() && !isList(v) && !isTuple(v)) return refusal("the value cannot be sliced");
    const std::size_t length = v.isString() ? v.asString().size() : elements(v).size();
    const auto indices = sliceIndices(length, bounds[0], bounds[1], bounds[2]);
    if (!indices) return refusal("slice step cannot be zero");
    if (v.isString()) {
        std::string out;
        for (std::size_t i : *indices) out += v.asString()[i];
        return literalOf(Json::string(out));
    }
    std::vector<Json> out;
    for (std::size_t i : *indices) out.push_back(elements(v)[i]);
    return literalOf(sequenceLike(v, std::move(out)));
}

Result added(const Result& a, const Result& b) {
    std::string error;
    if (a.isCycle && b.isCycle) {
        const auto sum = Cycler::add(a.cycle, b.cycle, &error);
        return sum ? cycleOf(*sum) : refusal(error);
    }
    if (a.isCycle) {
        // Cycler.__add__ compares the lengths, and then takes only a Cycler as its right-hand side:
        // a list or a string of that length adds nothing
        const Json& v = b.literal;
        if (!v.isString() && !isList(v) && !isTuple(v)) return refusal("a cycler can only be added to a cycler");
        const std::size_t length = v.isString() ? v.asString().size() : elements(v).size();
        if (length != a.cycle.size())
            return refusal("Can only add equal length cycles, not " + std::to_string(a.cycle.size()) + " and " + std::to_string(length));
        return a;
    }
    if (b.isCycle) return refusal("a cycler can only be added to a cycler");
    const Json& x = a.literal;
    const Json& y = b.literal;
    if (x.isString() && y.isString()) return literalOf(Json::string(x.asString() + y.asString()));
    if ((isList(x) && isList(y)) || (isTuple(x) && isTuple(y))) {
        std::vector<Json> out = elements(x);
        out.insert(out.end(), elements(y).begin(), elements(y).end());
        return literalOf(sequenceLike(x, std::move(out)));
    }
    if (isNumber(x) && isNumber(y)) return notYet("arithmetic in a cycler");
    return refusal("these values cannot be added");
}

Result multiplied(const Result& a, const Result& b) {
    std::string error;
    if (a.isCycle && b.isCycle) {
        if (a.cycle.size() * b.cycle.size() > kLongest) return notYet("a cycle of more than 10000 entries");
        const auto product = Cycler::multiply(a.cycle, b.cycle, &error);
        return product ? cycleOf(*product) : refusal(error);
    }
    if (a.isCycle || b.isCycle) {
        // Cycler.__mul__ and __rmul__ with an int
        const Result& cycle = a.isCycle ? a : b;
        const Json& n = (a.isCycle ? b : a).literal;
        if (!isInt(n)) return refusal("a cycler can only be multiplied by a cycler or an integer");
        if (intOf(n) > 0 && cycle.cycle.size() * static_cast<unsigned long long>(intOf(n)) > kLongest)
            return notYet("a cycle of more than 10000 entries");
        const auto repeated = cycle.cycle.times(intOf(n), &error);
        return repeated ? cycleOf(*repeated) : refusal(error);
    }
    const bool sequenceFirst = a.literal.isString() || isList(a.literal) || isTuple(a.literal);
    const Json& sequence = sequenceFirst ? a.literal : b.literal;
    const Json& n = sequenceFirst ? b.literal : a.literal;
    if ((sequence.isString() || isList(sequence) || isTuple(sequence)) && isInt(n) && !n.isBool()) {
        const std::size_t length = sequence.isString() ? sequence.asString().size() : elements(sequence).size();
        const long long times = std::max(0LL, intOf(n));
        if (length * static_cast<unsigned long long>(times) > kLongest) return notYet("a cycle of more than 10000 entries");
        if (sequence.isString()) {
            std::string out;
            for (long long i = 0; i < times; ++i) out += sequence.asString();
            return literalOf(Json::string(out));
        }
        std::vector<Json> out;
        for (long long i = 0; i < times; ++i) out.insert(out.end(), elements(sequence).begin(), elements(sequence).end());
        return literalOf(sequenceLike(sequence, std::move(out)));
    }
    if (isNumber(a.literal) && isNumber(b.literal)) return notYet("arithmetic in a cycler");
    if ((sequence.isString() || isList(sequence) || isTuple(sequence)) && n.isBool()) return notYet("arithmetic in a cycler");
    return refusal("these values cannot be multiplied");
}

// --- The expression --------------------------------------------------------------------------------

class Reader {
public:
    Reader(const std::string& source, std::vector<Token> tokens, const CycleValues& values)
        : source_(source), tokens_(std::move(tokens)), values_(values) {
        tokens_.push_back(Token{});  // End
    }

    // The whole text: one expression.
    Result whole() {
        Result r = sum();
        if (r.state != State::Taken) return r;
        if (peek().kind != Token::Kind::End) return refusal(afterAValue());
        return r;
    }

private:
    const Token& peek(std::size_t ahead = 0) const { return tokens_[std::min(at_ + ahead, tokens_.size() - 1)]; }
    bool isOperator(const char* text, std::size_t ahead = 0) const {
        return peek(ahead).kind == Token::Kind::Operator && peek(ahead).text == text;
    }
    static Result unsupportedAs(const std::string& message) {
        Result r;
        r.state = State::Unsupported;
        r.error = message;
        return r;
    }

    // What stands after a complete value where the expression should end or go on with + or *:
    // another operator (Python reads it, _eval_cycler_expr does not take it) or what Python does not read.
    std::string afterAValue() const {
        const Token& t = peek();
        if (t.kind == Token::Kind::Operator && std::string("-/%@&|^<>").find(t.text[0]) != std::string::npos) return "this operator is not allowed";
        return "this is not an expression that makes a cycler";
    }

    // The text of the tokens [first, last), as it is in the source.
    std::string span(std::size_t first, std::size_t last) const {
        return source_.substr(tokens_[first].begin, tokens_[last - 1].end - tokens_[first].begin);
    }

    // The token after the bracket that closes the one at `open`; tokens_.size() if none does.
    std::size_t afterClosing(std::size_t open) const {
        int depth = 0;
        for (std::size_t i = open; i + 1 < tokens_.size(); ++i) {
            if (tokens_[i].kind != Token::Kind::Operator) continue;
            const char c = tokens_[i].text[0];
            if (c == '(' || c == '[' || c == '{') ++depth;
            if (c == ')' || c == ']' || c == '}') --depth;
            if (depth == 0) return i + 1;
        }
        return tokens_.size();
    }

    // ast.literal_eval of a piece of the text.
    Result literal(std::size_t first, std::size_t last, bool otherLiteralsBreak) {
        const auto read = support::pythonLiteral(span(first, last));
        at_ = last;
        if (read.state == support::PythonLiteral::State::Value) return literalOf(read.value);
        if (read.state == support::PythonLiteral::State::Unsupported) return notYet("this spelling of a Python literal");
        // not a literal of the kinds read here: Python may still read it as one (bytes, a set, 1j)
        return otherLiteralsBreak ? refusal("this is not a value a cycler takes") : notYet("this kind of Python literal");
    }

    Result sum() {
        Result left = product();
        while (left.state == State::Taken && isOperator("+")) {
            ++at_;
            const Result right = product();
            if (right.state != State::Taken) return right;
            left = added(left, right);
        }
        return left;
    }

    Result product() {
        Result left = signedValue();
        while (left.state == State::Taken && isOperator("*")) {
            ++at_;
            const Result right = signedValue();
            if (right.state != State::Taken) return right;
            left = multiplied(left, right);
        }
        return left;
    }

    // A sign belongs to a number (ast.literal_eval takes -1 and +1); before anything else it is an
    // operation that _eval_cycler_expr does not take.
    Result signedValue() {
        if (isOperator("+") || isOperator("-")) {
            if (peek(1).kind == Token::Kind::Number) {
                const std::size_t first = at_;
                return literal(first, first + 2, true);
            }
            if (isOperator("(", 1)) return notYet("a sign before a bracket in a cycler");
            return refusal("this is not a value a cycler takes");
        }
        return postfix();
    }

    Result postfix() {
        Result value = atom();
        while (value.state == State::Taken) {
            if (isOperator("[")) value = subscript(value);
            else if (isOperator("(") || isOperator(".")) return refusal("only the 'cycler()' and 'concat()' functions are allowed");
            else break;
        }
        return value;
    }

    Result atom() {
        const Token& t = peek();
        const std::size_t first = at_;
        if (t.kind == Token::Kind::Name) {
            if (isOperator("(", 1)) return call();
            if (t.text == "None" || t.text == "True" || t.text == "False") return literal(first, first + 1, true);
            ++at_;
            return refusal("this is not a value a cycler takes");  // a name
        }
        if (t.kind == Token::Kind::Number) return literal(first, first + 1, true);
        if (t.kind == Token::Kind::String) {
            // strings side by side are one
            std::size_t last = first;
            while (tokens_[last].kind == Token::Kind::String) ++last;
            return literal(first, last, false);
        }
        if (isOperator("[") || isOperator("{")) {
            const std::size_t last = afterClosing(first);
            if (last == tokens_.size()) return refusal("a bracket is never closed");
            return literal(first, last, false);
        }
        if (isOperator("(")) {
            const std::size_t last = afterClosing(first);
            if (last == tokens_.size()) return refusal("a bracket is never closed");
            // a tuple or a literal in brackets; else an expression in brackets
            const auto read = support::pythonLiteral(span(first, last));
            if (read.state == support::PythonLiteral::State::Value) {
                at_ = last;
                return literalOf(read.value);
            }
            ++at_;
            Result inner = sum();
            if (inner.state != State::Taken) return inner;
            if (!isOperator(")")) return refusal(isOperator(",") ? "this is not a value a cycler takes" : afterAValue());
            ++at_;
            return inner;
        }
        return refusal("this is not an expression that makes a cycler");
    }

    // value[lower:upper:step]
    Result subscript(const Result& value) {
        const std::size_t open = at_, last = afterClosing(open);
        if (last == tokens_.size()) return refusal("a bracket is never closed");
        // the parts between the colons that are not inside brackets
        std::vector<std::pair<std::size_t, std::size_t>> parts;
        std::size_t start = open + 1;
        int depth = 0;
        for (std::size_t i = open + 1; i < last; ++i) {
            const Token& t = tokens_[i];
            const bool op = t.kind == Token::Kind::Operator;
            const char c = op ? t.text[0] : ' ';
            if (op && depth == 0 && (i == last - 1 || t.text == ":")) {
                parts.emplace_back(start, i);
                start = i + 1;
            } else if (op && depth == 0 && t.text == ",") {
                return refusal("only slicing is supported, not indexing");
            }
            if (c == '(' || c == '[' || c == '{') ++depth;
            if (c == ')' || c == ']' || c == '}') --depth;
        }
        if (parts.size() == 1) return refusal("only slicing is supported, not indexing");
        if (parts.size() > 3) return refusal("this is not an expression that makes a cycler");
        Json bounds[3];
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (parts[i].first == parts[i].second) continue;  // nothing: None
            const Result bound = literal(parts[i].first, parts[i].second, true);
            if (bound.state != State::Taken) return bound;
            bounds[i] = bound.literal;
        }
        at_ = last;
        return sliced(value, bounds[0], bounds[1], bounds[2]);
    }

    // cycler(...) or concat(...)
    Result call() {
        const std::string name = peek().text;
        at_ += 2;
        if (name != "cycler" && name != "concat") return refusal("only the 'cycler()' and 'concat()' functions are allowed");
        std::vector<Result> positional;
        std::vector<std::pair<std::string, Result>> keywords;
        while (!isOperator(")")) {
            if (peek().kind == Token::Kind::End) return refusal("a bracket is never closed");
            if (isOperator("*")) return refusal("this is not a value a cycler takes");  // *args, **kwargs
            if (peek().kind == Token::Kind::Name && isOperator("=", 1)) {
                const std::string keyword = peek().text;
                at_ += 2;
                Result value = sum();
                if (value.state != State::Taken) return value;
                // (a keyword given twice: Python's parser takes it, and the later one stays)
                const auto same = std::find_if(keywords.begin(), keywords.end(), [&](const auto& k) { return k.first == keyword; });
                if (same != keywords.end()) same->second = std::move(value);
                else keywords.emplace_back(keyword, std::move(value));
            } else {
                if (!keywords.empty()) return refusal("positional argument follows keyword argument");
                Result value = sum();
                if (value.state != State::Taken) return value;
                positional.push_back(std::move(value));
            }
            if (isOperator(",")) ++at_;
            else if (!isOperator(")")) return refusal(peek().kind == Token::Kind::End ? "a bracket is never closed" : afterAValue());
        }
        ++at_;
        return name == "cycler" ? cycler(positional, keywords) : concat(positional, keywords);
    }

    // rcsetup.cycler(*args, **kwargs)
    Result cycler(const std::vector<Result>& args, const std::vector<std::pair<std::string, Result>>& kwargs) {
        if (!args.empty() && !kwargs.empty()) return refusal("cycler() can only accept positional OR keyword arguments -- not both.");
        if (args.empty() && kwargs.empty()) return refusal("cycler() must have positional OR keyword arguments");
        if (args.size() == 1) {
            if (!args[0].isCycle) return refusal("If only one positional argument given, it must be a Cycler instance.");
            return args[0];  // (validate_cycler: its values were validated when it was made)
        }
        if (args.size() > 2) return refusal("cycler() takes 0-2 arguments");
        std::vector<std::pair<std::string, const Result*>> pairs;
        if (args.size() == 2) {
            if (args[0].isCycle || !args[0].literal.isString()) return refusal("Unknown artist property");
            pairs.emplace_back(args[0].literal.asString(), &args[1]);
        }
        for (const auto& [keyword, value] : kwargs) pairs.emplace_back(keyword, &value);
        std::optional<Cycler> made;
        for (const auto& [given, value] : pairs) {
            static const std::pair<const char*, const char*> aliases[] = {
                {"c", "color"},  {"lw", "linewidth"},        {"ls", "linestyle"},        {"fc", "facecolor"},  {"ec", "edgecolor"},
                {"mfc", "markerfacecolor"}, {"mec", "markeredgecolor"}, {"mew", "markeredgewidth"}, {"ms", "markersize"}};
            static const char* const properties[] = {"color",      "linewidth",       "linestyle",  "facecolor",       "edgecolor",       "joinstyle",
                                                     "capstyle",   "fillstyle",       "markerfacecolor", "markersize", "markeredgewidth",
                                                     "markeredgecolor", "markevery",  "alpha",      "marker",          "hatch",           "dashes"};
            std::string property = given;
            for (const auto& [alias, full] : aliases)
                if (given == alias) property = full;
            if (std::find_if(std::begin(properties), std::end(properties), [&](const char* p) { return property == p; }) == std::end(properties))
                return refusal("Unknown artist property: " + given);
            // (a cycler as the values is gone through like any iterable: its entries are no values)
            if (value->isCycle) return refusal("the values of " + given + " are not valid");
            const Validated list = values_(property, value->literal);
            if (list.state == State::Refused) return refusal(list.error);
            if (list.state == State::Unsupported) return unsupportedAs(list.error);
            const Cycler one = Cycler::of(property, list.value.items());
            if (!made) {
                made = one;
                continue;
            }
            std::string error;
            made = Cycler::add(*made, one, &error);
            if (!made) return refusal(error);
        }
        return cycleOf(*made);
    }

    // cycler.concat(left, right)
    Result concat(const std::vector<Result>& args, const std::vector<std::pair<std::string, Result>>& kwargs) {
        const Result* sides[2] = {args.size() > 0 ? &args[0] : nullptr, args.size() > 1 ? &args[1] : nullptr};
        if (args.size() > 2) return refusal("concat() takes 2 arguments");
        for (const auto& [keyword, value] : kwargs) {
            const int side = keyword == "left" ? 0 : keyword == "right" ? 1 : -1;
            if (side < 0 || sides[side]) return refusal("concat() takes left and right");
            sides[side] = &value;
        }
        if (!sides[0] || !sides[1]) return refusal("concat() takes 2 arguments");
        if (!sides[0]->isCycle || !sides[1]->isCycle) return refusal("concat() takes cyclers");
        if (sides[0]->cycle.size() + sides[1]->cycle.size() > kLongest) return notYet("a cycle of more than 10000 entries");
        std::string error;
        const auto made = Cycler::concat(sides[0]->cycle, sides[1]->cycle, &error);
        return made ? cycleOf(*made) : refusal(error);
    }

    const std::string& source_;
    std::vector<Token> tokens_;
    const CycleValues& values_;
    std::size_t at_ = 0;
};

}  // namespace

Validated cyclerFromText(const std::string& text, const CycleValues& values) {
    const auto failed = [&](State state, const std::string& why) { return Validated{state, Json(), why}; };
    for (const char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if (u >= 0x80) return failed(State::Unsupported, "a cycler with characters outside ASCII is not supported yet");
        if (u < 0x20 && c != '\n' && c != '\t') return failed(State::Unsupported, "a cycler with control characters is not supported yet");
    }
    const std::string notValid = "'" + text + "' is not a valid cycler construction: ";
    std::string problem;
    std::vector<Token> read = tokens(text, &problem);
    if (!problem.empty()) return failed(State::Refused, notValid + problem);
    if (read.empty()) return failed(State::Refused, notValid + "there is nothing");
    Reader reader(text, std::move(read), values);
    const Result made = reader.whole();
    if (made.state == State::Unsupported) return failed(State::Unsupported, made.error);
    if (made.state == State::Refused) return failed(State::Refused, notValid + made.error);
    if (!made.isCycle) return failed(State::Refused, "Object is not a string or Cycler instance: " + text);
    // (validate_cycler validates the values of each property once more: they are those of cycler())
    return Validated{State::Taken, made.cycle.toJson(), std::string()};
}

}  // namespace calculate_plot::rc
