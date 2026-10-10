// Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (the box model, the DejaVu Sans
// fontset, the parser's handlers) and lib/matplotlib/mathtext.py (MathTextParser).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/mathtext.hpp"

#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"

#include <freetype/ftglyph.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <tuple>
#include <limits>
#include <map>
#include <memory>
#include <set>

namespace calculate_plot::text {

namespace {

constexpr double kShrinkFactor = 0.7;  // SHRINK_FACTOR
constexpr int kNumSizeLevels = 6;      // NUM_SIZE_LEVELS

// Python's max(a, b): the first unless the second is greater.
double pyMax(double a, double b) { return b > a ? b : a; }

// _mathtext_data.tex2uni (data/tex2uni.json)
const std::map<std::string, unsigned>& tex2uni() {
    static const std::map<std::string, unsigned> table = [] {
        std::map<std::string, unsigned> out;
        const auto doc = support::parseJson(embedded::file("data/tex2uni.json"), nullptr);
        if (doc && doc->find("tex2uni"))
            for (const auto& [name, code] : doc->find("tex2uni")->members()) out[name] = unsigned(code.asInteger());
        return out;
    }();
    return table;
}

std::string utf8(char32_t c) {
    std::string out;
    if (c < 0x80) out += char(c);
    else if (c < 0x800) out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
    else if (c < 0x10000)
        out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
    else
        out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)), out += char(0x80 | ((c >> 6) & 0x3F)),
            out += char(0x80 | (c & 0x3F));
    return out;
}

// get_unicode_index: a single character, or a TeX name (without its backslash) in tex2uni.
std::optional<unsigned> unicodeIndex(const std::u32string& sym) {
    if (sym.size() == 1) return unsigned(sym[0]);
    std::string name;
    for (char32_t c : sym)
        if (c != U'\\') name += utf8(c);
    const auto it = tex2uni().find(name);
    if (it == tex2uni().end()) return std::nullopt;
    return it->second;
}

// For the code points the supported TeX reaches: true for letters (Unicode category L*), false for
// the others and for capital Greek letters; nullopt for code points not classified yet.
std::optional<bool> letterClass(unsigned c) {
    if (c < 0x80) return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    if (c >= 0x3B1 && c <= 0x3C9) return true;                       // Greek small letters
    if (c >= 0x391 && c <= 0x3A9) return false;                      // Greek capital letters
    if (c == 0xD7 || c == 0xF7 || c == 0xB1 || c == 0xB0) return false;  // multiplication, division, plus-minus, degree
    if ((c >= 0x2190 && c <= 0x22FF) || (c >= 0x2000 && c <= 0x206F)) return false;  // arrows, operators, punctuation
    return std::nullopt;
}

// --- Fonts -------------------------------------------------------------------------------------------

struct FontMetrics {
    double advance, height, width, xmin, xmax, ymin, ymax, iceberg;
    bool slanted;
};

struct FontInfo {
    FT2Font* font;
    std::string file;
    double fontsize;
    unsigned num;
    FT_UInt glyphIndex;
    FontMetrics metrics;
    double offset;
};

// A FontProperties from a fontconfig pattern of the forms the fontsets use: "Family",
// "Family:italic", "Family:weight=bold", "Family:italic:bold".
FontProperties fromPattern(const std::string& pattern, const rc::Params& rc) {
    FontProperties p = FontProperties::defaults(rc);
    std::size_t colon = pattern.find(':');
    p.family = {pattern.substr(0, colon)};
    while (colon != std::string::npos) {
        const std::size_t next = pattern.find(':', colon + 1);
        const std::string prop = pattern.substr(colon + 1, next == std::string::npos ? std::string::npos : next - colon - 1);
        if (prop == "italic" || prop == "oblique") p.style = prop;
        else if (prop == "bold") p.weight = std::string("bold");
        else if (prop.compare(0, 7, "weight=") == 0) p.weight = prop.substr(7);
        colon = next;
    }
    return p;
}

