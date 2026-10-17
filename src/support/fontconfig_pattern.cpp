// Derived from Matplotlib 3.11.2: lib/matplotlib/_fontconfig_pattern.py (parse_fontconfig_pattern).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#include "support/fontconfig_pattern.hpp"

#include <algorithm>
#include <cstring>

namespace calculate_plot::support {

namespace {

// _CONSTANTS
struct Constant {
    const char* name;
    const char* property;
    const char* value;
};
const Constant kConstants[] = {
    {"thin", "weight", "light"},           {"extralight", "weight", "light"},
    {"ultralight", "weight", "light"},     {"light", "weight", "light"},
    {"book", "weight", "book"},            {"regular", "weight", "regular"},
    {"normal", "weight", "normal"},        {"medium", "weight", "medium"},
    {"demibold", "weight", "demibold"},    {"semibold", "weight", "semibold"},
    {"bold", "weight", "bold"},            {"extrabold", "weight", "extra bold"},
    {"black", "weight", "black"},          {"heavy", "weight", "heavy"},
    {"roman", "slant", "normal"},          {"italic", "slant", "italic"},
    {"oblique", "slant", "oblique"},       {"ultracondensed", "width", "ultra-condensed"},
    {"extracondensed", "width", "extra-condensed"}, {"condensed", "width", "condensed"},
    {"semicondensed", "width", "semi-condensed"},   {"expanded", "width", "expanded"},
    {"extraexpanded", "width", "extra-expanded"},   {"ultraexpanded", "width", "ultra-expanded"},
};

}  // namespace

// str.expandtabs(): a tab goes to the next column that is a multiple of eight, counting the
// characters (not the bytes of UTF-8) from the start of its line.
std::string expandTabs(const std::string& text) {
    std::string out;
    std::size_t column = 0;
    for (char c : text) {
        if (c == '\t') {
            const std::size_t spaces = 8 - column % 8;
            out.append(spaces, ' ');
            column += spaces;
        } else {
            out += c;
            if (c == '\n' || c == '\r') column = 0;
            else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++column;
        }
    }
    return out;
}

namespace {

// The pyparsing grammar by hand: every element skips the blanks before it, and an alternative that
// does not match leaves the position where it was.
class Parser {
public:
    explicit Parser(std::string s) : s_(std::move(s)) {}

    std::optional<FontconfigPattern> parse() {
        FontconfigPattern out;
        const auto values = [&](const std::string& name) -> std::vector<std::string>& {
            auto it = std::find_if(out.properties.begin(), out.properties.end(), [&](const auto& p) { return p.first == name; });
            if (it == out.properties.end()) it = out.properties.insert(out.properties.end(), {name, {}});
            return it->second;
        };
        // Optional(comma_separated(family)("families")): a family may be empty, so there is always one
        values("family").push_back(unescaped(word("\\-:,"), "\\-:,"));
        while (literal(',')) values("family").push_back(unescaped(word("\\-:,"), "\\-:,"));
        // Optional("-" + comma_separated(size)("sizes"))
        const std::size_t beforeSizes = i_;
        if (literal('-')) {
            std::string size;
            if (!number(&size)) {
                i_ = beforeSizes;
            } else {
                values("size").push_back(size);
                for (;;) {
                    const std::size_t before = i_;
                    if (literal(',') && number(&size)) {
                        values("size").push_back(size);
                        continue;
                    }
                    i_ = before;
                    break;
                }
            }
        }
        // ZeroOrMore(":" + prop("properties*"))
        for (;;) {
            const std::size_t before = i_;
            std::string name;
            std::vector<std::string> given;
            if (literal(':') && property(&name, &given)) {
                // props.setdefault(k, []).extend(v)
                auto& list = values(name);
                list.insert(list.end(), given.begin(), given.end());
                continue;
            }
            i_ = before;
            break;
        }
        skipBlanks();
        if (i_ != s_.size()) return std::nullopt;  // StringEnd()
        return out;
    }

private:
    void skipBlanks() {
        while (i_ < s_.size() && std::strchr(" \t\n\r", s_[i_])) ++i_;
    }
    bool literal(char c) {
        const std::size_t before = i_;
        skipBlanks();
        if (i_ < s_.size() && s_[i_] == c) return ++i_, true;
        i_ = before;
        return false;
    }
    // ([^punctuation]|(\\[punctuation]))*: as it stands, escapes kept
    std::string word(const char* punctuation) {
        skipBlanks();
        const std::size_t from = i_;
        while (i_ < s_.size()) {
            if (!std::strchr(punctuation, s_[i_])) ++i_;
            else if (s_[i_] == '\\' && i_ + 1 < s_.size() && std::strchr(punctuation, s_[i_ + 1])) i_ += 2;
            else break;
        }
        return s_.substr(from, i_ - from);
    }
    // re.sub(r'\\(?=[punctuation])', '', text): every backslash that stands before one of them goes
    static std::string unescaped(const std::string& text, const char* punctuation) {
        std::string out;
        for (std::size_t k = 0; k < text.size(); ++k)
            if (!(text[k] == '\\' && k + 1 < text.size() && std::strchr(punctuation, text[k + 1]))) out += text[k];
        return out;
    }
    // ([0-9]+\.?[0-9]*|\.[0-9]+)
    bool number(std::string* out) {
        const std::size_t before = i_;
        skipBlanks();
        const std::size_t from = i_;
        const auto digits = [&] {
            const std::size_t start = i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
            return i_ - start;
        };
        if (digits()) {
            if (i_ < s_.size() && s_[i_] == '.') {
                ++i_;
                digits();
            }
        } else if (i_ < s_.size() && s_[i_] == '.' && (++i_, digits())) {
        } else {
            i_ = before;
            return false;
        }
        *out = s_.substr(from, i_ - from);
        return true;
    }
    // Group((name + Suppress("=") + comma_separated(value)) | one_of(_CONSTANTS))
    bool property(std::string* name, std::vector<std::string>* values) {
        const std::size_t before = i_;
        skipBlanks();
        const std::size_t from = i_;
        while (i_ < s_.size() && s_[i_] >= 'a' && s_[i_] <= 'z') ++i_;
        if (i_ > from) {
            *name = s_.substr(from, i_ - from);
            if (literal('=')) {
                values->push_back(unescaped(word("\\=_:,"), "\\=_:,"));
                while (literal(',')) values->push_back(unescaped(word("\\=_:,"), "\\=_:,"));
                return true;
            }
        }
        // one_of: the longest constant that begins here (it need not end where a word ends)
        i_ = from;
        const Constant* best = nullptr;
        for (const Constant& c : kConstants)
            if (s_.compare(i_, std::strlen(c.name), c.name) == 0 && (!best || std::strlen(c.name) > std::strlen(best->name))) best = &c;
        if (!best) {
            i_ = before;
            return false;
        }
        i_ += std::strlen(best->name);
        *name = best->property;
        values->assign(1, best->value);
        return true;
    }

    const std::string s_;
    std::size_t i_ = 0;
};

}  // namespace

std::optional<FontconfigPattern> parseFontconfigPattern(const std::string& pattern) {
    return Parser(expandTabs(pattern)).parse();
}

}  // namespace calculate_plot::support
