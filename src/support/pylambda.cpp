// Derived from NumPy 2.5.3: numpy/_core/src/multiarray/number.c (fast_scalar_power) and
// numpy/_core/src/npymath/npy_math_internal.h.src (npy_deg2rad, npy_rad2deg).
// Copyright (c) 2005-2025, NumPy Developers. All rights reserved.
// Licence: LICENSES/NumPy.txt (BSD-3-Clause). Changes: licensing/changes/numpy.md
#include "support/pylambda.hpp"

#include "support/log10.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <map>

namespace calculate_plot::support {

struct PyLambda::Node {
    enum class Kind { Int, Float, X, Constant, Negate, Plus, Add, Subtract, Multiply, Divide, Power, Function };
    Kind kind;
    long long integer = 0;
    double number = 0;
    std::string name;  // of a function
    std::shared_ptr<const Node> a, b;
};

namespace {

using Node = PyLambda::Node;
using Kind = Node::Kind;

// Python's ints stay exact here as far as a double holds every integer: beyond that they are refused
constexpr long long kLargestInt = 9007199254740992LL;  // 2**53

// The NumPy functions taken, each its float64 loop on the reference machine (the C library's function;
// log10 is FDLIBM's formula, as glibc computes it)
double npDeg2rad(double v) { return v * (M_PI / 180.0); }  // npy_deg2rad
double npRad2deg(double v) { return v * (180.0 / M_PI); }  // npy_rad2deg
double npAbs(double v) { return std::fabs(v); }
using Fn = double (*)(double);
const std::map<std::string, Fn>& functions() {
    static const std::map<std::string, Fn> fns = {
        {"sqrt", [](double v) { return std::sqrt(v); }},     {"exp", [](double v) { return std::exp(v); }},
        {"exp2", [](double v) { return std::exp2(v); }},     {"expm1", [](double v) { return std::expm1(v); }},
        {"log", [](double v) { return std::log(v); }},       {"log2", [](double v) { return std::log2(v); }},
        {"log10", [](double v) { return support::log10(v); }}, {"log1p", [](double v) { return std::log1p(v); }},
        {"cbrt", [](double v) { return std::cbrt(v); }},     {"sin", [](double v) { return std::sin(v); }},
        {"cos", [](double v) { return std::cos(v); }},       {"tan", [](double v) { return std::tan(v); }},
        {"arcsin", [](double v) { return std::asin(v); }},   {"arccos", [](double v) { return std::acos(v); }},
        {"arctan", [](double v) { return std::atan(v); }},   {"sinh", [](double v) { return std::sinh(v); }},
        {"cosh", [](double v) { return std::cosh(v); }},     {"arcsinh", [](double v) { return std::asinh(v); }},
        {"arctanh", [](double v) { return std::atanh(v); }}, {"abs", npAbs},
        {"deg2rad", npDeg2rad},                               {"radians", npDeg2rad},
        {"rad2deg", npRad2deg},                               {"degrees", npRad2deg}};
    return fns;
}

// --- The parser: Python's grammar for this subset ---------------------------------------------

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    std::shared_ptr<const Node> parse(std::string* error, bool* numpy) {
        auto node = sum();
        skip();
        if (node && at_ < s_.size()) fail("unexpected " + std::string(1, s_[at_]) + " at position " + std::to_string(at_ + 1));
        if (!error_.empty()) {
            *error = error_;
            return nullptr;
        }
        if (!usesX_) {
            *error = "the function does not use x";
            return nullptr;
        }
        *numpy = numpy_;
        return node;
    }

private:
    const std::string& s_;
    std::size_t at_ = 0;
    std::string error_;
    bool usesX_ = false, numpy_ = false;