// DejaVuSansFonts (TruetypeFonts, UnicodeFonts, DejaVuFonts) for the glyphs the subset needs.
class DejaVuSansFonts {
public:
    DejaVuSansFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags)
        : cache_(cache), rc_(rc), flags_(flags) {
        const std::string defaultFile = findFont(defaultProp, rc);
        files_["default"] = files_["regular"] = defaultFile;
        // DejaVuSansFonts._fontmap, then the STIX size alternatives DejaVuFonts adds
        const std::pair<const char*, const char*> fontmap[] = {
            {"rm", "DejaVu Sans"}, {"it", "DejaVu Sans:italic"}, {"bf", "DejaVu Sans:weight=bold"},
            {"bfit", "DejaVu Sans:italic:bold"}, {"sf", "DejaVu Sans"}, {"tt", "DejaVu Sans Mono"},
            {"ex", "DejaVu Sans Display"}, {"0", "DejaVu Sans"}, {"1", "STIXSizeOneSym"}, {"2", "STIXSizeTwoSym"},
            {"3", "STIXSizeThreeSym"}, {"4", "STIXSizeFourSym"}, {"5", "STIXSizeFiveSym"}};
        for (const auto& [key, pattern] : fontmap) files_[key] = findFont(fromPattern(pattern, rc), rc);
    }

    // FontInfo of a symbol (TruetypeFonts._get_info); nullopt with *error for glyphs outside the subset.
    std::optional<FontInfo> info(const std::string& fontname, const std::string& fontClass, const std::u32string& sym,
                                 double fontsize, double dpi, std::string* error) {
        const auto key = std::make_tuple(fontname, fontClass, sym, fontsize, dpi);
        const auto cached = infos_.find(key);
        if (cached != infos_.end()) return cached->second;
        std::string file;
        unsigned num = 0;
        bool slanted = false;
        if (!glyph(fontname, sym, &file, &num, &slanted, error)) return std::nullopt;
        FT2Font* font = cache_.get({file});
        font->set_size(fontsize, dpi);
        const FT_UInt glyphIndex = font->get_char_index(num, false);
        font->load_glyph(glyphIndex, flags_);
        const FT_Face face = font->get_face();
        FT_BBox bbox;
        FT_Glyph_Get_CBox(font->get_glyphs().back(), FT_GLYPH_BBOX_SUBPIXELS, &bbox);
        const double offset = 0.;  // _get_offset: only Cmex10 has one
        FontInfo info{font,
                      file,
                      fontsize,
                      num,
                      glyphIndex,
                      FontMetrics{double(face->glyph->linearHoriAdvance) / 65536, double(face->glyph->metrics.height) / 64,
                                  double(face->glyph->metrics.width) / 64, double(bbox.xMin) / 64, double(bbox.xMax) / 64,
                                  double(bbox.yMin) / 64 + offset, double(bbox.yMax) / 64 + offset,
                                  double(face->glyph->metrics.horiBearingY) / 64 + offset, slanted},
                      offset};
        infos_.emplace(key, info);
        return info;
    }

    double xheight(double fontsize, double dpi) const { return kXHeight * fontsize * dpi / 72; }
    double underlineThickness(double fontsize, double dpi) const { return ((0.75 / 12) * fontsize * dpi) / 72; }

    // get_quad: DejaVuSansFontConstants has no quad, so the advance of 'm'
    std::optional<double> quad(double fontsize, double dpi, std::string* error) {
        const auto i = info("normal", rc_.text("mathtext.default"), U"m", fontsize, dpi, error);
        if (!i) return std::nullopt;
        return i->metrics.advance;
    }

    // get_kern: kerning within one font name and size
    std::optional<double> kern(const std::string& font1, const std::string& class1, const std::u32string& sym1, double size1,
                               const std::string& font2, const std::string& class2, const std::u32string& sym2,
                               double size2, double dpi, std::string* error) {
        if (font1 == font2 && size1 == size2) {
            const auto i1 = info(font1, class1, sym1, size1, dpi, error);
            const auto i2 = info(font2, class2, sym2, size2, dpi, error);
            if (!i1 || !i2) return std::nullopt;
            return double(i1->font->get_kerning(i1->glyphIndex, i2->glyphIndex, FT_KERNING_DEFAULT)) / 64;
        }
        return 0.;
    }

    // DejaVuSansFontConstants
    static constexpr double kXHeightUnits = 1120;
    static constexpr double kXHeight = kXHeightUnits / 2048;
    static constexpr double kSupdrop = 790.527 / kXHeightUnits;
    static constexpr double kSubdrop = 102.4 / kXHeightUnits;
    static constexpr double kSup1 = 845.824 / kXHeightUnits;
    static constexpr double kSub1 = 307.199 / kXHeightUnits;
    static constexpr double kSub2 = 632.832 / kXHeightUnits;
    static constexpr double kScriptSpace = 0.05, kDelta = 0.025, kDeltaSlanted = 0.2;  // (delta_integral: drop-subscripts come later)

