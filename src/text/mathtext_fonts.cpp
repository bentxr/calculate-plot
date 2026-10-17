// Derived from Matplotlib 3.11.2: lib/matplotlib/_mathtext.py (Fonts, TruetypeFonts, BakomaFonts,
// UnicodeFonts, DejaVuFonts, StixFonts, the font constants) and lib/matplotlib/_mathtext_data.py.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "text/mathtext_fonts.hpp"

#include "rc/rc_params.hpp"
#include "support/embedded.hpp"
#include "support/json.hpp"

#include <freetype/ftglyph.h>

#include <algorithm>
#include <array>

namespace calculate_plot::text::mathtext {

using support::Json;

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

namespace {

const Json& mathData() {
    static const Json doc = [] {
        auto parsed = support::parseJson(embedded::file("data/mathtext_data.json"), nullptr);
        return parsed ? *parsed : Json();
    }();
    return doc;
}

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

using Ranges = std::vector<std::array<unsigned, 2>>;

const Ranges& unicodeRanges(const char* name) {
    static const std::map<std::string, Ranges> tables = [] {
        std::map<std::string, Ranges> out;
        const auto doc = support::parseJson(embedded::file("data/unicode_letters.json"), nullptr);
        for (const char* key : {"letters", "greek_capital", "digits"})
            if (doc && doc->find(key))
                for (const Json& r : doc->find(key)->items()) out[key].push_back({unsigned(r[0].asInteger()), unsigned(r[1].asInteger())});
        return out;
    }();
    static const Ranges none;
    const auto it = tables.find(name);
    return it == tables.end() ? none : it->second;
}

bool inRanges(const Ranges& ranges, unsigned c) {
    auto it = std::upper_bound(ranges.begin(), ranges.end(), c, [](unsigned v, const std::array<unsigned, 2>& r) { return v < r[0]; });
    return it != ranges.begin() && c <= (*(it - 1))[1];
}

// latex_to_bakoma: symbol -> (font name, glyph number)
const std::map<std::u32string, std::pair<std::string, unsigned>>& latexToBakoma() {
    static const std::map<std::u32string, std::pair<std::string, unsigned>> table = [] {
        std::map<std::u32string, std::pair<std::string, unsigned>> out;
        if (const Json* t = mathData().find("latex_to_bakoma"))
            for (const auto& [sym, value] : t->members()) out[codepoints(sym)] = {value[0].asString(), unsigned(value[1].asInteger())};
        return out;
    }();
    return table;
}

// One range of a STIX virtual font: code points first..last are those from `to` on in the font `font`.
struct VirtualRange {
    unsigned first, last;
    std::string font;
    unsigned to;
};
using VirtualRanges = std::vector<VirtualRange>;

// stix_virtual_fonts: font -> class ("" where the font has one list for every class) -> ranges
const std::map<std::string, std::map<std::string, VirtualRanges>>& stixVirtualFonts() {
    static const std::map<std::string, std::map<std::string, VirtualRanges>> table = [] {
        std::map<std::string, std::map<std::string, VirtualRanges>> out;
        const auto read = [](const Json& list) {
            VirtualRanges ranges;
            for (const Json& r : list.items())
                ranges.push_back({unsigned(r[0].asInteger()), unsigned(r[1].asInteger()), r[2].asString(), unsigned(r[3].asInteger())});
            return ranges;
        };
        if (const Json* t = mathData().find("stix_virtual_fonts"))
            for (const auto& [font, mapping] : t->members()) {
                if (mapping.isArray()) out[font][""] = read(mapping);
                else
                    for (const auto& [cls, list] : mapping.members()) out[font][cls] = read(list);
            }
        return out;
    }();
    return table;
}

const std::map<unsigned, unsigned>& stixGlyphFixes() {
    static const std::map<unsigned, unsigned> table = [] {
        std::map<unsigned, unsigned> out;
        if (const Json* t = mathData().find("stix_glyph_fixes"))
            for (const auto& [from, to] : t->members()) out[unsigned(std::stoul(from))] = unsigned(to.asInteger());
        return out;
    }();
    return table;
}

bool isSlantedSymbol(const std::u32string& sym) { return sym == U"\\int" || sym == U"\\oint"; }  // _slanted_symbols

// --- the constants of each fontset ---------------------------------------------------------------

FontConstants computerModernConstants() {
    FontConstants c;
    const double x = 451470;
    c.scriptSpace = 0.132861328125, c.delta = 0.132861328125, c.deltaSlanted = 0.3, c.deltaIntegral = 0.3;
    c.supdrop = 404864 / x, c.subdrop = 52429 / x, c.sup1 = 432949 / x, c.sub1 = 157286 / x, c.sub2 = 259226 / x;
    c.num1 = 709370 / x, c.num2 = 412858 / x, c.num3 = 465286 / x, c.denom1 = 719272 / x, c.denom2 = 361592 / x;
    c.axisHeight = 262144.0 / 1048576, c.quad = 1048579.0 / 1048576, c.xHeight = x / 1048576;
    return c;
}

FontConstants stixConstants(bool sans) {
    FontConstants c;
    const double x = 450;
    c.scriptSpace = 0.1, c.delta = 0.05, c.deltaSlanted = 0.3, c.deltaIntegral = 0.3;
    c.xHeight = x / 1000;
    c.supdrop = 386 / x, c.subdrop = 50.0002 / x, c.sup1 = 413 / x, c.sub1 = 150 / x, c.sub2 = 309 / x;
    c.num1 = 747 / x, c.num2 = 424 / x, c.num3 = 474 / x, c.denom1 = 756 / x, c.denom2 = 375 / x;
    c.axisHeight = 250.0 / 1000, c.quad = 1000.0 / 1000;
    if (sans) c.scriptSpace = 0.05, c.deltaSlanted = 0.6, c.deltaIntegral = 0.3;
    return c;
}

FontConstants dejavuConstants(bool serif) {
    FontConstants c;
    const double x = serif ? 1063 : 1120;
    c.xHeight = x / 2048;
    c.supdrop = 790.527 / x, c.subdrop = 102.4 / x, c.sup1 = 845.824 / x, c.sub1 = 307.199 / x, c.sub2 = 632.832 / x;
    c.num1 = 1529.86 / x, c.num2 = 868.352 / x, c.num3 = 970.752 / x, c.denom1 = 1548.29 / x, c.denom2 = 768 / x;
    c.axisHeight = 512.0 / 2048;
    return c;
}

// --- the fontsets --------------------------------------------------------------------------------

class UnicodeFonts : public Fonts {
public:
    // The 'custom' fontset: the fonts of rcParams["mathtext.rm"] and the like, with a fallback.
    UnicodeFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags);
    bool lookup(const std::string& fontname, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                std::string* error) override;
    std::vector<std::pair<std::string, std::u32string>> sizedAlternatives(const std::string& fontname,
                                                                         const std::u32string& sym) override {
        if (fallback_) return fallback_->sizedAlternatives(fontname, sym);
        return Fonts::sizedAlternatives(fontname, sym);
    }

protected:
    struct NoFontMap {};
    UnicodeFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags, NoFontMap)
        : Fonts(cache, defaultProp, rc, flags) {}
    virtual std::pair<std::string, unsigned> mapVirtualFont(const std::string& fontname, const std::string&, unsigned uniindex) {
        return {fontname, uniindex};
    }
    std::unique_ptr<Fonts> fallback_;
};