    std::shared_ptr<const Node> fail(const std::string& why) {
        if (error_.empty()) error_ = why;
        return nullptr;
    }
    void skip() {
        while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\t')) ++at_;
    }
    bool take(const char* token) {
        skip();
        const std::size_t n = std::char_traits<char>::length(token);
        if (s_.compare(at_, n, token) != 0) return false;
        // '*' is not '**'
        if (n == 1 && token[0] == '*' && at_ + 1 < s_.size() && s_[at_ + 1] == '*') return false;
        at_ += n;
        return true;
    }
    static std::shared_ptr<const Node> make(Kind k, std::shared_ptr<const Node> a = nullptr, std::shared_ptr<const Node> b = nullptr) {
        auto n = std::make_shared<Node>();
        n->kind = k;
        n->a = std::move(a);
        n->b = std::move(b);
        return n;
    }
    // a_expr: m_expr (("+" | "-") m_expr)*
    std::shared_ptr<const Node> sum() {
        auto left = product();
        while (left) {
            if (take("+")) left = make(Kind::Add, left, product());
            else if (take("-")) left = make(Kind::Subtract, left, product());
            else break;
            if (!left->b) return nullptr;
        }
        return left;
    }
    // m_expr: u_expr (("*" | "/") u_expr)*   (// and % are not taken)
    std::shared_ptr<const Node> product() {
        auto left = unary();
        while (left) {
            skip();
            if (s_.compare(at_, 2, "//") == 0) return fail("// is not supported");
            if (take("*")) left = make(Kind::Multiply, left, unary());
            else if (take("/")) left = make(Kind::Divide, left, unary());
            else break;
            if (!left->b) return nullptr;
        }
        skip();
        if (left && at_ < s_.size() && s_[at_] == '%') return fail("% is not supported");
        return left;
    }
    // u_expr: power | "-" u_expr | "+" u_expr
    std::shared_ptr<const Node> unary() {
        if (take("-")) {
            auto a = unary();
            return a ? make(Kind::Negate, a) : nullptr;
        }
        if (take("+")) {
            auto a = unary();
            return a ? make(Kind::Plus, a) : nullptr;
        }
        return power();
    }
    // power: primary ["**" u_expr]
    std::shared_ptr<const Node> power() {
        auto base = primary();
        if (base && take("**")) {
            auto exponent = unary();
            return exponent ? make(Kind::Power, base, exponent) : nullptr;
        }
        return base;
    }
    std::shared_ptr<const Node> primary() {
        skip();
        if (at_ >= s_.size()) return fail("the expression ends too soon");
        const char c = s_[at_];
        if (c == '(') {
            ++at_;
            auto inner = sum();
            if (!inner) return nullptr;
            if (!take(")")) return fail("a ( is not closed");
            return inner;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return number();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t end = at_;
            while (end < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[end])) || s_[end] == '_')) ++end;
            const std::string name = s_.substr(at_, end - at_);
            at_ = end;
            if (name == "x") {
                usesX_ = true;
                return make(Kind::X);
            }
            if (name != "np") return fail("unknown name " + name + " (the variable is x; NumPy is np)");
            if (!take(".")) return fail("np must be followed by one of its functions or constants");
            skip();
            end = at_;
            while (end < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[end])) || s_[end] == '_')) ++end;
            const std::string member = s_.substr(at_, end - at_);
            at_ = end;
            numpy_ = true;
            if (member == "pi" || member == "e") {
                auto n = std::make_shared<Node>();
                n->kind = Kind::Constant;
                n->number = member == "pi" ? M_PI : M_E;  // (Python floats)
                return n;
            }
            if (!functions().count(member)) return fail("np." + member + " is not supported");
            if (!take("(")) return fail("np." + member + " must be called");
            auto arg = sum();
            if (!arg) return nullptr;
            if (!take(")")) return fail("np." + member + " takes one argument");
            auto n = std::make_shared<Node>();
            n->kind = Kind::Function;
            n->name = member;
            n->a = arg;
            return n;
        }
        return fail("unexpected " + std::string(1, c) + " at position " + std::to_string(at_ + 1));
    }
    // Python's integer and float literals (decimal; no underscores, no imaginary numbers)
    std::shared_ptr<const Node> number() {
        const std::size_t start = at_;
        std::size_t i = at_;
        const auto digits = [&] {
            const std::size_t from = i;
            while (i < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i]))) ++i;
            return i - from;
        };
        const std::size_t whole = digits();
        bool isFloat = false;
        if (i < s_.size() && s_[i] == '.') {
            ++i;
            isFloat = true;
            if (whole == 0 && digits() == 0) return fail("a . is not a number");
            if (whole > 0) digits();
        }
        if (i < s_.size() && (s_[i] == 'e' || s_[i] == 'E')) {
            std::size_t j = i + 1;
            if (j < s_.size() && (s_[j] == '+' || s_[j] == '-')) ++j;
            if (j < s_.size() && std::isdigit(static_cast<unsigned char>(s_[j]))) {
                i = j;
                digits();
                isFloat = true;
            } else {
                return fail("an exponent needs digits");
            }
        }
        if (i < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[i])) || s_[i] == '_' || s_[i] == '.'))
            return fail("this spelling of a number is not supported");
        const std::string text = s_.substr(start, i - start);
        at_ = i;
        auto n = std::make_shared<Node>();
        if (!isFloat) {
            if (text.size() > 1 && text[0] == '0' && text.find_first_not_of('0') != std::string::npos)
                return fail("leading zeros in an integer are not Python");
            if (text.size() > 16 || std::strtoll(text.c_str(), nullptr, 10) > kLargestInt)
                return fail("an integer beyond 2**53 is not supported yet");
            n->kind = Kind::Int;
            n->integer = std::strtoll(text.c_str(), nullptr, 10);
            return n;
        }
        n->kind = Kind::Float;
        n->number = std::strtod(text.c_str(), nullptr);  // (correctly rounded, as Python's float literals)
        if (std::isinf(n->number)) return fail("a number too large for a float is not supported");
        return n;
    }
};