private:
    std::string fileOf(const std::string& fontname) const {
        const auto it = files_.find(fontname);
        return it == files_.end() ? std::string() : it->second;
    }

    bool hasGlyph(const std::string& file, unsigned num) {
        if (file.empty()) return false;
        FT2Font* font = cache_.get({file});
        return font && font->get_char_index(num, false) != 0;
    }

    // DejaVuFonts._get_glyph, then UnicodeFonts._get_glyph (without its STIX fallback yet)
    bool glyph(const std::string& fontname, const std::u32string& sym, std::string* file, unsigned* num, bool* slanted,
               std::string* error) {
        if (sym == U"\\prime") return *error = "\\prime is not supported yet", false;
        const auto uni = unicodeIndex(sym);
        if (!uni) return *error = "No TeX to Unicode mapping", false;
        std::string name = fontname;
        if (hasGlyph(fileOf("ex"), *uni)) name = "ex";  // DejaVu Sans Display first
        std::string newName = name;
        if (name == "normal" && *uni < 0x10000) {
            // unicodedata.category(char)[0] != "L" or a capital Greek letter: roman, else italic
            const auto category = letterClass(*uni);
            if (!category) return *error = "the character U+" + std::to_string(*uni) + " in 'normal' math is not supported yet", false;
            newName = *category ? "it" : "rm";
        }
        *slanted = newName == "it";  // (\int and \oint are not supported yet)
        const std::string f = fileOf(newName);
        if (f.empty() || !hasGlyph(f, *uni))
            return *error = "the glyph U+" + std::to_string(*uni) + " needs the STIX fallback, not supported yet", false;
        *file = f;
        *num = *uni;
        return true;
    }

    FontCache& cache_;
    const rc::Params& rc_;
    FT_Int32 flags_;
    std::map<std::string, std::string> files_;
    std::map<std::tuple<std::string, std::string, std::u32string, double, double>, FontInfo> infos_;
};

// --- Boxes ---------------------------------------------------------------------------------------

struct State {
    DejaVuSansFonts* fontset;
    std::string font, fontClass;
    double fontsize, dpi;
    void setFont(const std::string& name) {
        if (name == "normal" || name == "rm" || name == "it" || name == "bf" || name == "bfit") fontClass = name;
        font = name;
    }
};

enum class Kind { Box, Hbox, Vbox, Char, Kern, Hlist, Vlist };