class StixFonts : public UnicodeFonts {
public:
    StixFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags, bool sans)
        : UnicodeFonts(cache, defaultProp, rc, flags, NoFontMap{}), sans_(sans), constants_(stixConstants(sans)) {
        const std::pair<const char*, const char*> fontmap[] = {
            {"rm", "STIXGeneral"},        {"it", "STIXGeneral:italic"},        {"bf", "STIXGeneral:weight=bold"},
            {"bfit", "STIXGeneral:italic:bold"}, {"nonunirm", "STIXNonUnicode"}, {"nonuniit", "STIXNonUnicode:italic"},
            {"nonunibf", "STIXNonUnicode:weight=bold"}, {"0", "STIXGeneral"},  {"1", "STIXSizeOneSym"},
            {"2", "STIXSizeTwoSym"},      {"3", "STIXSizeThreeSym"},           {"4", "STIXSizeFourSym"},
            {"5", "STIXSizeFiveSym"}};
        for (const auto& [key, name] : fontmap) mapFont(key, name);
    }
    bool isStix() const override { return true; }
    const FontConstants& constants() const override { return constants_; }

    std::vector<std::pair<std::string, std::u32string>> sizedAlternatives(const std::string& fontname,
                                                                         const std::u32string& symIn) override {
        std::u32string sym = symIn;
        if (sym == U"\\{") sym = U"{";
        else if (sym == U"\\}") sym = U"}";
        else if (sym == U"\\[") sym = U"[";
        else if (sym == U"\\]") sym = U"]";
        else if (sym == U"<") sym = U"⟨";  // MATHEMATICAL LEFT ANGLE BRACKET
        else if (sym == U">") sym = U"⟩";
        const auto uni = unicodeIndex(sym);
        if (!uni) return {{fontname, sym}};
        std::vector<std::pair<std::string, std::u32string>> alternatives;
        for (int i = 0; i < 6; ++i)
            if (hasGlyph(fontFile(std::to_string(i)), *uni)) alternatives.emplace_back(std::to_string(i), std::u32string(1, char32_t(*uni)));
        // the largest radical does not join well with the rule
        if (sym == U"\\__sqrt__" && !alternatives.empty()) alternatives.pop_back();
        return alternatives;
    }