// --- Evaluation --------------------------------------------------------------------------------

struct Value {
    enum class Kind { Int, Float, NpFloat, Array } kind = Kind::Float;
    long long integer = 0;
    double number = 0;
    std::vector<double> array;
    double scalar() const { return kind == Kind::Int ? double(integer) : number; }
    bool isScalar() const { return kind != Kind::Array; }
};

using VK = Value::Kind;

Value floatValue(double v, VK k = VK::Float) {
    Value r;
    r.kind = k;
    r.number = v;
    return r;
}

// float ** float as Python computes it (its special cases first, then the C library's pow)
bool pythonFloatPower(double v, double w, double* out, std::string* error) {
    if (w == 0) return *out = 1.0, true;
    if (std::isnan(v)) return *out = v, true;
    if (std::isnan(w)) return *out = v == 1.0 ? 1.0 : w, true;
    if (std::isinf(v) || std::isinf(w)) return *error = "infinities in a power are not supported yet", false;
    const auto odd = [](double t) { return std::fmod(std::fabs(t), 2.0) == 1.0; };
    if (v == 0.0) {
        if (w < 0.0) return *error = "ZeroDivisionError: zero to a negative power", false;
        return *out = odd(w) ? v : 0.0, true;
    }
    bool negate = false;
    if (v < 0.0) {
        if (w != std::floor(w)) return *error = "a negative number to a fractional power is complex: not supported", false;
        v = -v;
        negate = odd(w);
    }
    if (v == 1.0) return *out = negate ? -1.0 : 1.0, true;
    const double r = std::pow(v, w);
    // (pow's ERANGE on overflow; an underflow is taken as it comes)
    if (std::isinf(r))
        return *error = "OverflowError: (34, 'Numerical result out of range')", false;
    return *out = negate ? -r : r, true;
}

bool intFits(long long v) { return v >= -kLargestInt && v <= kLargestInt; }