struct Node {
    explicit Node(Kind k) : kind(k) {}
    virtual ~Node() = default;
    Kind kind;
    int size = 0;
    double width = 0, height = 0, depth = 0;  // (Kern has height and depth 0)
    bool isBox() const { return kind == Kind::Box || kind == Kind::Hbox || kind == Kind::Vbox || kind == Kind::Hlist || kind == Kind::Vlist; }
    bool isList() const { return kind == Kind::Hlist || kind == Kind::Vlist; }
    virtual void shrink() {
        size += 1;
        if (isBox() && size < kNumSizeLevels) {
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
    }
    virtual bool isCharNode() const { return true; }
};
using NodePtr = std::shared_ptr<Node>;

struct KernNode : Node {
    explicit KernNode(double w) : Node(Kind::Kern) { width = w; }
    void shrink() override {
        size += 1;
        if (size < kNumSizeLevels) width *= kShrinkFactor;
    }
};

struct CharNode : Node {
    CharNode(std::u32string sym, const State& s) : Node(Kind::Char), c(std::move(sym)), state(s) {}
    std::u32string c;
    State state;
    FontMetrics metrics{};
    bool update(std::string* error) {
        const auto info = state.fontset->info(state.font, state.fontClass, c, state.fontsize, state.dpi, error);
        if (!info) return false;
        metrics = info->metrics;
        width = c == U" " ? metrics.advance : metrics.width;
        height = metrics.iceberg;
        depth = -(metrics.iceberg - metrics.height);
        return true;
    }
    void shrink() override {
        size += 1;
        if (size < kNumSizeLevels) {
            state.fontsize *= kShrinkFactor;
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
    }
};

struct ListNode : Node {
    explicit ListNode(Kind k, std::vector<NodePtr> elements) : Node(k), children(std::move(elements)) {}
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
    // _set_glue with no glue: the overfull/underfull warning case (no stretch or shrink anywhere)
    void setGlue(double x, int sign) {
        glueOrder = 0;
        glueSign = sign;
        (void)x;
        glueSign = 0;  // totals[o] == 0
    }
};

struct HlistNode : ListNode {
    explicit HlistNode(std::vector<NodePtr> elements) : ListNode(Kind::Hlist, std::move(elements)) {}
    bool isCharNode() const override {
        for (const auto& c : children)
            if (!c->isCharNode()) return false;
        return true;
    }
    void hpack(double w = 0.0, bool exactly = false) {
        double h = 0., d = 0., x = 0.;
        for (const auto& p : children) {
            if (p->kind == Kind::Char) {
                x += p->width;
                h = pyMax(h, p->height);
                d = pyMax(d, p->depth);
            } else if (p->isBox()) {
                x += p->width;
                if (!std::isinf(p->height) && !std::isinf(p->depth)) {
                    const double s = p->isList() ? static_cast<ListNode&>(*p).shiftAmount : 0.;
                    h = pyMax(h, p->height - s);
                    d = pyMax(d, p->depth + s);
                }
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
        setGlue(x, x > 0. ? 1 : -1);
    }
};

struct VlistNode : ListNode {
    explicit VlistNode(std::vector<NodePtr> elements) : ListNode(Kind::Vlist, std::move(elements)) { vpack(); }
    bool isCharNode() const override { return false; }
    void vpack(double h = 0.0) {
        double w = 0., d = 0., x = 0.;
        for (const auto& p : children) {
            if (p->isBox()) {
                x += d + p->height;
                d = p->depth;
                if (!std::isinf(p->width)) {
                    const double s = p->isList() ? static_cast<ListNode&>(*p).shiftAmount : 0.;
                    w = pyMax(w, p->width + s);
                }
            } else if (p->kind == Kind::Kern) {
                x += d + p->width;
                d = 0.;
            }
        }
        width = w;
        depth = d;  // l = inf
        h += x;
        height = h;
        x = h - x;
        if (x == 0) {
            glueSign = 0;
            glueOrder = 0;
            return;
        }
        setGlue(x, x > 0. ? 1 : -1);
    }
};

// Hlist(elements, do_kern=True): kerning between neighbours, then hpack.
std::shared_ptr<HlistNode> makeHlist(std::vector<NodePtr> elements, bool doKern, std::string* error, bool* ok) {
    auto list = std::make_shared<HlistNode>(std::vector<NodePtr>{});
    if (doKern) {
        std::vector<NodePtr> kerned;
        for (std::size_t i = 0; i < elements.size(); ++i) {
            const NodePtr& e0 = elements[i];
            kerned.push_back(e0);
            double distance = 0.;
            if (e0->kind == Kind::Char) {  // Char.get_kerning
                auto& c0 = static_cast<CharNode&>(*e0);
                const double advance = c0.metrics.advance - c0.width;
                double k = 0.;
                if (i + 1 < elements.size() && elements[i + 1]->kind == Kind::Char) {
                    auto& c1 = static_cast<CharNode&>(*elements[i + 1]);
                    const auto kern = c0.state.fontset->kern(c0.state.font, c0.state.fontClass, c0.c, c0.state.fontsize,
                                                             c1.state.font, c1.state.fontClass, c1.c, c1.state.fontsize,
                                                             c0.state.dpi, error);
                    if (!kern) {
                        *ok = false;
                        return list;
                    }
                    k = *kern;
                }
                distance = advance + k;
            }
            if (distance != 0.) kerned.push_back(std::make_shared<KernNode>(distance));
        }
        list->children = std::move(kerned);
    } else {
        list->children = std::move(elements);
    }
    list->hpack();
    return list;
}

// --- Parser --------------------------------------------------------------------------------------

const std::set<std::u32string>& binaryOperators() {
    static const std::set<std::u32string> s{U"+", U"*", U"-", U"−", U"\\pm", U"\\mp", U"\\times", U"\\div",
                                            U"\\ast", U"\\star", U"\\circ", U"\\bullet", U"\\cdot", U"\\cap", U"\\cup",
                                            U"\\uplus", U"\\dotplus", U"\\dotminus", U"\\setminus", U"\\wedge", U"\\vee",
                                            U"\\oplus", U"\\ominus", U"\\otimes", U"\\oslash", U"\\odot", U"\\slash"};
    return s;
}
const std::set<std::u32string>& relationSymbols() {
    static const std::set<std::u32string> s{U"=", U"<", U">", U":", U"\\leq", U"\\geq", U"\\equiv", U"\\sim", U"\\simeq",
                                            U"\\approx", U"\\neq", U"\\ne", U"\\propto", U"\\ll", U"\\gg", U"\\in",
                                            U"\\subset", U"\\supset", U"\\perp", U"\\mid", U"\\parallel"};
    return s;
}
// The spaced symbols matplotlib has beyond these lists (other relations, arrows) are refused.
const std::set<std::u32string>& otherSpacedSymbols() {
    static const std::set<std::u32string> s{U"\\leftarrow", U"\\rightarrow", U"\\to", U"\\Rightarrow", U"\\Leftarrow",
                                            U"\\leftrightarrow", U"\\mapsto", U"\\dots", U"\\models", U"\\cong"};
    return s;
}
const std::set<std::u32string>& punctuation() {
    static const std::set<std::u32string> s{U",", U";", U".", U"!", U"\\ldotp", U"\\cdotp"};
    return s;
}
const std::set<std::string>& fontNames() {
    static const std::set<std::string> s{"rm", "cal", "it", "tt", "sf", "bf", "bfit", "default", "bb", "frak", "scr", "regular", "normal"};
    return s;
}

bool isLetter(char32_t c) { return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z'); }

class Parser {
public:
    Parser(DejaVuSansFonts& fontset, double fontsize, double dpi, const rc::Params& rc) : rc_(rc) {
        states_.push_back(State{&fontset, "default", "rm", fontsize, dpi});
    }

    // Parser.parse: non_math (math_string non_math)*
    std::shared_ptr<HlistNode> parse(const std::u32string& s) {
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
            // math_string: QuotedString('$', '\\')
            std::size_t j = i + 1;
            while (j < s.size() && s[j] != U'$') j += (s[j] == U'\\' && j + 1 < s.size()) ? 2 : 1;
            if (j >= s.size()) return fail("Expected end of text"), nullptr;
            parts.push_back(math(s.substr(i + 1, j - i - 1)));
            if (!ok_) return nullptr;
            i = j + 1;
        }
        auto main = hlist(std::move(parts));
        return ok_ ? main : nullptr;
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

    std::shared_ptr<HlistNode> hlist(std::vector<NodePtr> elements, bool doKern = true) {
        bool ok = true;
        auto h = makeHlist(std::move(elements), doKern, &error, &ok);
        if (!ok) ok_ = false;
        return h;
    }

    std::shared_ptr<CharNode> character(const std::u32string& c) {
        auto node = std::make_shared<CharNode>(c, state());
        if (!node->update(&error)) ok_ = false;
        return node;
    }

    NodePtr nonMath(const std::u32string& text) {
        std::vector<NodePtr> symbols;
        for (char32_t c : text) symbols.push_back(character(std::u32string(1, c)));
        auto h = hlist(std::move(symbols));
        pushState();
        state().setFont(rc_.text("mathtext.default"));
        return h;
    }

    NodePtr math(const std::u32string& s) {
        s_ = s;
        pos_ = 0;
        std::vector<NodePtr> tokens;
        while (ok_ && skipSpaces() < s_.size()) {
            auto t = token();
            if (!ok_) break;
            if (t) tokens.push_back(t);
        }
        if (ok_ && tokens.empty() && !fontOnly_) fail("Expected token");
        auto h = hlist(std::move(tokens));
        popState();
        return h;
    }

    // pyparsing skips whitespace before each token (and before '^', '_' and '}'), but not before a
    // symbol in a script: x^ 2 has a space as its superscript.
    std::size_t skipSpaces() {
        while (pos_ < s_.size() && (s_[pos_] == U' ' || s_[pos_] == U'\t' || s_[pos_] == U'\n' || s_[pos_] == U'\r')) ++pos_;
        return pos_;
    }

    // token: simple (space | customspace | font | subsuper) | auto_delim | unclosed_group | unknown_symbol
    NodePtr token() {
        const char32_t c = s_[pos_];
        if (c == U'\\') {
            std::size_t j = pos_ + 1;
            while (j < s_.size() && isLetter(s_[j])) ++j;
            std::string name;
            for (std::size_t k = pos_ + 1; k < j; ++k) name += char(s_[k]);
            if (fontNames().count(name)) {  // font: \rm, \it, ...
                state().setFont(name);
                pos_ = j;
                fontOnly_ = true;
                return nullptr;
            }
        }
        return subsuper();
    }

    // placeable: symbol | group (the other placeables are not supported yet); nullptr if none here
    NodePtr placeable(bool* found) {
        *found = false;
        if (pos_ >= s_.size()) return nullptr;
        const char32_t c = s_[pos_];
        if (c == U'{' || (c == U'\\' && s_.compare(pos_, 5, U"\\math") == 0)) {
            if (c == U'\\') {
                std::size_t j = pos_ + 5;
                while (j < s_.size() && isLetter(s_[j])) ++j;
                std::string name;
                for (std::size_t k = pos_ + 5; k < j; ++k) name += char(s_[k]);
                if (!fontNames().count(name) || j >= s_.size() || s_[j] != U'{') {
                    // \mathrm etc. must open a group; anything else is a symbol or unknown
                    return symbolOrUnknown(found);
                }
                *found = true;
                pos_ = j + 1;
                return group(name);
            }
            *found = true;
            ++pos_;
            return group("");
        }
        return symbolOrUnknown(found);
    }

    // group: start_group token* end_group (start_group pushes the state, end_group pops it)
    NodePtr group(const std::string& font) {
        pushState();
        if (!font.empty()) state().setFont(font);
        std::vector<NodePtr> tokens;
        while (ok_ && skipSpaces() < s_.size() && s_[pos_] != U'}') {
            auto t = token();
            if (t) tokens.push_back(t);
        }
        if (!ok_) return nullptr;
        if (pos_ >= s_.size()) return fail("Expected '}'"), nullptr;
        ++pos_;
        popState();
        return hlist(std::move(tokens));
    }

    NodePtr symbolOrUnknown(bool* found) {
        const char32_t c = s_[pos_];
        std::u32string sym;
        std::size_t end = pos_ + 1;
        if (c == U'\\') {
            if (pos_ + 1 >= s_.size()) return nullptr;
            const char32_t n = s_[pos_ + 1];
            if (std::u32string(U"%${}[]_|").find(n) != std::u32string::npos) {
                sym = std::u32string{c, n};
                end = pos_ + 2;
            } else {
                std::size_t j = pos_ + 1;
                while (j < s_.size() && isLetter(s_[j])) ++j;
                std::string name;
                for (std::size_t k = pos_ + 1; k < j; ++k) name += char(s_[k]);
                if (j == pos_ + 1 && n == U'#') {
                    sym = U"\\#";
                    end = pos_ + 2;
                } else if (!name.empty() && tex2uni().count(name)) {
                    sym = s_.substr(pos_, j - pos_);
                    end = j;
                } else {
                    if (name.empty()) return nullptr;
                    static const std::set<std::string> known{"frac", "dfrac", "binom", "genfrac", "sqrt", "overline",
                                                             "underline", "overset", "underset", "text", "substack",
                                                             "hspace", "phantom", "llap", "rlap", "left", "right",
                                                             "middle", "operatorname", "boldsymbol"};
                    if (known.count(name)) return fail("\\" + name + " is not supported yet"), nullptr;
                    return fail("Unknown symbol: \\" + name), nullptr;
                }
            }
        } else {
            static const std::u32string ascii = U"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 +-*/<>=:,.;!?&'@()[]|";
            if (ascii.find(c) == std::u32string::npos && !(c >= 0x80 && c <= 0x1ffff)) return nullptr;
            if (c == U'\'') return fail("apostrophes in math are not supported yet"), nullptr;
            sym = std::u32string(1, c);
        }
        *found = true;
        const std::size_t loc = pos_;
        pos_ = end;
        return symbol(sym, loc);
    }

    // Parser.symbol
    NodePtr symbol(std::u32string c, std::size_t loc) {
        if (c == U"-") c = U"−";
        auto ch = character(c);
        if (!ok_) return nullptr;
        if (otherSpacedSymbols().count(c)) return fail("this spaced symbol is not supported yet"), nullptr;
        const bool spaced = binaryOperators().count(c) || relationSymbols().count(c);
        char32_t prev = 0;  // the last non-space character before it
        for (std::size_t k = loc; k-- > 0;)
            if (s_[k] != U' ') {
                prev = s_[k];
                break;
            }
        if (spaced) {
            bool nothingBefore = true;
            for (std::size_t k = 0; k < loc; ++k)
                if (s_[k] != U' ' && s_[k] != U'\t' && s_[k] != U'\n') nothingBefore = false;
            const bool afterOpen = prev == U'{' || prev == U'(' || prev == U'[' || prev == U'<' || prev == U'=' ||
                                   prev == U'>' || prev == U':';
            if (binaryOperators().count(c) && (nothingBefore || afterOpen)) return ch;
            return hlist({space(0.2), ch, space(0.2)});
        }
        if (punctuation().count(c)) {
            char32_t next = 0;
            for (std::size_t k = loc + 1; k < s_.size(); ++k)
                if (s_[k] != U' ') {
                    next = s_[k];
                    break;
                }
            const auto digit = [](char32_t d) { return d >= U'0' && d <= U'9'; };
            if (c == U"," && prev == U'{' && next == U'}') return ch;
            if (c == U"." && digit(prev) && digit(next)) return ch;
            return hlist({ch, space(0.2)});
        }
        return ch;
    }

    // _make_space
    NodePtr space(double percentage) {
        const auto key = std::make_tuple(state().font, state().fontsize, state().dpi);
        auto it = emWidth_.find(key);
        if (it == emWidth_.end()) {
            const auto q = state().fontset->quad(state().fontsize, state().dpi, &error);
            if (!q) {
                ok_ = false;
                return std::make_shared<KernNode>(0.);
            }
            it = emWidth_.emplace(key, *q).first;
        }
        return std::make_shared<KernNode>(it->second * percentage);
    }

    // subsuper: [placeable] (('_' | '^') placeable)+ | placeable
    NodePtr subsuper() {
        bool found = false;
        const std::size_t start = pos_;
        NodePtr nucleus;
        if (s_[pos_] != U'_' && s_[pos_] != U'^') {
            nucleus = placeable(&found);
            if (!ok_) return nullptr;
            if (!found) {
                pos_ = start;
                return fail("Unexpected character"), nullptr;
            }
        }
        NodePtr sub, super;
        bool any = false;
        for (;;) {
            const std::size_t before = pos_;
            if (!ok_ || skipSpaces() >= s_.size() || (s_[pos_] != U'_' && s_[pos_] != U'^')) {
                pos_ = before;
                break;
            }
            const bool isSub = s_[pos_] == U'_';
            ++pos_;
            bool f = false;
            NodePtr arg = placeable(&f);
            if (!ok_) return nullptr;
            if (!f) return fail("Expected a placeable after the script operator"), nullptr;
            if (isSub) {
                if (sub) return fail("Double subscript"), nullptr;
                sub = arg;
            } else {
                if (super) return fail("Double superscript"), nullptr;
                super = arg;
            }
            any = true;
        }
        if (!any) return nucleus;
        if (!nucleus) nucleus = std::make_shared<Node>(Kind::Hbox);  // Hbox(0)
        return layoutSubsuper(nucleus, sub, super);
    }

    // The positioning half of Parser.subsuper (no over/under symbols, apostrophes or drop-subscripts yet)
    NodePtr layoutSubsuper(NodePtr nucleus, NodePtr sub, NodePtr super) {
        State& st = state();
        DejaVuSansFonts& fs = *st.fontset;
        const double ruleThickness = fs.underlineThickness(st.fontsize, st.dpi);
        const double xHeight = fs.xheight(st.fontsize, st.dpi);
        Node* lastChar = nucleus.get();
        std::shared_ptr<HlistNode> nucleusList;
        if (nucleus->kind == Kind::Hlist) {
            auto children = static_cast<HlistNode&>(*nucleus).children;
            if (!children.empty()) {
                if (children.size() >= 2 && children.back()->kind == Kind::Kern &&
                    children[children.size() - 2]->kind == Kind::Char)
                    children.pop_back();
                lastChar = children.back().get();
                if (lastChar->kind == Kind::Char) lastChar->width = static_cast<CharNode*>(lastChar)->metrics.advance;
            }
            nucleusList = hlist(children, false);
        } else {
            if (nucleus->kind == Kind::Char) lastChar->width = static_cast<CharNode*>(lastChar)->metrics.advance;
            nucleusList = hlist({nucleus});
        }
        if (!ok_) return nullptr;
        const double lcHeight = lastChar->height;
        double superkern = DejaVuSansFonts::kDelta * xHeight;
        double subkern = DejaVuSansFonts::kDelta * xHeight;
        const bool slanted = lastChar->kind == Kind::Char && static_cast<CharNode*>(lastChar)->metrics.slanted;
        if (slanted) {
            superkern += DejaVuSansFonts::kDelta * xHeight;
            superkern += DejaVuSansFonts::kDeltaSlanted * (lcHeight - xHeight * 2 / 3);
            subkern = 0;
        }
        double shiftUp = 0.0, shiftDown = 0.0;
        if (!nucleusList->isCharNode()) {
            const double shrunkXHeight = fs.xheight(st.fontsize * kShrinkFactor, st.dpi);
            shiftUp = nucleusList->height - DejaVuSansFonts::kSupdrop * shrunkXHeight;
            shiftDown = nucleusList->depth + DejaVuSansFonts::kSubdrop * shrunkXHeight;
        }
        std::shared_ptr<ListNode> x;
        if (!super) {
            auto h = hlist({std::make_shared<KernNode>(subkern), sub});
            h->shrink();
            shiftDown = pyMax(pyMax(shiftDown, DejaVuSansFonts::kSub1 * xHeight), h->height - xHeight * 4 / 5);
            h->shiftAmount = shiftDown;
            x = h;
        } else {
            auto h = hlist({std::make_shared<KernNode>(superkern), super});
            h->shrink();
            shiftUp = pyMax(pyMax(shiftUp, DejaVuSansFonts::kSup1 * xHeight), h->depth + xHeight / 4);
            if (!sub) {
                h->shiftAmount = -shiftUp;
                x = h;
            } else {
                auto y = hlist({std::make_shared<KernNode>(subkern), sub});
                y->shrink();
                shiftDown = pyMax(shiftDown, DejaVuSansFonts::kSub2 * xHeight);
                double clr = (4 * ruleThickness - ((shiftUp - h->depth) - (y->height - shiftDown)));
                if (clr > 0.) {
                    shiftDown += clr;
                    clr = xHeight * 4 / 5 - shiftUp + h->depth;
                    if (clr > 0) {
                        shiftUp += clr;
                        shiftDown -= clr;
                    }
                }
                auto v = std::make_shared<VlistNode>(std::vector<NodePtr>{
                    h, std::make_shared<KernNode>((shiftUp - h->depth) - (y->height - shiftDown)), y});
                v->shiftAmount = shiftDown;
                x = v;
            }
        }
        x->width += DejaVuSansFonts::kScriptSpace * xHeight;
        return hlist({nucleusList, x});
    }

    const rc::Params& rc_;
    std::vector<State> states_;
    std::u32string s_;
    std::size_t pos_ = 0;
    bool ok_ = true;
    bool fontOnly_ = false;
    std::map<std::tuple<std::string, double, double>, double> emWidth_;
};

// --- ship ----------------------------------------------------------------------------------------

struct Shipped {
    std::vector<std::pair<std::array<double, 2>, CharNode*>> glyphs;
};

void ship(HlistNode& box, Shipped& out) {
    double curV = 0., curH = 0.;
    const double offH = 0., offV = box.height;
    std::function<void(HlistNode&)> hlistOut;
    std::function<void(VlistNode&)> vlistOut;
    hlistOut = [&](HlistNode& b) {
        const double baseLine = curV;
        for (const auto& p : b.children) {
            if (p->kind == Kind::Char) {
                out.glyphs.push_back({{curH + offH, curV + offV}, static_cast<CharNode*>(p.get())});
                curH += p->width;
            } else if (p->kind == Kind::Kern) {
                curH += p->width;
            } else if (p->isList()) {
                auto& l = static_cast<ListNode&>(*p);
                if (l.children.empty()) {
                    curH += p->width;
                } else {
                    const double edge = curH;
                    curV = baseLine + l.shiftAmount;
                    if (p->kind == Kind::Hlist) hlistOut(static_cast<HlistNode&>(*p));
                    else vlistOut(static_cast<VlistNode&>(*p));
                    curH = edge + p->width;
                    curV = baseLine;
                }
            } else if (p->isBox()) {
                curH += p->width;  // (no rules yet: Hbox and Vbox have no height and width)
            }
        }
    };
    vlistOut = [&](VlistNode& b) {
        const double leftEdge = curH;
        curV -= b.height;
        for (const auto& p : b.children) {
            if (p->kind == Kind::Kern) {
                curV += p->width;
            } else if (p->isList()) {
                auto& l = static_cast<ListNode&>(*p);
                if (l.children.empty()) {
                    curV += p->height + p->depth;
                } else {
                    curV += p->height;
                    curH = leftEdge + l.shiftAmount;
                    const double saveV = curV;
                    p->width = b.width;
                    if (p->kind == Kind::Hlist) hlistOut(static_cast<HlistNode&>(*p));
                    else vlistOut(static_cast<VlistNode&>(*p));
                    curV = saveV + p->depth;
                    curH = leftEdge;
                }
            }
        }
    };
    hlistOut(box);
}

}  // namespace

std::optional<MathParse> parseMath(FontCache& fonts, const std::string& s, double dpi, const FontProperties& prop,
                                   const rc::Params& rc, std::string* error) {
    if (rc.text("mathtext.fontset") != "dejavusans") {
        *error = "mathtext.fontset '" + rc.text("mathtext.fontset") + "' is not supported yet";
        return std::nullopt;
    }
    DejaVuSansFonts fontset(fonts, prop, rc, FT_LOAD_NO_HINTING);  // vector output: unhinted
    Parser parser(fontset, prop.size, dpi, rc);
    const auto box = parser.parse(codepoints(s));
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
    for (const auto& [at, ch] : shipped.glyphs) {
        std::string ignored;
        const auto info = ch->state.fontset->info(ch->state.font, ch->state.fontClass, ch->c, ch->state.fontsize,
                                                  ch->state.dpi, &ignored);
        out.glyphs.push_back(MathGlyph{info->font, info->file, info->fontsize, info->num, info->glyphIndex, at[0],
                                       h - at[1] + info->offset});
    }
    return out;
}

}  // namespace calculate_plot::text