protected:
    std::pair<std::string, unsigned> mapVirtualFont(const std::string& fontnameIn, const std::string& fontClass,
                                                    unsigned uniindex) override {
        std::string fontname = fontnameIn;
        const auto& fonts = stixVirtualFonts();
        auto mappingOf = fonts.find(fontname);
        bool doingSansConversion = false;
        if (sans_ && mappingOf == fonts.end() && fontname != "regular" && fontname != "default") {
            mappingOf = fonts.find("sf");
            doingSansConversion = true;
        }
        const VirtualRanges* mapping = nullptr;
        if (mappingOf != fonts.end()) {
            const auto& byClass = mappingOf->second;
            if (byClass.count("")) mapping = &byClass.at("");  // one list for every class
            else if (byClass.count(fontClass)) mapping = &byClass.at(fontClass);
            else mapping = &byClass.at("rm");
        }
        if (mapping && !mapping->empty()) {
            // Binary search for the source glyph
            std::size_t lo = 0, hi = mapping->size();
            const VirtualRange* range = nullptr;
            while (lo < hi) {
                const std::size_t mid = (lo + hi) / 2;
                range = &(*mapping)[mid];
                if (uniindex < range->first) hi = mid;
                else if (uniindex <= range->last) break;
                else lo = mid + 1;
            }
            if (range->first <= uniindex && uniindex <= range->last) {
                uniindex = uniindex - range->first + range->to;
                fontname = range->font;
            } else if (!doingSansConversion) {
                // This will generate a dummy character
                uniindex = 0x1;
                fontname = rc_.text("mathtext.default");
            }
        }
        // Fix some incorrect glyphs.
        if (fontname == "rm" || fontname == "it" || fontname == "normal") {
            const auto fix = stixGlyphFixes().find(uniindex);
            if (fix != stixGlyphFixes().end()) uniindex = fix->second;
        }
        // Handle private use area glyphs
        if ((fontname == "it" || fontname == "rm" || fontname == "bf" || fontname == "bfit") && 0xe000 <= uniindex && uniindex <= 0xf8ff)
            fontname = "nonuni" + fontname;
        return {fontname, uniindex};
    }

private:
    bool sans_;
    FontConstants constants_;
};