bool binary(Kind op, const Value& a, const Value& b, Value* out, std::string* error) {
    // NumPy: an array with anything, elementwise in float64
    if (!a.isScalar() || !b.isScalar()) {
        const std::size_t n = a.isScalar() ? b.array.size() : a.array.size();
        if (!a.isScalar() && !b.isScalar() && a.array.size() != b.array.size()) return *error = "arrays of different sizes", false;
        out->kind = VK::Array;
        out->array.resize(n);
        const auto at = [](const Value& v, std::size_t i) { return v.isScalar() ? v.scalar() : v.array[i]; };
        // fast_scalar_power: an array to the power of a scalar 2, -1 or 0.5 (any of Python's int and float
        // or NumPy's float64) is np.square, np.reciprocal or np.sqrt of it
        int fast = 0;
        if (op == Kind::Power && !a.isScalar() && b.isScalar()) {
            const double e = b.scalar();
            fast = e == 2.0 ? 2 : e == -1.0 ? -1 : e == 0.5 ? 5 : 0;
        }
        for (std::size_t i = 0; i < n; ++i) {
            const double x = at(a, i), y = at(b, i);
            double r = 0;
            switch (op) {
            case Kind::Add: r = x + y; break;
            case Kind::Subtract: r = x - y; break;
            case Kind::Multiply: r = x * y; break;
            case Kind::Divide: r = x / y; break;
            default: r = fast == 2 ? x * x : fast == -1 ? 1.0 / x : fast == 5 ? std::sqrt(x) : std::pow(x, y); break;
            }
            out->array[i] = r;
        }
        return true;
    }
    // np.float64 with a Python number: NumPy's scalar arithmetic (no errors; pow for powers)
    if (a.kind == VK::NpFloat || b.kind == VK::NpFloat) {
        const double x = a.scalar(), y = b.scalar();
        double r = 0;
        switch (op) {
        case Kind::Add: r = x + y; break;
        case Kind::Subtract: r = x - y; break;
        case Kind::Multiply: r = x * y; break;
        case Kind::Divide: r = x / y; break;
        default: r = std::pow(x, y); break;
        }
        *out = floatValue(r, VK::NpFloat);
        return true;
    }
    // Python's ints
    if (a.kind == VK::Int && b.kind == VK::Int) {
        const long long x = a.integer, y = b.integer;
        long long r = 0;
        switch (op) {
        case Kind::Add: r = x + y; break;
        case Kind::Subtract: r = x - y; break;
        case Kind::Multiply:
            if (__builtin_mul_overflow(x, y, &r)) return *error = "an integer beyond 2**53 is not supported yet", false;
            break;
        case Kind::Divide:  // true division: both are exact as doubles, so the quotient is correctly rounded
            if (y == 0) return *error = "ZeroDivisionError: division by zero", false;
            *out = floatValue(double(x) / double(y));
            return true;
        default:
            if (y < 0) {  // a negative exponent: float ** float
                double f = 0;
                if (x == 0) return *error = "ZeroDivisionError: zero to a negative power", false;
                if (!pythonFloatPower(double(x), double(y), &f, error)) return false;
                *out = floatValue(f);
                return true;
            }
            r = 1;
            for (long long k = 0; k < y; ++k) {
                if (__builtin_mul_overflow(r, x, &r) || !intFits(r)) return *error = "an integer beyond 2**53 is not supported yet", false;
                if (r == 0 || r == 1) break;
                if (r == -1) {
                    r = (y % 2) ? -1 : 1;
                    break;
                }
            }
            break;
        }
        if (!intFits(r)) return *error = "an integer beyond 2**53 is not supported yet", false;
        out->kind = VK::Int;
        out->integer = r;
        return true;
    }
    // Python's floats (an int beside one is converted)
    const double x = a.scalar(), y = b.scalar();
    double r = 0;
    switch (op) {
    case Kind::Add: r = x + y; break;
    case Kind::Subtract: r = x - y; break;
    case Kind::Multiply: r = x * y; break;
    case Kind::Divide:
        if (y == 0) return *error = "ZeroDivisionError: float division by zero", false;
        r = x / y;
        break;
    default:
        if (!pythonFloatPower(x, y, &r, error)) return false;
        break;
    }
    *out = floatValue(r);
    return true;
}

bool evaluate(const Node& n, const std::vector<double>& x, Value* out, std::string* error) {
    switch (n.kind) {
    case Kind::Int:
        out->kind = VK::Int;
        out->integer = n.integer;
        return true;
    case Kind::Float:
    case Kind::Constant: *out = floatValue(n.number); return true;
    case Kind::X:
        out->kind = VK::Array;
        out->array = x;
        return true;
    case Kind::Negate:
    case Kind::Plus: {
        Value a;
        if (!evaluate(*n.a, x, &a, error)) return false;
        if (n.kind == Kind::Negate) {
            if (a.kind == VK::Int) a.integer = -a.integer;
            else if (a.kind == VK::Array)
                for (double& v : a.array) v = -v;
            else a.number = -a.number;
        }
        *out = std::move(a);
        return true;
    }
    case Kind::Function: {
        Value a;
        if (!evaluate(*n.a, x, &a, error)) return false;
        const Fn f = functions().at(n.name);
        if (a.isScalar()) {
            *out = floatValue(f(a.scalar()), VK::NpFloat);
        } else {
            for (double& v : a.array) v = f(v);
            *out = std::move(a);
        }
        return true;
    }
    default: {
        Value a, b;
        if (!evaluate(*n.a, x, &a, error) || !evaluate(*n.b, x, &b, error)) return false;
        return binary(n.kind, a, b, out, error);
    }
    }
}

}  // namespace

std::optional<PyLambda> PyLambda::parse(const std::string& text, std::string* error) {
    if (text.find_first_of("\n\r") != std::string::npos) return *error = "the function must be on one line", std::nullopt;
    Parser p(text);
    PyLambda f;
    std::string why;
    f.root_ = p.parse(&why, &f.usesNumpy_);
    if (!f.root_) return *error = why, std::nullopt;
    f.text_ = text;
    return f;
}

bool PyLambda::apply(const std::vector<double>& x, std::vector<double>* out, std::string* error) const {
    Value v;
    if (!evaluate(*root_, x, &v, error)) return false;
    if (v.isScalar()) return *error = "the function does not use x", false;  // (parse refuses those)
    *out = std::move(v.array);
    return true;
}

}  // namespace calculate_plot::support
