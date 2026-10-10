// Derived from Matplotlib 3.11.2: src/ft2font.h, src/ft2font_wrapper.cpp.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

/* matplotlib's interface to FreeType, without Python */
#pragma once

#ifndef MPL_FT2FONT_H
#define MPL_FT2FONT_H

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

extern "C" {
#include <ft2build.h>
#include FT_BITMAP_H
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TYPE1_TABLES_H
#include FT_TRUETYPE_TABLES_H
}

#include <raqm.h>

// The one FreeType library object, as matplotlib's module-level _ft2Library.
FT_Library ft2Library();

// A rendered glyph: an 8-bit coverage bitmap and where its top-left corner goes (FreeType's
// bitmap_left/bitmap_top), as matplotlib's _PositionedBitmap.
struct PositionedBitmap {
    FT_Int left = 0;
    FT_Int top = 0;
    unsigned rows = 0;
    unsigned width = 0;
    std::vector<unsigned char> buffer;  // rows * width bytes
};

class FT2Font;

// One glyph of a laid-out string, as matplotlib's LayoutItem.
struct LayoutItem {
    FT2Font *ft_object;
    std::u32string character;
    int glyph_index;
    double x;
    double y;
    double prev_kern;
};

class FT2Font
{
  public:
    using LanguageRange = std::tuple<std::string, int, int>;
    using LanguageType = std::optional<std::vector<LanguageRange>>;

    FT2Font(std::vector<FT2Font *> &fallback_list, bool warn_if_used);
    virtual ~FT2Font();
    bool open(const std::string &path, FT_Long face_index);
    void close();
    void clear();
    void set_size(double ptsize, double dpi);
    void _set_transform(
        std::array<std::array<FT_Fixed, 2>, 2> matrix, std::array<FT_Fixed, 2> delta);
    void set_charmap(int i);
    void select_charmap(unsigned long i);
    std::vector<raqm_glyph_t> layout(std::u32string_view text, FT_Int32 flags,
                                     std::optional<std::vector<std::string>> features,
                                     LanguageType languages,
                                     std::set<FT_String*>& glyph_seen_fonts);
    void set_text(std::u32string_view codepoints, double angle, FT_Int32 flags,
                  std::optional<std::vector<std::string>> features,
                  LanguageType languages, std::vector<double> &xys);
    int get_kerning(FT_UInt left, FT_UInt right, FT_Kerning_Mode mode);
    void set_kerning_factor(int factor);
    void load_char(long charcode, FT_Int32 flags, FT2Font *&ft_object, bool fallback);
    bool load_char_with_fallback(FT2Font *&ft_object_with_glyph,
                                 FT_UInt &final_glyph_index,
                                 std::vector<FT_Glyph> &parent_glyphs,
                                 std::unordered_map<long, FT2Font *> &parent_char_to_font,
                                 long charcode,
                                 FT_Int32 flags,
                                 FT_Error &charcode_error,
                                 FT_Error &glyph_error,
                                 std::set<FT_String*> &glyph_seen_fonts);
    void load_glyph(FT_UInt glyph_index, FT_Int32 flags);
    std::tuple<long, long> get_width_height();
    std::tuple<long, long> get_bitmap_offset();
    long get_descent();
    std::string get_glyph_name(unsigned int glyph_number);
    long get_name_index(char *name);
    FT_UInt get_char_index(FT_ULong charcode, bool fallback);
    void get_path(std::vector<double> &vertices, std::vector<unsigned char> &codes);
    bool get_char_fallback_index(FT_ULong charcode, int& index) const;

    // From the Python wrapper: lay out a string with fallbacks (FT2Font._layout), and load and
    // render one glyph (FT2Font._render_glyph).
    std::vector<LayoutItem> layout_items(std::u32string_view text, FT_Int32 flags,
                                         std::optional<std::vector<std::string>> features,
                                         LanguageType languages);
    PositionedBitmap render_glyph(FT_UInt index, FT_Int32 flags, FT_Render_Mode mode);

    FT_Face const &get_face() const
    {
        return face;
    }
    std::vector<FT_Glyph> &get_glyphs()
    {
        return glyphs;
    }
    size_t get_num_glyphs() const
    {
        return glyphs.size();
    }
    FT_Bool has_kerning() const
    {
        return FT_HAS_KERNING(face);
    }
    const std::string &path() const
    {
        return m_path;
    }

    // Where matplotlib raises an exception, this port stops and keeps the reason here; missing
    // glyphs, which matplotlib reports as warnings, are listed in `warnings`.
    std::string error;
    std::vector<std::string> warnings;

  protected:
    virtual void ft_glyph_warn(FT_ULong charcode, std::set<FT_String*> family_names);
  private:
    void fail(const char *what, FT_Error code);

    bool warn_if_used;
    FT_Face face;
    FT_Vector pen;    /* untransformed origin  */
    std::vector<FT_Glyph> glyphs;
    std::vector<FT2Font *> fallbacks;
    std::unordered_map<long, FT2Font *> char_to_font;
    FT_BBox bbox;
    FT_Pos advance;
    int kerning_factor;
    std::string m_path;

    // prevent copying
    FT2Font(const FT2Font &);
    FT2Font &operator=(const FT2Font &);
};

#endif