class BakomaFonts : public Fonts {
public:
    BakomaFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags)
        : Fonts(cache, defaultProp, rc, flags),
          stixFallback_(std::make_unique<StixFonts>(cache, defaultProp, rc, flags, false)),
          constants_(computerModernConstants()) {
        const std::pair<const char*, const char*> fontmap[] = {{"normal", "cmmi10"}, {"cal", "cmsy10"}, {"rm", "cmr10"},
                                                               {"tt", "cmtt10"},     {"it", "cmti10"},  {"bf", "cmb10"},
                                                               {"sf", "cmss10"},     {"ex", "cmex10"}};
        for (const auto& [key, name] : fontmap) mapFont(key, name);
    }
    const FontConstants& constants() const override { return constants_; }

    bool lookup(const std::string& fontnameIn, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                std::string* error) override {
        std::string fontname = fontnameIn, file;
        unsigned num = 0;
        bool slanted = false;
        const auto known = latexToBakoma().find(sym);
        if (fontmap_.count(fontname) && known != latexToBakoma().end()) {
            const std::string& basename = known->second.first;
            num = known->second.second;
            slanted = basename == "cmmi10" || basename == "cmti10" || isSlantedSymbol(sym);
            file = fontFile(basename);
        } else if (sym.size() == 1) {
            slanted = fontname == "it" || fontname == "normal";
            if (fontname == "normal" && isDigit(unsigned(sym[0]))) {
                fontname = "rm";
                slanted = false;
            }
            file = fontFile(fontname);
            num = unsigned(sym[0]);
        }
        if (hasGlyph(file, num)) {
            *out = {file, num, slanted};
            return true;
        }
        return lookupIn(*stixFallback_, fontname, fontClass, sym, out, error);
    }

    // The Computer Modern sizes of the delimiters, the radical and the wide accents
    std::vector<std::pair<std::string, std::u32string>> sizedAlternatives(const std::string& fontname,
                                                                         const std::u32string& symIn) override {
        std::u32string sym = symIn;
        static const std::map<std::u32string, std::u32string> aliases = {
            {U"\\leftparen", U"("},   {U"\\rightparen", U")"},   {U"\\leftbrace", U"{"}, {U"\\rightbrace", U"}"},
            {U"\\leftbracket", U"["}, {U"\\rightbracket", U"]"}, {U"\\langle", U"<"},    {U"\\rangle", U">"},
            {U"\\{", U"{"},           {U"\\}", U"}"},            {U"\\[", U"["},         {U"\\]", U"]"}};
        if (const auto alias = aliases.find(sym); alias != aliases.end()) sym = alias->second;
        using Alternatives = std::vector<std::pair<std::string, std::u32string>>;
        const auto sized = [](const char* name) {
            Alternatives out;
            for (const char* size : {"big", "Big", "bigg", "Bigg"}) out.emplace_back("ex", codepoints(std::string("\\__") + name + size + "__"));
            return out;
        };
        const auto with = [](std::pair<std::string, std::u32string> first, Alternatives rest) {
            rest.insert(rest.begin(), std::move(first));
            return rest;
        };
        if (sym == U"(") return with({"rm", U"("}, sized("parenleft"));
        if (sym == U")") return with({"rm", U")"}, sized("parenright"));
        if (sym == U"{") return sized("braceleft");
        if (sym == U"}") return sized("braceright");
        if (sym == U"[") return with({"rm", U"["}, sized("bracketleft"));
        if (sym == U"]") return with({"rm", U"]"}, sized("bracketright"));
        if (sym == U"<") return with({"cal", U"\\__angbracketleft__"}, sized("angbracketleft"));
        if (sym == U">") return with({"cal", U"\\__angbracketright__"}, sized("angbracketright"));
        if (sym == U"\\lfloor") return sized("floorleft");
        if (sym == U"\\rfloor") return sized("floorright");
        if (sym == U"\\lceil") return sized("ceilingleft");
        if (sym == U"\\rceil") return sized("ceilingright");
        if (sym == U"\\__sqrt__") return sized("radical");
        if (sym == U"\\backslash") return sized("backslash");
        if (sym == U"/") return with({"rm", U"/"}, sized("slash"));
        if (sym == U"\\widehat")
            return {{"rm", U"\x5e"}, {"ex", U"\\__hatwide__"}, {"ex", U"\\__hatwider__"}, {"ex", U"\\__hatwidest__"}};
        if (sym == U"\\widetilde")
            return {{"rm", U"\x7e"}, {"ex", U"\\__tildewide__"}, {"ex", U"\\__tildewider__"}, {"ex", U"\\__tildewidest__"}};
        return {{fontname, symIn}};
    }

