// Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (the box model, the parser and its
// handlers, ship and Output) and lib/matplotlib/mathtext.py (MathTextParser).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/mathtext.hpp"

#include "rc/rc_params.hpp"
#include "text/mathtext_fonts.hpp"
#include "support/fontconfig_pattern.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <variant>

namespace calculate_plot::text {

namespace {

using mathtext::FontConstants;
using mathtext::FontMetrics;
using mathtext::Fonts;
using mathtext::symbolSet;
using mathtext::utf8;

constexpr double kShrinkFactor = 0.7;  // SHRINK_FACTOR
constexpr int kNumSizeLevels = 6;      // NUM_SIZE_LEVELS
const double kInf = std::numeric_limits<double>::infinity();

// Python's max(a, b): the first unless the second is greater.
double pyMax(double a, double b) { return b > a ? b : a; }

std::string utf8(const std::u32string& s) {
    std::string out;
    for (char32_t c : s) out += utf8(c);
    return out;
}

bool isAsciiLetter(char32_t c) { return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z'); }
// pyparsing's white space (a tab too, but the tabs are expanded before the text is parsed)
bool isWhite(char32_t c) { return c == U' ' || c == U'\n' || c == U'\r'; }

// --- Boxes ---------------------------------------------------------------------------------------

// ParserState
struct State {
    Fonts* fontset;
    std::string font, fontClass;
    double fontsize, dpi;
    void setFont(const std::string& name) {
        if (name == "normal" || name == "rm" || name == "it" || name == "bf" || name == "bfit") fontClass = name;
        font = name;
    }
    double underlineThickness() const { return fontset->underlineThickness(fontsize, dpi); }
};

enum class Kind { Box, Char, Hlist, Vlist, Rule, Glue, Kern };

struct Node {
    explicit Node(Kind k) : kind(k) {}
    virtual ~Node() = default;
    Kind kind;
    int size = 0;
    double width = 0, height = 0, depth = 0;  // (a Kern has no height or depth; a Glue none of the three)
    // isinstance(node, Box): Box, Hbox, Vbox, the lists and the rules
    bool isBox() const { return kind == Kind::Box || kind == Kind::Hlist || kind == Kind::Vlist || kind == Kind::Rule; }
    bool isList() const { return kind == Kind::Hlist || kind == Kind::Vlist; }
    virtual void shrink() {
        size += 1;
        if (isBox() && size < kNumSizeLevels) {
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
    }
    virtual double kerning(const Node*) { return 0.0; }  // get_kerning
    virtual bool isCharNode() const { return true; }
};
using NodePtr = std::shared_ptr<Node>;

NodePtr makeBox(double width, double height, double depth) {
    auto b = std::make_shared<Node>(Kind::Box);
    b->width = width, b->height = height, b->depth = depth;
    return b;
}
NodePtr vbox(double height, double depth) { return makeBox(0., height, depth); }
NodePtr hbox(double width) { return makeBox(width, 0., 0.); }

struct KernNode : Node {
    explicit KernNode(double w) : Node(Kind::Kern) { width = w; }
    void shrink() override {
        size += 1;
        if (size < kNumSizeLevels) width *= kShrinkFactor;
    }
};
NodePtr kern(double width) { return std::make_shared<KernNode>(width); }

// _GlueSpec without its width: the two kinds of glue mathtext uses ('ss' and 'fill') have none, so
// a glue takes no room of its own (and shrinking it changes nothing).
struct GlueNode : Node {
    GlueNode(double stretchBy, int stretchOrd, double shrinkBy, int shrinkOrd)
        : Node(Kind::Glue), stretch(stretchBy), stretchOrder(stretchOrd), shrinkage(shrinkBy), shrinkOrder(shrinkOrd) {}
    double stretch;
    int stretchOrder;
    double shrinkage;
    int shrinkOrder;
};
NodePtr glueSs() { return std::make_shared<GlueNode>(1., 1, -1., 1); }   // 'ss'
NodePtr glueFill() { return std::make_shared<GlueNode>(1., 2, 0., 0); }  // 'fill'

// Char, and Accent (a Char whose box is its ink)
struct CharNode : Node {
    CharNode(std::u32string sym, const State& s, bool isAccent)
        : Node(Kind::Char), c(std::move(sym)), fontset(s.fontset), font(s.font), fontClass(s.fontClass),
          fontsize(s.fontsize), dpi(s.dpi), accent(isAccent) {}
    std::u32string c;
    Fonts* fontset;
    std::string font, fontClass;
    double fontsize, dpi;
    bool accent;
    FontMetrics metrics{};
    // _update_metrics
    bool update(std::string* error) {
        const auto info = fontset->info(font, fontClass, c, fontsize, dpi, error);
        if (!info) return false;
        metrics = info->metrics;
        if (accent) {
            width = metrics.xmax - metrics.xmin;
            height = metrics.ymax - metrics.ymin;
            depth = 0;
        } else {
            width = c == U" " ? metrics.advance : metrics.width;
            height = metrics.iceberg;
            depth = -(metrics.iceberg - metrics.height);
        }
        return true;
    }
    void shrink() override {
        size += 1;
        if (size < kNumSizeLevels) {
            fontsize *= kShrinkFactor;
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
        if (accent) {
            std::string ignored;
            update(&ignored);
        }
    }
    double kerning(const Node* next) override {
        const double advance = metrics.advance - width;
        double k = 0.;
        if (next && next->kind == Kind::Char) {
            const auto& n = static_cast<const CharNode&>(*next);
            std::string ignored;
            const auto found = fontset->kern(font, fontClass, c, fontsize, n.font, n.fontClass, n.c, n.fontsize, dpi, &ignored);
            if (found) k = *found;
        }
        return advance + k;
    }
};

struct ListNode : Node {
    ListNode(Kind k, std::vector<NodePtr> elements) : Node(k), children(std::move(elements)) {}
    std::vector<NodePtr> children;
    double shiftAmount = 0, glueSet = 0;
    int glueSign = 0, glueOrder = 0;
    void shrink() override {
        for (auto& child : children) child->shrink();
        Node::shrink();
        if (size < kNumSizeLevels) {
            shiftAmount *= kShrinkFactor;
            glueSet *= kShrinkFactor;
        }
    }
    // _set_glue
    void setGlue(double x, int sign, const double totals[4]) {
        int o = 0;
        for (int i = 3; i >= 0; --i)
            if (totals[i] != 0) {
                o = i;
                break;
            }
        glueOrder = o;
        glueSign = sign;
        if (totals[o] != 0.) glueSet = x / totals[o];
        else glueSign = 0;  // (a root lower than its body: its fill does not shrink)
    }
};

double shiftOf(const Node& p) { return p.isList() ? static_cast<const ListNode&>(p).shiftAmount : 0.; }

struct HlistNode : ListNode {
    explicit HlistNode(std::vector<NodePtr> elements) : ListNode(Kind::Hlist, std::move(elements)) {}
    bool isPhantom = false;
    std::string functionName;  // of \sin and the like
    bool isCharNode() const override {
        for (const auto& c : children)
            if (!c->isCharNode()) return false;
        return true;
    }
    // Insert Kern nodes between Char nodes to set kerning.
    void kern() {
        std::vector<NodePtr> kerned;
        for (std::size_t i = 0; i < children.size(); ++i) {
            kerned.push_back(children[i]);
            const double distance = children[i]->kerning(i + 1 < children.size() ? children[i + 1].get() : nullptr);
            if (distance != 0.) kerned.push_back(std::make_shared<KernNode>(distance));
        }
        children = std::move(kerned);
    }
    void hpack(double w = 0.0, bool exactly = false) {
        double h = 0., d = 0., x = 0.;
        double totalStretch[4] = {0., 0., 0., 0.}, totalShrink[4] = {0., 0., 0., 0.};
        for (const auto& p : children) {
            if (p->kind == Kind::Char) {
                x += p->width;
                h = pyMax(h, p->height);
                d = pyMax(d, p->depth);
            } else if (p->isBox()) {
                x += p->width;
                // (matplotlib leaves out a box of infinite height or depth: a vertical rule, which the
                // parser never sets)
                const double s = shiftOf(*p);
                h = pyMax(h, p->height - s);
                d = pyMax(d, p->depth + s);
            } else if (p->kind == Kind::Glue) {
                const auto& g = static_cast<const GlueNode&>(*p);
                totalStretch[g.stretchOrder] += g.stretch;
                totalShrink[g.shrinkOrder] += g.shrinkage;
            } else if (p->kind == Kind::Kern) {
                x += p->width;
            }
        }
        height = h;
        depth = d;
        if (!exactly) w += x;
        width = w;
        x = w - x;
        if (x == 0.) {
            glueSign = 0;
            glueOrder = 0;
            return;
        }
        if (x > 0.) setGlue(x, 1, totalStretch);
        else setGlue(x, -1, totalShrink);
    }
};
using HlistPtr = std::shared_ptr<HlistNode>;

struct VlistNode : ListNode {
    explicit VlistNode(std::vector<NodePtr> elements) : ListNode(Kind::Vlist, std::move(elements)) {}
    bool isCharNode() const override { return false; }
    void vpack(double h = 0.0, bool exactly = false, double l = kInf) {
        double w = 0., d = 0., x = 0.;
        double totalStretch[4] = {0., 0., 0., 0.}, totalShrink[4] = {0., 0., 0., 0.};
        for (const auto& p : children) {
            if (p->isBox()) {
                x += d + p->height;
                d = p->depth;
                // (matplotlib adds the box's shift: no list set in a vertical one has any)
                if (!std::isinf(p->width)) w = pyMax(w, p->width);
            } else if (p->kind == Kind::Glue) {
                x += d;
                d = 0.;
                const auto& g = static_cast<const GlueNode&>(*p);
                totalStretch[g.stretchOrder] += g.stretch;
                totalShrink[g.shrinkOrder] += g.shrinkage;
            } else if (p->kind == Kind::Kern) {
                x += d + p->width;
                d = 0.;
            }
            // (a Char in a Vlist is matplotlib's "Internal mathtext error"; the parser puts none there)
        }
        width = w;
        if (d > l) {
            x += d - l;
            depth = l;
        } else {
            depth = d;
        }
        if (!exactly) h += x;
        height = h;
        x = h - x;
        if (x == 0) {
            glueSign = 0;
            glueOrder = 0;
            return;
        }
        if (x > 0.) setGlue(x, 1, totalStretch);
        else setGlue(x, -1, totalShrink);
    }
};
using VlistPtr = std::shared_ptr<VlistNode>;

// Hlist(elements, w, m, do_kern)
HlistPtr makeHlist(std::vector<NodePtr> elements, bool doKern = true) {
    auto list = std::make_shared<HlistNode>(std::move(elements));
    if (doKern) list->kern();
    list->hpack();
    return list;
}

// Vlist(elements)
VlistPtr makeVlist(std::vector<NodePtr> elements) {
    auto list = std::make_shared<VlistNode>(std::move(elements));
    list->vpack();
    return list;
}

// HCentered: the elements between two pieces of stretchable glue
HlistPtr hcentered(std::vector<NodePtr> elements) {
    elements.insert(elements.begin(), glueSs());
    elements.push_back(glueSs());
    return makeHlist(std::move(elements), false);
}

// Hrule(state, thickness): as wide as its list, half the thickness above and below
NodePtr hrule(const State& state, std::optional<double> thickness = std::nullopt) {
    const double t = thickness ? *thickness : state.underlineThickness();
    auto r = std::make_shared<Node>(Kind::Rule);
    r->width = kInf, r->height = t * 0.5, r->depth = t * 0.5;
    return r;
}

// --- Parser --------------------------------------------------------------------------------------

// One element of what \left ... \right holds: a node, or "\middle" or a delimiter still to be made.
using Middle = std::variant<NodePtr, std::u32string>;

class Parser {
public:
    Parser(Fonts& fontset, double fontsize, double dpi, const rc::Params& rc) : rc_(rc) {
        states_.push_back(State{&fontset, "default", "rm", fontsize, dpi});
    }

    // Parser.parse: main = non_math (math_string non_math)* StringEnd
    HlistPtr parse(const std::u32string& s) {
        std::vector<NodePtr> parts;
        std::size_t i = 0;
        for (;;) {
            // non_math: (\$ | [^$])*
            std::u32string text;
            while (i < s.size() && s[i] != U'$') {
                if (s[i] == U'\\' && i + 1 < s.size() && s[i + 1] == U'$') {
                    text += U'$';
                    i += 2;
                } else {
                    text += s[i++];
                }
            }
            parts.push_back(nonMath(text));
            if (!ok_) return nullptr;
            if (i >= s.size()) break;
            // math_string: QuotedString('$', '\\'): to the next $ that no backslash escapes, on one line
            std::size_t j = i + 1;
            bool closed = false;
            while (j < s.size()) {
                if (s[j] == U'$') {
                    closed = true;
                    break;
                }
                if (s[j] == U'\n' || s[j] == U'\r') break;
                if (s[j] == U'\\') {
                    if (j + 1 >= s.size() || s[j + 1] == U'\n') break;
                    j += 2;
                } else {
                    ++j;
                }
            }
            if (!closed) return fail("Expected end of text"), nullptr;
            parts.push_back(math(s.substr(i + 1, j - i - 1)));
            if (!ok_) return nullptr;
            i = j + 1;
        }
        return makeHlist(std::move(parts));
    }

    std::string error;

private:
    void fail(const std::string& what) {
        if (ok_) error = what;
        ok_ = false;
    }
    State& state() { return states_.back(); }
    void pushState() { states_.push_back(states_.back()); }
    void popState() { states_.pop_back(); }

    // Char(c, state); nullptr (not yet an error) where matplotlib raises ValueError
    std::shared_ptr<CharNode> makeChar(const std::u32string& c, const State& s, bool accent = false) {
        auto node = std::make_shared<CharNode>(c, s, accent);
        std::string why;
        if (!node->update(&why)) {
            // a font file that cannot be opened is an error whatever the symbol
            if (!s.fontset->failure().empty()) fail(s.fontset->failure());
            return nullptr;
        }
        return node;
    }
    // A Char that must exist (the characters of text and of function names)
    NodePtr mustChar(const std::u32string& c, const State& s, bool accent = false) {
        auto node = makeChar(c, s, accent);
        if (!node) fail("Unknown symbol: " + utf8(c));
        return node;
    }

    NodePtr nonMath(const std::u32string& text) {
        std::vector<NodePtr> symbols;
        for (char32_t c : text) {
            auto ch = mustChar(std::u32string(1, c), state());
            if (!ok_) return nullptr;
            symbols.push_back(ch);
        }
        auto h = makeHlist(std::move(symbols));
        // We're going into math now, so set font to 'normal'
        pushState();
        state().setFont(rc_.text("mathtext.default"));
        return h;
    }

    // math_string: math = OneOrMore(token), the whole string
    NodePtr math(const std::u32string& s) {
        const std::u32string outer = s_;
        const std::size_t outerPos = pos_;
        s_ = s;
        pos_ = 0;
        std::vector<NodePtr> tokens;
        int count = 0;
        while (ok_ && token(tokens)) ++count;
        if (ok_ && count == 0) fail("Expected token");
        skipWhite();
        if (ok_ && pos_ < s_.size()) fail("Expected end of text");
        if (!ok_) return nullptr;
        auto h = makeHlist(std::move(tokens));
        popState();
        s_ = outer;
        pos_ = outerPos;
        return h;
    }

    void skipWhite() {
        while (pos_ < s_.size() && isWhite(s_[pos_])) ++pos_;
    }
    bool startsWith(const std::u32string& what) const { return s_.compare(pos_, what.size(), what) == 0; }
    bool startsWith(const std::string& ascii) const {
        if (pos_ + ascii.size() > s_.size()) return false;
        for (std::size_t i = 0; i < ascii.size(); ++i)
            if (s_[pos_ + i] != char32_t(static_cast<unsigned char>(ascii[i]))) return false;
        return true;
    }
    // A literal after optional white space; the position moves only if it is there
    bool literal(const std::string& ascii) {
        const std::size_t save = pos_;
        skipWhite();
        if (startsWith(ascii)) {
            pos_ += ascii.size();
            return true;
        }
        pos_ = save;
        return false;
    }
    // The letters after a backslash at `at` (a control sequence name), "" if there is none
    std::string csname(std::size_t at) const {
        std::string name;
        if (at >= s_.size() || s_[at] != U'\\') return name;
        for (std::size_t k = at + 1; k < s_.size() && isAsciiLetter(s_[k]); ++k) name += char(s_[k]);
        return name;
    }
    // The first character after `from` that is not a space, 0 if there is none
    char32_t nextNonSpace(std::size_t from) const {
        for (std::size_t k = from; k < s_.size(); ++k)
            if (s_[k] != U' ') return s_[k];
        return 0;
    }
    char32_t prevNonSpace(std::size_t before) const {
        for (std::size_t k = std::min(before, s_.size()); k-- > 0;)
            if (s_[k] != U' ') return s_[k];
        return 0;
    }
    // one_of(names): the longest of the names found here
    template <typename Names>
    std::u32string oneOf(const Names& names) const {
        std::u32string best;
        for (const std::u32string& n : names)
            if (n.size() > best.size() && startsWith(n)) best = n;
        return best;
    }

    // _make_space
    NodePtr makeSpace(double percentage) {
        const auto key = std::make_tuple(state().font, state().fontsize, state().dpi);
        auto it = emWidth_.find(key);
        if (it == emWidth_.end()) {
            std::string why;
            const auto q = state().fontset->quad("normal", state().fontsize, state().dpi, &why);
            if (!q) return fail(why), kern(0.);
            it = emWidth_.emplace(key, *q).first;
        }
        return kern(it->second * percentage);
    }
    NodePtr thinSpace() { return makeSpace(mathtext::spaceWidths().at("\\,")); }

    // float_literal: [-+]?([0-9]+\.?[0-9]*|\.[0-9]+)
    bool floatLiteral(double* value) {
        const std::size_t save = pos_;
        skipWhite();
        std::string text;
        std::size_t k = pos_;
        if (k < s_.size() && (s_[k] == U'-' || s_[k] == U'+')) text += char(s_[k++]);
        const auto digits = [&] {
            std::size_t n = 0;
            while (k < s_.size() && s_[k] >= U'0' && s_[k] <= U'9') text += char(s_[k++]), ++n;
            return n;
        };
        bool ok = false;
        if (digits() > 0) {
            ok = true;
            if (k < s_.size() && s_[k] == U'.') {
                text += char(s_[k++]);
                digits();
            }
        } else if (k < s_.size() && s_[k] == U'.') {
            text += char(s_[k++]);
            ok = digits() > 0;
        }
        if (!ok) {
            pos_ = save;
            return false;
        }
        pos_ = k;
        *value = std::strtod(text.c_str(), nullptr);
        return true;
    }

    // token: simple | auto_delim | unclosed_group | unknown_symbol. False when no token starts here
    // (the caller's list ends) or after an error.
    bool token(std::vector<NodePtr>& out) {
        if (simple(out)) return true;
        if (!ok_) return false;
        const std::size_t save = pos_;
        skipWhite();
        if (startsWith("\\left")) {
            NodePtr delimited = autoDelim();
            if (!ok_) return false;
            out.push_back(delimited);
            return true;
        }
        // unknown_symbol is an error (unclosed_group was one in the group); anything else ends the
        // list of tokens
        if (!csname(pos_).empty()) return fail("Unknown symbol: \\" + csname(pos_)), false;
        pos_ = save;
        return false;
    }

    // simple: space | customspace | font | subsuper
    bool simple(std::vector<NodePtr>& out) {
        const std::size_t save = pos_;
        skipWhite();
        for (const auto& [name, width] : mathtext::spaceWidths())
            if (startsWith(name)) {
                pos_ += name.size();
                out.push_back(makeSpace(width));
                return ok_;
            }
        if (startsWith("\\hspace")) {
            pos_ += 7;
            double width = 0;
            if (!(literal("{") && floatLiteral(&width) && literal("}"))) return fail("Expected \\hspace{space}"), false;
            out.push_back(makeSpace(width));
            return ok_;
        }
        if (const std::string name = csname(pos_); symbolSet("fontnames").count(codepoints(name))) {
            pos_ += 1 + name.size();
            state().setFont(name);
            return true;
        }
        if (subsuper(out)) return true;
        if (ok_) pos_ = save;
        return false;
    }

    // tokens up to where no token starts: ZeroOrMore(token)
    std::vector<NodePtr> tokens() {
        std::vector<NodePtr> out;
        while (ok_ && token(out)) {}
        return out;
    }

    // "{" token* "}" as one Hlist (optional_group, and required_group when `required`); nullptr with
    // the position unchanged if it is not there.
    HlistPtr braced(bool required) {
        const std::size_t save = pos_;
        if (!literal("{")) return nullptr;
        std::vector<NodePtr> out;
        int count = 0;
        while (ok_ && token(out)) ++count;
        if (!ok_) return nullptr;
        if ((required && count == 0) || !literal("}")) {
            pos_ = save;
            return nullptr;
        }
        return makeHlist(std::move(out));
    }

    // placeable: the alternatives in matplotlib's order. The symbol does not skip white space (a
    // space is a symbol); the others do.
    bool placeable(NodePtr* out) {
        const std::size_t start = pos_;
        skipWhite();
        const std::size_t at = pos_;
        const std::string name = csname(at);
        const auto command = [&](const char* cs) { return startsWith(std::string(cs)); };
        // phantom | llap | rlap
        for (const char* cs : {"\\phantom", "\\llap", "\\rlap"})
            if (command(cs)) {
                pos_ += std::char_traits<char>::length(cs);
                HlistPtr value = braced(false);
                if (!ok_) return false;
                if (!value) return fail(std::string("Expected ") + cs + "{value}"), false;
                if (cs[1] == 'p') {
                    value->isPhantom = true;
                    *out = value;
                } else if (cs[1] == 'l') {
                    *out = makeHlist({kern(-value->width), value});
                } else {
                    *out = makeHlist({value, kern(-value->width)});
                }
                return true;
            }
        // accent: before the symbol, as all accents are symbols
        if (at < s_.size() && s_[at] == U'\\') {
            std::string accent;
            if (!name.empty() && (mathtext::accentMap().count(name) || symbolSet("wide_accents").count(codepoints(name)))) {
                accent = name;
            } else if (name.empty() && at + 1 < s_.size() && std::u32string(U"\"`'~.^").find(s_[at + 1]) != std::u32string::npos) {
                accent = std::string(1, char(s_[at + 1]));
            }
            if (!accent.empty()) {
                pos_ = at + 1 + accent.size();
                // named_placeable took its whitespace setting from the placeable while that was still
                // empty: unlike the placeable after ^ or _, it skips whitespace.
                skipWhite();
                NodePtr sym;
                if (!placeable(&sym)) return fail("Expected a placeable after the accent"), false;
                *out = makeAccent(accent, sym);
                return ok_;
            }
        }
        // symbol (leave_whitespace)
        pos_ = start;
        if (symbolHere(out)) return true;
        if (!ok_) return false;
        pos_ = at;
        // function
        if (!name.empty() && symbolSet("function_names").count(codepoints(name))) {
            pos_ = at + 1 + name.size();
            *out = operatorName(at, name, nullptr);
            return ok_;
        }
        if (command("\\operatorname")) {
            pos_ += 13;
            std::vector<NodePtr> parts;
            if (!bracedSimple(&parts)) return fail("Expected \\operatorname{name}"), false;
            *out = operatorName(at, std::string(), &parts);
            return ok_;
        }
        // group: start_group token* end_group
        if (NodePtr g = group()) {
            *out = g;
            return true;
        }
        if (!ok_) return false;
        pos_ = at;
        if (command("\\frac") || command("\\dfrac") || command("\\binom")) {
            const bool display = s_[at + 1] == U'd', binom = s_[at + 1] == U'b';
            pos_ += display || binom ? 6 : 5;
            HlistPtr num = braced(true);
            HlistPtr den = num ? braced(true) : nullptr;
            if (!ok_) return false;
            if (!den) return fail(std::string("Expected \\") + (display ? "dfrac" : binom ? "binom" : "frac") + "{num}{den}"), false;
            const std::optional<double> rule = binom ? 0. : state().underlineThickness();
            *out = genfrac(binom ? U"(" : U"", binom ? U")" : U"", rule, display ? 0 : 1, num, den);
            return ok_;
        }
        if (command("\\genfrac")) {
            pos_ += 8;
            *out = parseGenfrac();
            return ok_;
        }
        if (command("\\overset") || command("\\underset")) {
            const bool under = s_[at + 1] == U'u';
            pos_ += under ? 9 : 8;
            HlistPtr annotation = braced(false);
            HlistPtr body = annotation ? braced(false) : nullptr;
            if (!ok_) return false;
            if (!body) return fail(std::string("Expected \\") + (under ? "underset" : "overset") + "{annotation}{body}"), false;
            *out = genset(under, annotation, body);
            return ok_;
        }
        if (command("\\sqrt")) {
            pos_ += 5;
            *out = parseSqrt();
            return ok_;
        }
        if (command("\\overline") || command("\\underline")) {
            const bool under = s_[at + 1] == U'u';
            pos_ += under ? 10 : 9;
            HlistPtr body = braced(true);
            if (!ok_) return false;
            if (!body) return fail(std::string("Expected \\") + (under ? "underline" : "overline") + "{body}"), false;
            *out = under ? underline(body) : overline(body);
            return ok_;
        }
        if (command("\\text")) {
            pos_ += 5;
            std::u32string text;
            if (!bracedText(&text)) return fail("Expected \\text"), false;
            pushState();
            state().setFont("rm");
            std::vector<NodePtr> chars;
            for (char32_t c : text) {
                chars.push_back(mustChar(std::u32string(1, c), state()));
                if (!ok_) return false;
            }
            *out = makeHlist(std::move(chars));
            popState();
            return true;
        }
        if (command("\\boldsymbol")) {
            pos_ += 11;
            std::vector<NodePtr> parts;
            if (!bracedSimple(&parts)) return fail("Expected \\boldsymbol{value}"), false;
            *out = boldsymbol(parts);
            return ok_;
        }
        if (command("\\substack")) {
            pos_ += 9;
            *out = parseSubstack();
            return ok_;
        }
        if (command("\\left")) {
            *out = autoDelim();
            return ok_;
        }
        pos_ = start;
        return false;
    }

    // "{" simple* "}" (the argument of \operatorname and \boldsymbol)
    bool bracedSimple(std::vector<NodePtr>* parts) {
        if (!literal("{")) return false;
        while (ok_ && simple(*parts)) {}
        if (!ok_) return false;
        return literal("}");
    }

    // _BracedText: a brace-delimited literal string; nested braces group and are not kept, a
    // backslash escapes the next character.
    bool bracedText(std::u32string* text) {
        skipWhite();
        if (pos_ >= s_.size() || s_[pos_] != U'{') return false;
        std::size_t loc = pos_;
        int depth = 0;
        while (loc < s_.size()) {
            const char32_t ch = s_[loc];
            if (ch == U'\\' && loc + 1 < s_.size()) {
                const char32_t escaped = s_[loc + 1];
                *text += escaped == U't' ? U'\t' : escaped == U'n' ? U'\n' : escaped == U'f' ? U'\f' : escaped == U'r' ? U'\r' : escaped;
                loc += 2;
                continue;
            }
            loc += 1;
            if (ch == U'{') {
                depth += 1;
                continue;
            }
            if (ch == U'}') {
                depth -= 1;
                if (depth == 0) {
                    pos_ = loc;
                    return true;
                }
                continue;
            }
            *text += ch;
        }
        return false;
    }

    // group: start_group (an optional \mathXX, then "{") token* "}"
    NodePtr group() {
        const std::size_t save = pos_;
        skipWhite();
        std::string font;
        if (startsWith("\\math")) {
            const std::size_t before = pos_;
            pos_ += 5;
            skipWhite();
            const std::u32string name = oneOf(symbolSet("fontnames"));
            if (name.empty()) {
                pos_ = before;  // Optional(\math font) is not there
            } else {
                pos_ += name.size();
                font = utf8(name);
            }
        }
        if (!literal("{")) {
            pos_ = save;
            return nullptr;
        }
        pushState();
        if (!font.empty()) state().setFont(font);
        std::vector<NodePtr> inside = tokens();
        if (!ok_) return nullptr;
        if (!literal("}")) {
            // unclosed_group: the tokens run to the end of the text. Otherwise no group is here (and
            // nothing else can be: the state pushed stays, as in matplotlib, and is never looked at).
            skipWhite();
            if (pos_ >= s_.size()) return fail("Expected '}'"), nullptr;
            pos_ = save;
            return nullptr;
        }
        popState();
        return makeHlist(std::move(inside));
    }

    // symbol: one character of the class, an escaped character, or a TeX name (not followed by a letter)
    bool symbolHere(NodePtr* out) {
        if (pos_ >= s_.size()) return false;
        const char32_t c = s_[pos_];
        std::u32string sym;
        static const std::u32string single = U"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 +-*/<>=:,.;!?&'@()[]|";
        if (single.find(c) != std::u32string::npos || (c >= 0x80 && c <= 0x1ffff)) {
            sym = std::u32string(1, c);
        } else if (c == U'\\' && pos_ + 1 < s_.size()) {
            const char32_t n = s_[pos_ + 1];
            const std::string name = csname(pos_);
            if (std::u32string(U"%${}[]_|").find(n) != std::u32string::npos) sym = std::u32string{c, n};
            else if (!name.empty() && mathtext::isTexName(name)) sym = s_.substr(pos_, 1 + name.size());
            else if (n == U'#' && !(pos_ + 2 < s_.size() && isAsciiLetter(s_[pos_ + 2]))) sym = U"\\#";
        }
        if (sym.empty()) return false;
        const std::size_t loc = pos_;
        pos_ += sym.size();
        *out = symbol(sym, loc);
        return ok_;
    }

    // Parser.symbol: the character, with the space its kind takes
    NodePtr symbol(std::u32string c, std::size_t loc) {
        if (c == U"-") c = U"−";
        NodePtr ch = makeChar(c, state());
        if (!ch) return fail("Unknown symbol: " + utf8(c)), nullptr;
        const char32_t prev = prevNonSpace(loc);
        const std::u32string prevChar = prev ? std::u32string(1, prev) : std::u32string();
        const bool binary = symbolSet("binary_operators").count(c) != 0;
        if (binary || symbolSet("relation_symbols").count(c) || symbolSet("arrow_symbols").count(c)) {
            // Binary operators at start of string should not be spaced; also, operators in sub- or
            // superscripts should not be spaced, nor those after an opening delimiter or a relation.
            bool nothingBefore = true;  // len(s[:loc].split()) == 0
            for (std::size_t k = 0; k < loc && k < s_.size(); ++k)
                if (!isWhite(s_[k]) && s_[k] != U'\f' && s_[k] != U'\v') nothingBefore = false;
            const bool afterOpening = prevChar == U"{" || symbolSet("left_delims").count(prevChar) ||
                                      symbolSet("relation_symbols").count(prevChar);
            if (needsSpaceAfterSubsuper_ || (binary && (nothingBefore || (prev && afterOpening)))) return ch;
            return makeHlist({makeSpace(0.2), ch, makeSpace(0.2)});
        }
        if (symbolSet("punctuation_symbols").count(c)) {
            const char32_t next = nextNonSpace(loc + 1);
            // Do not space commas between brackets
            if (c == U"," && prev == U'{' && next == U'}') return ch;
            // Do not space dots as decimal separators
            if (c == U"." && prev && mathtext::isDigit(unsigned(prev)) && next && mathtext::isDigit(unsigned(next))) return ch;
            return makeHlist({ch, makeSpace(0.2)});
        }
        return ch;
    }

    // subsuper: [placeable] (('_' | '^') placeable)+ '* | '+ | placeable '*
    bool subsuper(std::vector<NodePtr>& out) {
        const std::size_t loc = pos_;  // (white space is skipped already)
        NodePtr nucleus;
        const bool haveNucleus = placeable(&nucleus);
        if (!ok_) return false;
        std::vector<std::pair<char32_t, NodePtr>> scripts;
        for (;;) {
            const std::size_t save = pos_;
            skipWhite();
            if (pos_ >= s_.size() || (s_[pos_] != U'_' && s_[pos_] != U'^')) {
                pos_ = save;
                break;
            }
            const char32_t op = s_[pos_++];
            NodePtr arg;
            if (!placeable(&arg)) return fail("Expected a placeable after the script"), false;
            scripts.emplace_back(op, arg);
        }
        const auto apostrophesHere = [&] {
            const std::size_t save = pos_;
            skipWhite();
            std::size_t n = 0;
            while (pos_ < s_.size() && s_[pos_] == U'\'') ++pos_, ++n;
            if (n == 0) pos_ = save;
            return n;
        };
        std::size_t napostrophes = 0;
        if (!scripts.empty()) {
            napostrophes = apostrophesHere();
        } else if (loc < s_.size() && s_[loc] == U'\'') {
            // apostrophes with nothing before them (the first alternative took one as its nucleus
            // and found no script)
            pos_ = loc;
            napostrophes = apostrophesHere();
            nucleus = nullptr;
        } else if (haveNucleus) {
            napostrophes = apostrophesHere();
        } else {
            return false;
        }
        NodePtr result = subsuperLayout(loc, nucleus, scripts, napostrophes);
        if (!ok_ || !result) return false;
        out.push_back(result);
        return true;
    }

    bool isOverunder(const Node& nucleus) const {
        if (nucleus.kind == Kind::Char) return symbolSet("overunder_symbols").count(static_cast<const CharNode&>(nucleus).c) != 0;
        if (nucleus.kind == Kind::Hlist) {
            const std::string& f = static_cast<const HlistNode&>(nucleus).functionName;
            return !f.empty() && symbolSet("overunder_functions").count(codepoints(f)) != 0;
        }
        return false;
    }
    bool isDropsub(const Node& n) const {
        return n.kind == Kind::Char && symbolSet("dropsub_symbols").count(static_cast<const CharNode&>(n).c) != 0;
    }
    bool isSlanted(const Node& n) const { return n.kind == Kind::Char && static_cast<const CharNode&>(n).metrics.slanted; }

    // Parser.subsuper
    NodePtr subsuperLayout(std::size_t loc, NodePtr nucleus, const std::vector<std::pair<char32_t, NodePtr>>& scripts,
                           std::size_t napostrophes) {
        if (!nucleus) nucleus = hbox(0);
        if (scripts.empty() && !napostrophes) return nucleus;
        NodePtr sub, super;
        for (const auto& [op, arg] : scripts) {
            if (op == U'_') {
                if (sub) return fail("Double subscript"), nullptr;
                sub = arg;
            } else {
                if (super) return fail("Double superscript"), nullptr;
                super = arg;
            }
        }
        State& st = state();
        std::string why;
        const double ruleThickness = st.underlineThickness();
        const auto xh = st.fontset->xheight(st.font, st.fontsize, st.dpi, &why);
        if (!xh) return fail(why), nullptr;
        const double xHeight = *xh;
        if (napostrophes) {
            HlistPtr primes;
            if (!super) primes = makeHlist({});
            else if (super->kind != Kind::Hlist) primes = makeHlist({super});
            else primes = std::static_pointer_cast<HlistNode>(super);
            for (std::size_t i = 0; i < napostrophes; ++i) {
                NodePtr prime = symbol(U"\\prime", loc);
                if (!ok_) return nullptr;
                primes->children.push_back(prime);
            }
            // kern() and hpack() needed to get the metrics right after extending
            primes->kern();
            primes->hpack();
            super = primes;
        }
        // Handle over/under symbols, such as sum or prod
        if (isOverunder(*nucleus)) {
            std::vector<NodePtr> vlist;
            double shift = 0.;
            double width = nucleus->width;
            if (super) {
                super->shrink();
                width = pyMax(width, super->width);
            }
            if (sub) {
                sub->shrink();
                width = pyMax(width, sub->width);
            }
            const double vgap = ruleThickness * 3.0;
            if (super) {
                auto h = hcentered({super});
                h->hpack(width, true);
                vlist.push_back(h);
                vlist.push_back(vbox(0, vgap));
            }
            auto h = hcentered({nucleus});
            h->hpack(width, true);
            vlist.push_back(h);
            if (sub) {
                auto hs = hcentered({sub});
                hs->hpack(width, true);
                vlist.push_back(vbox(0, vgap));
                vlist.push_back(hs);
                shift = hs->height + vgap + nucleus->depth;
            }
            auto vlt = makeVlist(std::move(vlist));
            vlt->shiftAmount = shift;
            std::vector<NodePtr> parts{vlt};
            if (needsSpaceAfterSubsuper_) parts.push_back(thinSpace());
            needsSpaceAfterSubsuper_ = false;
            return makeHlist(std::move(parts));
        }
        Node* lastChar = nucleus.get();
        HlistPtr nucleusList;
        if (nucleus->kind == Kind::Hlist) {
            auto children = static_cast<HlistNode&>(*nucleus).children;
            if (!children.empty()) {
                // remove kern after a symbol before a sub/superscript
                if (children.back()->kind == Kind::Kern) {
                    // new_children[-2] of a list of one: an IndexError, which pyparsing takes for a
                    // failed match once the action has run before; so nothing here matches
                    if (children.size() < 2) return nullptr;
                    if (children[children.size() - 2]->kind == Kind::Char) children.pop_back();
                }
                lastChar = children.back().get();
                if (lastChar->kind == Kind::Char) lastChar->width = static_cast<CharNode*>(lastChar)->metrics.advance;
            }
            nucleusList = makeHlist(children, false);
        } else {
            if (nucleus->kind == Kind::Char) lastChar->width = static_cast<CharNode*>(lastChar)->metrics.advance;
            nucleusList = makeHlist({nucleus});
        }
        const FontConstants& consts = st.fontset->constants();
        const double lcHeight = lastChar->height;
        double lcBaseline = 0.0;
        if (isDropsub(*lastChar)) lcBaseline = lastChar->depth;
        // Compute kerning for sub and super
        double superkern = consts.delta * xHeight;
        double subkern = consts.delta * xHeight;
        if (isSlanted(*lastChar)) {
            superkern += consts.delta * xHeight;
            superkern += consts.deltaSlanted * (lcHeight - xHeight * 2 / 3);
            if (isDropsub(*lastChar)) {
                subkern = (3 * consts.delta - consts.deltaIntegral) * lcHeight;
                superkern = (3 * consts.delta + consts.deltaIntegral) * lcHeight;
            } else {
                subkern = 0;
            }
        }
        double shiftUp = 0.0, shiftDown = 0.0;
        if (!nucleusList->isCharNode()) {
            const auto shrunk = st.fontset->xheight(st.font, st.fontsize * kShrinkFactor, st.dpi, &why);
            if (!shrunk) return fail(why), nullptr;
            shiftUp = nucleusList->height - consts.supdrop * *shrunk;
            shiftDown = nucleusList->depth + consts.subdrop * *shrunk;
        }
        std::shared_ptr<ListNode> x;
        if (!super) {
            // subscript only (sub is there: there is a script and no superscript)
            auto h = makeHlist({kern(subkern), sub});
            h->shrink();
            if (isDropsub(*lastChar)) shiftDown = lcBaseline + consts.subdrop * xHeight;
            else shiftDown = pyMax(pyMax(shiftDown, consts.sub1 * xHeight), h->height - xHeight * 4 / 5);
            h->shiftAmount = shiftDown;
            x = h;
        } else {
            auto h = makeHlist({kern(superkern), super});
            h->shrink();
            if (isDropsub(*lastChar)) shiftUp = lcHeight - consts.subdrop * xHeight;
            else shiftUp = pyMax(pyMax(shiftUp, consts.sup1 * xHeight), h->depth + xHeight / 4);
            if (!sub) {
                h->shiftAmount = -shiftUp;
                x = h;
            } else {
                auto y = makeHlist({kern(subkern), sub});
                y->shrink();
                if (isDropsub(*lastChar)) shiftDown = lcBaseline + consts.subdrop * xHeight;
                else shiftDown = pyMax(shiftDown, consts.sub2 * xHeight);
                // If sub and superscript collide, move super up
                double clr = (4 * ruleThickness - ((shiftUp - h->depth) - (y->height - shiftDown)));
                if (clr > 0.) {
                    shiftDown += clr;
                    clr = xHeight * 4 / 5 - shiftUp + h->depth;
                    if (clr > 0) {
                        shiftUp += clr;
                        shiftDown -= clr;
                    }
                }
                auto v = makeVlist({h, kern((shiftUp - h->depth) - (y->height - shiftDown)), y});
                v->shiftAmount = shiftDown;
                x = v;
            }
        }
        if (!isDropsub(*lastChar)) x->width += consts.scriptSpace * xHeight;
        // Do we need to add a space after the nucleus? To find out, check the flag set by operatorname
        std::vector<NodePtr> parts{nucleusList, x};
        if (needsSpaceAfterSubsuper_) parts.push_back(thinSpace());
        needsSpaceAfterSubsuper_ = false;
        return makeHlist(std::move(parts));
    }

    // Parser.accent
    NodePtr makeAccent(const std::string& accent, const NodePtr& sym) {
        State& st = state();
        const double thickness = st.underlineThickness();
        HlistPtr centered;
        if (symbolSet("wide_accents").count(codepoints(accent))) {
            NodePtr box = autoWidthChar(codepoints("\\" + accent), sym->width, st, true);
            if (!ok_) return nullptr;
            centered = hcentered({box});
        } else {
            auto box = mustChar(codepoints(mathtext::accentMap().at(accent)), st, true);
            if (!ok_) return nullptr;
            if (accent == "mathring") {
                box->shrink();
                box->shrink();
            }
            centered = hcentered({hbox(sym->width / 4.0), box});
        }
        centered->hpack(sym->width, true);
        return makeVlist({centered, vbox(0., thickness * 2.0), makeHlist({sym})});
    }

    // Parser.operatorname and Parser.function: the name upright, a thin space after it unless a
    // delimiter or a script follows.
    NodePtr operatorName(std::size_t loc, const std::string& functionName, std::vector<NodePtr>* parts) {
        pushState();
        state().setFont("rm");
        std::vector<NodePtr> list;
        std::size_t nextCharLoc = loc + 1;
        if (parts) {
            // Change the font of Chars, but leave Kerns alone
            for (const NodePtr& c : *parts) {
                if (c->kind == Kind::Char) {
                    auto& ch = static_cast<CharNode&>(*c);
                    ch.font = "rm";
                    std::string why;
                    if (!ch.update(&why)) return fail(why), nullptr;
                }
                list.push_back(c);
            }
            nextCharLoc += parts->size() + 14;  // len('operatorname{}'), one character for each part
        } else {
            for (char c : functionName) {
                list.push_back(mustChar(std::u32string(1, char32_t(c)), state()));
                if (!ok_) return nullptr;
            }
            nextCharLoc += functionName.size();
        }
        const char32_t next = nextNonSpace(nextCharLoc);
        const std::u32string nextChar = next ? std::u32string(1, next) : std::u32string();
        const bool script = next == U'^' || next == U'_';
        const bool delimiter = next && (symbolSet("left_delims").count(nextChar) || symbolSet("right_delims").count(nextChar) ||
                                        symbolSet("ambi_delims").count(nextChar));
        // Add thin space except when followed by parenthesis, bracket, etc.
        if (!delimiter && !script) list.push_back(thinSpace());
        popState();
        // If followed by a sub/superscript, set flag to true to tell subsuper to add space after this operator.
        needsSpaceAfterSubsuper_ = script;
        auto h = makeHlist(std::move(list));
        h->functionName = functionName;
        return h;
    }

    // Parser.boldsymbol
    NodePtr boldsymbol(const std::vector<NodePtr>& parts) {
        pushState();
        std::vector<NodePtr> list;
        std::string why;
        for (const NodePtr& c : parts) {
            if (c->kind == Kind::Hlist) {
                auto& children = static_cast<HlistNode&>(*c).children;
                if (children.size() < 2) return fail("\\boldsymbol of an empty group"), nullptr;  // (IndexError)
                if (children[1]->kind == Kind::Char) {
                    auto& k = static_cast<CharNode&>(*children[1]);
                    k.font = "bf";
                    if (!k.update(&why)) return fail(why), nullptr;
                }
            } else if (c->kind == Kind::Char) {
                auto& ch = static_cast<CharNode&>(*c);
                ch.font = "bf";
                const bool latin = ch.c.size() == 1 && isAsciiLetter(ch.c[0]);
                // (c.c[1:] in _small_greek: the name without its backslash)
                if (latin || symbolSet("small_greek").count(ch.c.substr(1))) ch.font = "bfit";
                if (!ch.update(&why)) return fail(why), nullptr;
            }
            list.push_back(c);
        }
        popState();
        return makeHlist(std::move(list));
    }

    // \genfrac{ldelim}{rdelim}{rulesize}{style}{num}{den}
    NodePtr parseGenfrac() {
        const auto bad = [&] { return fail("Expected \\genfrac{ldelim}{rdelim}{rulesize}{style}{num}{den}"), nullptr; };
        std::u32string delims[2];
        for (auto& d : delims) {
            if (!literal("{")) return bad();
            skipWhite();
            d = delimiterHere();
            pos_ += d.size();
            if (!literal("}")) return bad();
        }
        double rule = 0;
        if (!(literal("{") && floatLiteral(&rule) && literal("}"))) return bad();
        if (!literal("{")) return bad();
        int style = 1;  // TEXTSTYLE
        skipWhite();
        if (pos_ < s_.size() && s_[pos_] >= U'0' && s_[pos_] <= U'3') style = int(s_[pos_++] - U'0');
        if (!literal("}")) return bad();
        HlistPtr num = braced(true);
        HlistPtr den = num ? braced(true) : nullptr;
        if (!ok_) return nullptr;
        if (!den) return bad();
        return genfrac(delims[0], delims[1], rule, style, num, den);
    }

    // The delimiter at the position (one_of(_delims)), "" if none
    std::u32string delimiterHere() const {
        std::u32string best;
        for (const char* set : {"left_delims", "right_delims", "ambi_delims"}) {
            const std::u32string d = oneOf(symbolSet(set));
            if (d.size() > best.size()) best = d;
        }
        return best;
    }

    // Parser._genfrac (style: 0 display, 1 text, 2 script, 3 scriptscript)
    NodePtr genfrac(const std::u32string& ldelim, const std::u32string& rdelim, std::optional<double> rule, int style,
                    const HlistPtr& num, const HlistPtr& den) {
        State& st = state();
        std::string why;
        const double thickness = st.underlineThickness();
        const auto axis = st.fontset->axisHeight(st.font, st.fontsize, st.dpi, &why);
        const auto xh = st.fontset->xheight(st.font, st.fontsize, st.dpi, &why);
        if (!axis || !xh) return fail(why), nullptr;
        const double axisHeight = *axis;
        double xHeight = *xh;
        const FontConstants& consts = st.fontset->constants();
        for (int i = 0; i < style; ++i) {
            xHeight *= kShrinkFactor;
            num->shrink();
            den->shrink();
        }
        auto cnum = hcentered({num});
        auto cden = hcentered({den});
        const double width = pyMax(num->width, den->width);
        cnum->hpack(width, true);
        cden->hpack(width, true);
        VlistPtr vlist;
        if (rule && *rule != 0) {
            double numShiftUp, denShiftDown, clr;
            if (style == 0) {
                numShiftUp = consts.num1 * xHeight;
                denShiftDown = consts.denom1 * xHeight;
                clr = 3 * *rule;  // The minimum clearance.
            } else {
                numShiftUp = consts.num2 * xHeight;
                denShiftDown = consts.denom2 * xHeight;
                clr = *rule;
            }
            const double delta = *rule / 2;
            const double numClr = pyMax((numShiftUp - cnum->depth) - (axisHeight + delta), clr);
            const double denClr = pyMax((axisHeight - delta) - (cden->height - denShiftDown), clr);
            vlist = makeVlist({cnum, vbox(0, numClr), hrule(st, *rule), vbox(0, denClr), cden});
            vlist->shiftAmount = cden->height + denClr + delta - axisHeight;
        } else {
            double numShiftUp, denShiftDown, minClr;
            if (style == 0) {
                numShiftUp = consts.num1 * xHeight;
                denShiftDown = consts.denom1 * xHeight;
                minClr = 7 * thickness;
            } else {
                numShiftUp = consts.num3 * xHeight;
                denShiftDown = consts.denom2 * xHeight;
                minClr = 3 * thickness;
            }
            const double defClr = (numShiftUp - cnum->depth) - (cden->height - denShiftDown);
            const double clr = pyMax(defClr, minClr);
            vlist = makeVlist({cnum, vbox(0, clr), cden});
            vlist->shiftAmount = denShiftDown;
            if (defClr < minClr) vlist->shiftAmount += (minClr - defClr) / 2;
        }
        NodePtr result = makeHlist({hbox(thickness), vlist, hbox(thickness)});
        if (!ldelim.empty() || !rdelim.empty()) {
            std::vector<Middle> middle{result};
            return autoSizedDelimiter(ldelim.empty() ? U"." : ldelim, middle, rdelim.empty() ? U"." : rdelim);
        }
        return result;
    }

    // Parser._genset (\overset and \underset)
    NodePtr genset(bool under, const HlistPtr& annotation, const HlistPtr& body) {
        const double thickness = state().underlineThickness();
        annotation->shrink();
        auto centeredAnnotation = hcentered({annotation});
        auto centeredBody = hcentered({body});
        const double width = pyMax(centeredAnnotation->width, centeredBody->width);
        centeredAnnotation->hpack(width, true);
        centeredBody->hpack(width, true);
        const double vgap = thickness * 3;
        if (under) {
            auto vlist = makeVlist({centeredBody, vbox(0, vgap), centeredAnnotation});
            // Shift so the body sits in the same vertical position
            vlist->shiftAmount = centeredBody->depth + centeredAnnotation->height + vgap;
            return vlist;
        }
        return makeVlist({centeredAnnotation, vbox(0, vgap), centeredBody});
    }

    // \sqrt[root]{value}
    NodePtr parseSqrt() {
        const auto bad = [&] { return fail("Expected \\sqrt{value}"), nullptr; };
        std::vector<NodePtr> root;
        bool haveRoot = false;
        if (literal("[")) {
            // OneOrMore(NotAny("]") + token) "]"
            int count = 0;
            for (;;) {
                const std::size_t before = pos_;
                skipWhite();
                const bool closing = pos_ < s_.size() && s_[pos_] == U']';
                pos_ = before;
                if (closing || !ok_ || !token(root)) break;
                ++count;
            }
            if (!ok_) return nullptr;
            // (without its "]" the root is not there, and the "[" is no group either)
            if (count == 0 || !literal("]")) return bad();
            haveRoot = true;
        }
        HlistPtr body = braced(true);
        if (!ok_) return nullptr;
        if (!body) return bad();
        State& st = state();
        const double thickness = st.underlineThickness();
        // Determine the height of the body, and add a little extra to the height so it doesn't seem cramped
        double height = body->height - body->shiftAmount + 5 * thickness;
        double depth = body->depth + body->shiftAmount;
        HlistPtr check = autoHeightChar(U"\\__sqrt__", height, depth, st, std::nullopt);
        if (!ok_) return nullptr;
        height = check->height - check->shiftAmount;
        depth = check->depth + check->shiftAmount;
        // Put a little extra space to the left and right of the body
        auto paddedBody = makeHlist({hbox(2 * thickness), body, hbox(2 * thickness)});
        auto rightside = makeVlist({hrule(st), glueFill(), paddedBody});
        // Stretch the glue between the hrule and the body
        rightside->vpack(height + (st.fontsize * st.dpi) / (100 * 12), true, depth);
        NodePtr rootBox;
        if (!haveRoot) {
            rootBox = makeBox(0.5 * check->width, 0., 0.);
        } else {
            auto r = makeHlist(std::move(root));
            r->shrink();
            r->shrink();
            rootBox = r;
        }
        auto rootVlist = makeVlist({makeHlist({rootBox})});
        rootVlist->shiftAmount = -height * 0.6;
        return makeHlist({rootVlist, kern(-0.5 * check->width), check, rightside});
    }

    NodePtr overline(const HlistPtr& body) {
        State& st = state();
        const double thickness = st.underlineThickness();
        const double height = body->height - body->shiftAmount + 3 * thickness;
        const double depth = body->depth + body->shiftAmount;
        auto rightside = makeVlist({hrule(st), glueFill(), makeHlist({body})});
        rightside->vpack(height + (st.fontsize * st.dpi) / (100 * 12), true, depth);
        return makeHlist({rightside});
    }

    NodePtr underline(const HlistPtr& body) {
        State& st = state();
        const double thickness = st.underlineThickness();
        auto vlist = makeVlist({makeHlist({body}), kern(3 * thickness), hrule(st, thickness)});
        const double delta = vlist->height + vlist->depth + thickness;
        vlist->height = body->height;
        vlist->depth = delta - vlist->height;
        return makeHlist({vlist});
    }

    // \substack{row \\ row ...}
    // A string in quotes as pyparsing's quoted_string takes it ('' inside is one quote, a backslash
    // escapes a character): its length, or 0 when none starts here.
    std::size_t quotedString(std::size_t at) const {
        if (at >= s_.size() || (s_[at] != U'"' && s_[at] != U'\'')) return 0;
        const char32_t q = s_[at];
        const auto hex = [](char32_t c) { return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'f') || (c >= U'A' && c <= U'F'); };
        std::size_t i = at + 1;
        while (i < s_.size()) {
            const char32_t c = s_[i];
            if (c != q && c != U'\n' && c != U'\r' && c != U'\\') {
                ++i;
            } else if (c == q && i + 1 < s_.size() && s_[i + 1] == q) {
                i += 2;
            } else if (c == U'\\' && i + 1 < s_.size() && s_[i + 1] != U'x') {
                i += 2;
            } else if (c == U'\\' && i + 2 < s_.size() && hex(s_[i + 2])) {
                i += 3;
            } else {
                break;
            }
        }
        return i < s_.size() && s_[i] == q ? i + 1 - at : 0;
    }

    // \substack: its argument is pyparsing's nested_expr over braces, whose items are quoted strings,
    // nested braces, and rows of tokens each closed by any number of \\. Only the rows can be laid
    // out: matplotlib breaks on a quoted string and on braces that are not empty, and empty braces
    // are an empty row.
    NodePtr parseSubstack() {
        if (!literal("{")) return fail("Expected \\substack{parts}"), nullptr;
        std::vector<HlistPtr> rows;
        bool breaks = false;
        std::vector<int> open{0};  // the number of items of each list of braces that is open
        const auto item = [&](HlistPtr row) {
            if (open.size() > 1) ++open.back();
            else if (row) rows.push_back(row);
            else breaks = true;
        };
        while (!open.empty()) {
            const std::size_t save = pos_;
            skipWhite();
            if (const std::size_t n = quotedString(pos_)) {
                pos_ += n;
                item(nullptr);
                continue;
            }
            pos_ = save;
            if (literal("{")) {
                open.push_back(0);
                continue;
            }
            std::vector<NodePtr> row;
            int count = 0;
            while (ok_ && token(row)) ++count;
            if (!ok_) return nullptr;
            if (count > 0) {
                while (literal("\\\\")) {}
                item(makeHlist(std::move(row)));
                continue;
            }
            if (!literal("}")) return fail("Expected \\substack{parts}"), nullptr;
            const int inside = open.back();
            open.pop_back();
            if (!open.empty()) item(inside == 0 ? makeHlist({}) : nullptr);
        }
        if (breaks) return fail("\\substack takes neither a quoted string nor braces as a row"), nullptr;
        if (rows.empty()) return fail("\\substack needs a row"), nullptr;  // (max() of nothing)
        const double thickness = state().underlineThickness();
        double maxWidth = rows[0]->width;
        for (const auto& r : rows) maxWidth = pyMax(maxWidth, r->width);
        std::vector<NodePtr> stack;
        const NodePtr gap = vbox(0, thickness * 2);  // (one box, used between every two rows)
        for (std::size_t i = 0; i < rows.size(); ++i) {
            auto cp = hcentered({rows[i]});
            cp->hpack(maxWidth, true);
            stack.push_back(cp);
            if (i + 1 < rows.size()) stack.push_back(gap);
        }
        return makeHlist({makeVlist(std::move(stack))});
    }

    // auto_delim: \left delim (simple | auto_delim | \middle delim)* \right delim
    NodePtr autoDelim() {
        if (!literal("\\left")) return nullptr;
        skipWhite();
        const std::u32string left = delimiterHere();
        if (left.empty()) return fail("Expected a delimiter"), nullptr;
        pos_ += left.size();
        std::vector<Middle> mid;
        for (;;) {
            std::vector<NodePtr> parts;
            if (simple(parts)) {
                for (auto& p : parts) mid.emplace_back(p);
                continue;
            }
            if (!ok_) return nullptr;
            const std::size_t save = pos_;
            skipWhite();
            if (startsWith("\\left")) {
                NodePtr inner = autoDelim();
                if (!ok_) return nullptr;
                mid.emplace_back(inner);
                continue;
            }
            if (startsWith("\\middle")) {
                pos_ += 7;
                skipWhite();
                const std::u32string d = delimiterHere();
                if (d.empty()) return fail("Expected a delimiter"), nullptr;
                pos_ += d.size();
                mid.emplace_back(std::u32string(U"\\middle"));
                mid.emplace_back(d);
                continue;
            }
            pos_ = save;
            break;
        }
        if (!literal("\\right")) return fail("Expected '\\right'"), nullptr;
        skipWhite();
        const std::u32string right = delimiterHere();
        if (right.empty()) return fail("Expected a delimiter"), nullptr;
        pos_ += right.size();
        return autoSizedDelimiter(left, mid, right);
    }

    // Parser._auto_sized_delimiter
    NodePtr autoSizedDelimiter(const std::u32string& front, std::vector<Middle>& middle, const std::u32string& back) {
        State& st = state();
        double height = 0, depth = 0;
        std::optional<double> factor;
        std::vector<NodePtr> middlePart;
        if (!middle.empty()) {
            bool any = false;
            for (const Middle& m : middle)
                if (const NodePtr* n = std::get_if<NodePtr>(&m)) {
                    height = any ? pyMax(height, (*n)->height) : (*n)->height;
                    depth = any ? pyMax(depth, (*n)->depth) : (*n)->depth;
                    any = true;
                }
            if (!any) return fail("nothing between the delimiters to size them by"), nullptr;  // (max() of nothing)
            // Python iterates the list while it changes it: after a \middle the next element is skipped
            for (std::size_t idx = 0; idx < middle.size(); ++idx) {
                const std::u32string* el = std::get_if<std::u32string>(&middle[idx]);
                if (!el || *el != U"\\middle") continue;
                const std::u32string c = std::get<std::u32string>(middle[idx + 1]);
                if (c != U".") {
                    middle[idx + 1] = NodePtr(autoHeightChar(c, height, depth, st, factor));
                    if (!ok_) return nullptr;
                } else {
                    // middle.remove(c): the first "." of the list
                    for (std::size_t k = 0; k < middle.size(); ++k)
                        if (const std::u32string* str = std::get_if<std::u32string>(&middle[k]); str && *str == c) {
                            middle.erase(middle.begin() + long(k));
                            break;
                        }
                }
                middle.erase(middle.begin() + long(idx));
            }
            for (const Middle& m : middle) {
                if (const NodePtr* n = std::get_if<NodePtr>(&m)) middlePart.push_back(*n);
                else return fail("a delimiter was left unsized"), nullptr;
            }
        } else {
            factor = 1.0;
        }
        std::vector<NodePtr> parts;
        // \left. and \right. aren't supposed to produce any symbols
        if (front != U".") {
            parts.push_back(autoHeightChar(front, height, depth, st, factor));
            if (!ok_) return nullptr;
        }
        parts.insert(parts.end(), middlePart.begin(), middlePart.end());
        if (back != U".") {
            parts.push_back(autoHeightChar(back, height, depth, st, factor));
            if (!ok_) return nullptr;
        }
        return makeHlist(std::move(parts));
    }

    // AutoHeightChar: a character as close to the given height and depth as possible
    HlistPtr autoHeightChar(const std::u32string& c, double height, double depth, const State& stateIn,
                            std::optional<double> factor) {
        std::string why;
        const auto alternatives = stateIn.fontset->sizedAlternatives(stateIn.font, c);
        const auto xh = stateIn.fontset->xheight(stateIn.font, stateIn.fontsize, stateIn.dpi, &why);
        if (!xh) return fail(why), nullptr;
        State st = stateIn;
        const double targetTotal = height + depth;
        std::shared_ptr<CharNode> ch;
        std::u32string sym;
        for (const auto& [fontname, s] : alternatives) {
            st.setFont(fontname);
            sym = s;
            ch = makeChar(sym, st);
            if (!ch) return fail("Unknown symbol: " + utf8(sym)), nullptr;
            // Ensure that size 0 is chosen when the text is regular sized but with descender glyphs
            // by subtracting 0.2 * x_height
            if (ch->height + ch->depth >= targetTotal - 0.2 * *xh) break;
        }
        if (!ch) return fail("no font has the delimiter " + utf8(c)), nullptr;
        double shift = 0.0;
        if (st.font != "0" || alternatives.size() == 1) {
            if (!factor) {
                if (ch->height + ch->depth == 0) return fail("division by zero"), nullptr;
                factor = targetTotal / (ch->height + ch->depth);
            }
            st.fontsize *= *factor;
            ch = makeChar(sym, st);
            if (!ch) return fail("Unknown symbol: " + utf8(sym)), nullptr;
            shift = (depth - ch->depth);
        }
        auto list = makeHlist({ch});
        list->shiftAmount = shift;
        return list;
    }

    // AutoWidthChar: a character as close to the given width as possible
    NodePtr autoWidthChar(const std::u32string& c, double width, const State& stateIn, bool accent) {
        const auto alternatives = stateIn.fontset->sizedAlternatives(stateIn.font, c);
        State st = stateIn;
        std::shared_ptr<CharNode> ch;
        std::u32string sym;
        for (const auto& [fontname, s] : alternatives) {
            st.setFont(fontname);
            sym = s;
            ch = makeChar(sym, st, accent);
            if (!ch) return fail("Unknown symbol: " + utf8(sym)), nullptr;
            if (ch->width >= width) break;
        }
        if (!ch) return fail("no font has the accent " + utf8(c)), nullptr;
        if (ch->width == 0) return fail("division by zero"), nullptr;
        const double factor = width / ch->width;
        st.fontsize *= factor;
        ch = makeChar(sym, st, accent);
        if (!ch) return fail("Unknown symbol: " + utf8(sym)), nullptr;
        auto list = makeHlist({ch});
        list->width = ch->width;
        return list;
    }

    const rc::Params& rc_;
    std::vector<State> states_;
    std::u32string s_;
    std::size_t pos_ = 0;
    bool ok_ = true;
    bool needsSpaceAfterSubsuper_ = false;
    std::map<std::tuple<std::string, double, double>, double> emWidth_;
};

// --- ship ----------------------------------------------------------------------------------------

struct Shipped {
    struct Glyph {
        double ox, oy;
        CharNode* node;
    };
    std::vector<Glyph> glyphs;
    std::vector<std::array<double, 4>> rects;
};

double clampGlue(double value) { return value < -1e9 ? -1e9 : value > +1e9 ? +1e9 : value; }

void ship(HlistNode& root, Shipped& out) {
    double curV = 0., curH = 0.;
    const double offV = root.height;  // (and off_h = 0: mathtext ships its boxes at the origin)
    std::vector<bool> phantom;
    const auto hidden = [&] { return std::find(phantom.begin(), phantom.end(), true) != phantom.end(); };
    const auto renderChar = [&](CharNode& c, double x, double y) {
        if (hidden()) return;
        if (c.accent) out.glyphs.push_back({x - c.metrics.xmin, y + c.metrics.ymin, &c});
        else out.glyphs.push_back({x, y, &c});
    };
    const auto renderRule = [&](double x, double y, double w, double h) {
        if (hidden()) return;
        out.rects.push_back({x, y - h, w, h});
    };
    std::function<void(HlistNode&)> hlistOut;
    std::function<void(VlistNode&)> vlistOut;
    // The glue's width as the list has set it
    const auto glued = [](const ListNode& box, const GlueNode& g, double& curG, double& curGlue) {
        double ruleSize = -curG;
        if (box.glueSign != 0) {
            if (box.glueSign == 1) {  // stretching
                if (g.stretchOrder == box.glueOrder) {
                    curGlue += g.stretch;
                    curG = std::nearbyint(clampGlue(box.glueSet * curGlue));
                }
            } else if (g.shrinkOrder == box.glueOrder) {
                curGlue += g.shrinkage;
                curG = std::nearbyint(clampGlue(box.glueSet * curGlue));
            }
        }
        ruleSize += curG;
        return ruleSize;
    };
    hlistOut = [&](HlistNode& box) {
        double curG = 0, curGlue = 0.;
        const double baseLine = curV;
        phantom.push_back(box.isPhantom);
        for (const auto& p : box.children) {
            if (p->kind == Kind::Char) {
                renderChar(static_cast<CharNode&>(*p), curH, curV + offV);
                curH += p->width;
            } else if (p->kind == Kind::Kern) {
                curH += p->width;
            } else if (p->isList()) {
                auto& l = static_cast<ListNode&>(*p);
                // node623 (matplotlib only adds the width of an empty list: the same)
                const double edge = curH;
                curV = baseLine + l.shiftAmount;
                if (p->kind == Kind::Hlist) hlistOut(static_cast<HlistNode&>(*p));
                else vlistOut(static_cast<VlistNode&>(*p));  // p.vpack(box.height + box.depth, 'exactly')
                curH = edge + p->width;
                curV = baseLine;
            } else if (p->isBox()) {
                // node624: a rule would be drawn here, but the parser sets rules in vertical lists
                // only; what comes is an Hbox, which has a width and nothing to draw
                curH += p->width;
            } else if (p->kind == Kind::Glue) {
                // node625
                curH += glued(box, static_cast<const GlueNode&>(*p), curG, curGlue);
            }
        }
        phantom.pop_back();
    };
    vlistOut = [&](VlistNode& box) {
        double curG = 0, curGlue = 0.;
        const double leftEdge = curH;
        curV -= box.height;
        for (const auto& p : box.children) {
            if (p->kind == Kind::Kern) {
                curV += p->width;
            } else if (p->isList()) {
                // (the parser puts only horizontal lists in a vertical one, and none of them empty:
                // matplotlib's shortcut for an empty list and its vlist_out here are not needed)
                auto& l = static_cast<HlistNode&>(*p);
                curV += p->height;
                curH = leftEdge;  // (+ the list's shift, which is zero in a vertical list)
                const double saveV = curV;
                p->width = box.width;
                hlistOut(l);
                curV = saveV + p->depth;
                curH = leftEdge;
            } else if (p->isBox()) {
                double ruleHeight = p->height;
                const double ruleDepth = p->depth;
                double ruleWidth = p->width;
                if (std::isinf(ruleWidth)) ruleWidth = box.width;
                ruleHeight += ruleDepth;
                if (ruleHeight > 0 && ruleDepth > 0) {
                    curV += ruleHeight;
                    if (p->kind == Kind::Rule) renderRule(curH, curV + offV, ruleWidth, ruleHeight);
                }
            } else if (p->kind == Kind::Glue) {
                curV += glued(box, static_cast<const GlueNode&>(*p), curG, curGlue);
            }
        }
    };
    hlistOut(root);
}

}  // namespace

std::optional<MathParse> parseMath(FontCache& fonts, const std::string& s, double dpi, const FontProperties& prop,
                                   const rc::Params& rc, std::string* error) {
    // MathTextParser('path'): the glyphs are loaded unhinted
    const std::unique_ptr<Fonts> fontset = mathtext::makeFontset(prop.mathFontfamily, fonts, prop, rc, FT_LOAD_NO_HINTING);
    if (!fontset->failure().empty()) {  // (matplotlib raises as it makes the fontset, whatever the text)
        *error = fontset->failure();
        return std::nullopt;
    }
    Parser parser(*fontset, prop.size, dpi, rc);
    // (pyparsing expands the tabs of what it parses)
    const auto box = parser.parse(codepoints(support::expandTabs(s)));
    if (!box) {
        *error = parser.error;
        return std::nullopt;
    }
    Shipped shipped;
    ship(*box, shipped);
    // Output.to_vector
    MathParse out;
    const double w = std::ceil(box->width), h = std::ceil(box->height), d = std::ceil(box->depth);
    out.width = w;
    out.height = h + d;
    out.depth = d;
    for (const auto& g : shipped.glyphs) {
        const auto info = fontset->info(g.node->font, g.node->fontClass, g.node->c, g.node->fontsize, g.node->dpi, error);
        if (!info) return std::nullopt;
        out.glyphs.push_back(MathGlyph{info->font, info->file, info->fontsize, info->num, info->glyphIndex, g.ox, h - g.oy + info->offset});
    }
    for (const auto& r : shipped.rects) out.rects.push_back(std::array<double, 4>{r[0], h - (r[1] + r[3]), r[2], r[3]});
    return out;
}

}  // namespace calculate_plot::text