private:
    std::unique_ptr<Fonts> stixFallback_;
    FontConstants constants_;
};

UnicodeFonts::UnicodeFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags)
    : Fonts(cache, defaultProp, rc, flags) {
    const auto& fallback = rc.value("mathtext.fallback");
    const std::string name = fallback.isString() ? fallback.asString() : std::string();
    if (name == "stix") fallback_ = std::make_unique<StixFonts>(cache, defaultProp, rc, flags, false);
    else if (name == "stixsans") fallback_ = std::make_unique<StixFonts>(cache, defaultProp, rc, flags, true);
    else if (name == "cm") fallback_ = std::make_unique<BakomaFonts>(cache, defaultProp, rc, flags);
    for (const char* texfont : {"cal", "rm", "tt", "it", "bf", "sf", "bfit"})
        fontmap_[texfont] = findPattern(rc.text(std::string("mathtext.") + texfont));
    fontmap_["ex"] = findPattern("cmex10");
    // include STIX sized alternatives for glyphs if the fallback is STIX
    if (fallback_ && fallback_->isStix()) {
        const std::pair<const char*, const char*> sized[] = {{"0", "STIXGeneral"},      {"1", "STIXSizeOneSym"},
                                                             {"2", "STIXSizeTwoSym"},   {"3", "STIXSizeThreeSym"},
                                                             {"4", "STIXSizeFourSym"},  {"5", "STIXSizeFiveSym"}};
        for (const auto& [key, fontName] : sized) mapFont(key, fontName);
    }
}

bool UnicodeFonts::lookup(const std::string& fontnameIn, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                          std::string* error) {
    const auto index = unicodeIndex(sym);
    bool found = index.has_value();
    unsigned uniindex = found ? *index : unsigned('?');  // (matplotlib warns: no TeX to Unicode mapping)
    std::string fontname;
    std::tie(fontname, uniindex) = mapVirtualFont(fontnameIn, fontClass, uniindex);
    std::string newFontname = fontname, file;
    bool slanted = false;
    // Only characters in the "Letter" class should be italicized in 'it' mode. Greek capital letters
    // should be Roman.
    if (found) {
        if (fontname == "normal" && uniindex < 0x10000) newFontname = (!isLetter(uniindex) || isGreekCapital(uniindex)) ? "rm" : "it";
        slanted = newFontname == "it" || isSlantedSymbol(sym);
        found = false;
        file = fontFile(newFontname);
        // cmr10 has the multiplication and the minus signs of cmsy10 elsewhere
        FT2Font* font = file.empty() ? nullptr : open(file);
        if ((uniindex == 0x00D7 || uniindex == 0x2212) && font && std::string(font->get_face()->family_name) == "cmr10") {
            file = "cmsy10.ttf";
            uniindex = uniindex == 0x00D7 ? 0x00A3 : 0x00A1;
        }
        found = hasGlyph(file, uniindex);
    }
    if (!found) {
        if (fallback_) {
            if ((fontname == "it" || fontname == "regular" || fontname == "normal") && fallback_->isStix()) fontname = "rm";
            return lookupIn(*fallback_, fontname, fontClass, sym, out, error);
        }
        if ((fontname == "it" || fontname == "regular" || fontname == "normal") && isStix())
            return glyph("rm", fontClass, sym, out, error);
        // (matplotlib warns and substitutes a dummy symbol: the currency sign)
        file = fontFile("rm");
        uniindex = 0xA4;
        slanted = false;
    }
    *out = {file, uniindex, slanted};
    return true;
}

class DejaVuFonts : public UnicodeFonts {
public:
    DejaVuFonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags, bool serif)
        : UnicodeFonts(cache, defaultProp, rc, flags, NoFontMap{}),
          bakoma_(std::make_unique<BakomaFonts>(cache, defaultProp, rc, flags)),
          constants_(dejavuConstants(serif)) {
        fallback_ = std::make_unique<StixFonts>(cache, defaultProp, rc, flags, !serif);
        const std::string family = serif ? "DejaVu Serif" : "DejaVu Sans";
        const std::pair<std::string, std::string> fontmap[] = {
            {"rm", family},          {"it", family + ":italic"}, {"bf", family + ":weight=bold"},
            {"bfit", family + ":italic:bold"}, {"sf", "DejaVu Sans"},  {"tt", "DejaVu Sans Mono"},
            {"ex", family + " Display"},       {"0", family},          {"1", "STIXSizeOneSym"},
            {"2", "STIXSizeTwoSym"}, {"3", "STIXSizeThreeSym"},  {"4", "STIXSizeFourSym"},
            {"5", "STIXSizeFiveSym"}};
        for (const auto& [key, name] : fontmap) mapFont(key, name);
    }
    const FontConstants& constants() const override { return constants_; }

    bool lookup(const std::string& fontname, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                std::string* error) override {
        // Override prime symbol to use Bakoma.
        if (sym == U"\\prime") return lookupIn(*bakoma_, fontname, fontClass, sym, out, error);
        // check whether the glyph is available in the display font
        const auto uniindex = unicodeIndex(sym);
        if (!uniindex) {
            if (error) *error = "not a valid Unicode character or TeX/Type1 symbol";
            return false;
        }
        const std::string display = fontFile("ex");
        if (hasGlyph(display, *uniindex)) return UnicodeFonts::lookup("ex", fontClass, sym, out, error);
        // otherwise return regular glyph
        return UnicodeFonts::lookup(fontname, fontClass, sym, out, error);
    }

private:
    std::unique_ptr<Fonts> bakoma_;
    FontConstants constants_;
};

}  // namespace

// --- tables --------------------------------------------------------------------------------------

std::optional<unsigned> unicodeIndex(const std::u32string& sym) {
    if (sym.size() == 1) return unsigned(sym[0]);
    // symbol.strip("\\"): a name comes with one backslash, in front
    std::string name;
    for (std::size_t i = sym[0] == U'\\' ? 1 : 0; i < sym.size(); ++i) name += utf8(sym[i]);
    const auto it = tex2uni().find(name);
    if (it == tex2uni().end()) return std::nullopt;
    return it->second;
}

bool isTexName(const std::string& name) { return tex2uni().count(name) != 0; }

const std::set<std::u32string>& symbolSet(const std::string& name) {
    static std::map<std::string, std::set<std::u32string>> sets;
    auto it = sets.find(name);
    if (it == sets.end()) {
        std::set<std::u32string> s;
        if (const Json* list = mathData().find(name))
            for (const Json& v : list->items()) s.insert(codepoints(v.asString()));
        it = sets.emplace(name, std::move(s)).first;
    }
    return it->second;
}

const std::map<std::string, double>& spaceWidths() {
    static const std::map<std::string, double> table = [] {
        std::map<std::string, double> out;
        if (const Json* t = mathData().find("space_widths"))
            for (const auto& [name, width] : t->members()) out[name] = width.asNumber();
        return out;
    }();
    return table;
}

const std::map<std::string, std::string>& accentMap() {
    static const std::map<std::string, std::string> table = [] {
        std::map<std::string, std::string> out;
        if (const Json* t = mathData().find("accent_map"))
            for (const auto& [name, symbol] : t->members()) out[name] = symbol.asString();
        return out;
    }();
    return table;
}

bool isLetter(unsigned c) { return inRanges(unicodeRanges("letters"), c); }
bool isGreekCapital(unsigned c) { return inRanges(unicodeRanges("greek_capital"), c); }
bool isDigit(unsigned c) { return inRanges(unicodeRanges("digits"), c); }

// --- Fonts (TruetypeFonts) -----------------------------------------------------------------------

Fonts::Fonts(FontCache& cache, const FontProperties& defaultProp, const rc::Params& rc, FT_Int32 flags)
    : cache_(cache), rc_(rc), flags_(flags), defaultProp_(defaultProp), defaultFile_(findFont(defaultProp, rc)) {}

// findfont(pattern): the font of a fontconfig pattern. Where FontProperties refuses the pattern, and
// for one that names a font file, the fontset fails (matplotlib raises as the fontset is made; a
// file it would open, or pass over if it is not there).
std::string Fonts::findPattern(const std::string& pattern) {
    FontProperties prop;
    std::optional<std::string> file;
    std::string error;
    if (!fontPropertiesFromPattern(pattern, rc_, &prop, &file, &error)) {
        if (failure_.empty()) failure_ = "the font '" + pattern + "': " + error;
        return {};
    }
    if (file) {
        if (failure_.empty()) failure_ = "the font '" + pattern + "': a font file in a pattern is not supported";
        return {};
    }
    return findFont(prop, rc_);
}

void Fonts::mapFont(const std::string& key, const std::string& pattern) {
    const std::string file = findPattern(pattern);
    fontmap_[key] = file;
    fontmap_[pattern] = file;
}

std::string Fonts::fontFile(const std::string& font) const {
    const auto it = fontmap_.find(font);
    const std::string& basename = it == fontmap_.end() ? font : it->second;
    if (basename == "default" || basename == "regular") return defaultFile_;
    return it == fontmap_.end() ? std::string() : basename;
}

bool Fonts::hasGlyph(const std::string& file, unsigned num) {
    if (file.empty()) return false;
    FT2Font* font = open(file);
    // (get_char_index's fallback is on in Python: a character that a text has loaded from one of the
    // font's fallbacks counts as found; nothing drawn with Agg or SVG loads one that way)
    return font && font->get_char_index(num, true) != 0;
}

FT2Font* Fonts::open(const std::string& file) {
    FT2Font* font = cache_.get({file});
    if (!font && failure_.empty()) failure_ = "the font " + file + " cannot be opened";
    return font;
}

// The lookups go on as if a font that cannot be opened had no glyph (one of them, in STIX, by calling
// itself with another font): once that has happened no glyph is given any more.
bool Fonts::glyph(const std::string& fontname, const std::string& fontClass, const std::u32string& sym, Glyph* out,
                  std::string* error) {
    const bool found = failure_.empty() && lookup(fontname, fontClass, sym, out, error);
    if (failure_.empty()) return found;
    if (error) *error = failure_;
    return false;
}

bool Fonts::lookupIn(Fonts& other, const std::string& fontname, const std::string& fontClass, const std::u32string& sym,
                     Glyph* out, std::string* error) {
    const bool found = other.glyph(fontname, fontClass, sym, out, error);
    if (failure_.empty()) failure_ = other.failure_;
    return found;
}

const FontConstants& Fonts::constants() const {
    static const FontConstants base;
    return base;
}

std::vector<std::pair<std::string, std::u32string>> Fonts::sizedAlternatives(const std::string& fontname,
                                                                            const std::u32string& sym) {
    return {{fontname, sym}};
}

std::optional<FontInfo> Fonts::info(const std::string& fontname, const std::string& fontClass, const std::u32string& sym,
                                    double fontsize, double dpi, std::string* error) {
    const auto key = std::make_tuple(fontname, fontClass, sym, fontsize, dpi);
    const auto cached = infos_.find(key);
    if (cached != infos_.end()) return cached->second;
    Glyph g;
    if (!glyph(fontname, fontClass, sym, &g, error)) return std::nullopt;
    FT2Font* font = open(g.file);
    if (!font) {
        if (error) *error = failure_;
        return std::nullopt;
    }
    font->set_size(fontsize, dpi);
    const FT_UInt glyphIndex = font->get_char_index(g.num, true);
    font->load_glyph(glyphIndex, flags_);
    const FT_Face face = font->get_face();
    FT_BBox bbox;
    FT_Glyph_Get_CBox(font->get_glyphs().back(), FT_GLYPH_BBOX_SUBPIXELS, &bbox);
    // _get_offset: the glyphs of cmex10 hang from their reference point
    double offset = 0.;
    const char* postscript = FT_Get_Postscript_Name(face);
    if (postscript && std::string(postscript) == "Cmex10")
        offset = (double(face->glyph->metrics.height) / 64 / 2) + (fontsize / 3 * dpi / 72);
    FontInfo info{font,
                  g.file,
                  fontsize,
                  g.num,
                  glyphIndex,
                  FontMetrics{double(face->glyph->linearHoriAdvance) / 65536, double(face->glyph->metrics.height) / 64,
                              double(face->glyph->metrics.width) / 64, double(bbox.xMin) / 64, double(bbox.xMax) / 64,
                              double(bbox.yMin) / 64 + offset, double(bbox.yMax) / 64 + offset,
                              double(face->glyph->metrics.horiBearingY) / 64 + offset, g.slanted},
                  offset};
    infos_.emplace(key, info);
    return info;
}

std::optional<double> Fonts::axisHeight(const std::string& fontname, double fontsize, double dpi, std::string* error) {
    if (constants().axisHeight) return *constants().axisHeight * fontsize * dpi / 72;
    const auto i = info(fontname, rc_.text("mathtext.default"), U"−", fontsize, dpi, error);
    if (!i) return std::nullopt;
    return (i->metrics.ymax + i->metrics.ymin) / 2;
}

std::optional<double> Fonts::quad(const std::string& fontname, double fontsize, double dpi, std::string* error) {
    if (constants().quad) return *constants().quad * fontsize * dpi / 72;
    const auto i = info(fontname, rc_.text("mathtext.default"), U"m", fontsize, dpi, error);
    if (!i) return std::nullopt;
    return i->metrics.advance;
}

std::optional<double> Fonts::xheight(const std::string& fontname, double fontsize, double dpi, std::string* error) {
    if (constants().xHeight) return *constants().xHeight * fontsize * dpi / 72;
    const auto i = info(fontname, rc_.text("mathtext.default"), U"x", fontsize, dpi, error);
    if (!i) return std::nullopt;
    return i->metrics.iceberg;
}

std::optional<double> Fonts::kern(const std::string& font1, const std::string& class1, const std::u32string& sym1,
                                  double size1, const std::string& font2, const std::string& class2,
                                  const std::u32string& sym2, double size2, double dpi, std::string* error) {
    if (font1 == font2 && size1 == size2) {
        const auto i1 = info(font1, class1, sym1, size1, dpi, error);
        const auto i2 = info(font2, class2, sym2, size2, dpi, error);
        if (!i1 || !i2) return std::nullopt;
        return double(i1->font->get_kerning(i1->glyphIndex, i2->glyphIndex, FT_KERNING_DEFAULT)) / 64;
    }
    return 0.;
}

std::unique_ptr<Fonts> makeFontset(const std::string& name, FontCache& cache, const FontProperties& defaultProp,
                                   const rc::Params& rc, FT_Int32 flags) {
    if (name == "cm") return std::make_unique<BakomaFonts>(cache, defaultProp, rc, flags);
    if (name == "dejavuserif") return std::make_unique<DejaVuFonts>(cache, defaultProp, rc, flags, true);
    if (name == "dejavusans") return std::make_unique<DejaVuFonts>(cache, defaultProp, rc, flags, false);
    if (name == "stix") return std::make_unique<StixFonts>(cache, defaultProp, rc, flags, false);
    if (name == "stixsans") return std::make_unique<StixFonts>(cache, defaultProp, rc, flags, true);
    return std::make_unique<UnicodeFonts>(cache, defaultProp, rc, flags);  // 'custom'
}

}  // namespace calculate_plot::text::mathtext
