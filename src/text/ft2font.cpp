// Derived from Matplotlib 3.11.2: src/ft2font.cpp, src/ft2font_wrapper.cpp.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

/* -*- mode: c++; c-basic-offset: 4 -*- */

#include "text/ft2font.h"
#include "mplutils.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <cstdio>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif

// Error handling: where matplotlib raises, record the reason and stop.
#define FT_CHECK(func, ...) do { \
    if (FT_Error error_ = func(__VA_ARGS__)) { \
        fail(#func, error_); \
        return; \
    } \
} while (0)

FT_Library ft2Library()
{
    static FT_Library library = [] {
        FT_Library lib = nullptr;
        FT_Init_FreeType(&lib);  // initialize library
        return lib;
    }();
    return library;
}

// ft_outline_decomposer should be passed to FT_Outline_Decompose.
struct ft_outline_decomposer
{
    std::vector<double> &vertices;
    std::vector<unsigned char> &codes;
};

static int
ft_outline_move_to(FT_Vector const* to, void* user)
{
    ft_outline_decomposer* d = reinterpret_cast<ft_outline_decomposer*>(user);
    if (!d->vertices.empty()) {
        // Appending CLOSEPOLY is important to make patheffects work.
        d->vertices.push_back(0);
        d->vertices.push_back(0);
        d->codes.push_back(CLOSEPOLY);
    }
    d->vertices.push_back(to->x * (1. / 64.));
    d->vertices.push_back(to->y * (1. / 64.));
    d->codes.push_back(MOVETO);
    return 0;
}

static int
ft_outline_line_to(FT_Vector const* to, void* user)
{
    ft_outline_decomposer* d = reinterpret_cast<ft_outline_decomposer*>(user);
    d->vertices.push_back(to->x * (1. / 64.));
    d->vertices.push_back(to->y * (1. / 64.));
    d->codes.push_back(LINETO);
    return 0;
}

static int
ft_outline_conic_to(FT_Vector const* control, FT_Vector const* to, void* user)
{
    ft_outline_decomposer* d = reinterpret_cast<ft_outline_decomposer*>(user);
    d->vertices.push_back(control->x * (1. / 64.));
    d->vertices.push_back(control->y * (1. / 64.));
    d->vertices.push_back(to->x * (1. / 64.));
    d->vertices.push_back(to->y * (1. / 64.));
    d->codes.push_back(CURVE3);
    d->codes.push_back(CURVE3);
    return 0;
}

static int
ft_outline_cubic_to(
  FT_Vector const* c1, FT_Vector const* c2, FT_Vector const* to, void* user)
{
    ft_outline_decomposer* d = reinterpret_cast<ft_outline_decomposer*>(user);
    d->vertices.push_back(c1->x * (1. / 64.));
    d->vertices.push_back(c1->y * (1. / 64.));
    d->vertices.push_back(c2->x * (1. / 64.));
    d->vertices.push_back(c2->y * (1. / 64.));
    d->vertices.push_back(to->x * (1. / 64.));
    d->vertices.push_back(to->y * (1. / 64.));
    d->codes.push_back(CURVE4);
    d->codes.push_back(CURVE4);
    d->codes.push_back(CURVE4);
    return 0;
}

static FT_Outline_Funcs ft_outline_funcs = {
    ft_outline_move_to,
    ft_outline_line_to,
    ft_outline_conic_to,
    ft_outline_cubic_to};

void
FT2Font::get_path(std::vector<double> &vertices, std::vector<unsigned char> &codes)
{
    if (!face->glyph) {
        error = "No glyph loaded";
        return;
    }
    ft_outline_decomposer decomposer = {
        vertices,
        codes,
    };
    // We can make a close-enough estimate based on number of points and number of
    // contours (which produce a MOVETO each), though it's slightly underestimating due
    // to higher-order curves.
    size_t estimated_points = static_cast<size_t>(face->glyph->outline.n_contours) +
                              static_cast<size_t>(face->glyph->outline.n_points);
    vertices.reserve(2 * estimated_points);
    codes.reserve(estimated_points);
    if (FT_Error code = FT_Outline_Decompose(
            &face->glyph->outline, &ft_outline_funcs, &decomposer)) {
        fail("FT_Outline_Decompose", code);
        return;
    }
    if (vertices.empty()) {  // Don't append CLOSEPOLY to null glyphs.
        return;
    }
    vertices.push_back(0);
    vertices.push_back(0);
    codes.push_back(CLOSEPOLY);
}

FT2Font::FT2Font(std::vector<FT2Font *> &fallback_list, bool warn_if_used)
    : warn_if_used(warn_if_used), face(nullptr), fallbacks(fallback_list),
      // set default kerning factor to 0, i.e., no kerning manipulation
      kerning_factor(0)
{
    clear();
}

FT2Font::~FT2Font()
{
    close();
}

bool FT2Font::open(const std::string &path, FT_Long face_index)
{
    m_path = path;
    if (FT_Error code = FT_New_Face(ft2Library(), path.c_str(), face_index, &face)) {
        face = nullptr;
        fail("FT_New_Face", code);
        return false;
    }

    // This allows us to get back to our data if we need it, though it makes a pointer
    // loop, so don't set a free-function for it.
    face->generic.data = this;
    face->generic.finalizer = nullptr;
    return true;
}

bool FT2Font::openMemory(const std::string &path, std::string_view bytes, FT_Long face_index)
{
    m_path = path;
    if (FT_Error code = FT_New_Memory_Face(ft2Library(), reinterpret_cast<const FT_Byte *>(bytes.data()),
                                           static_cast<FT_Long>(bytes.size()), face_index, &face)) {
        face = nullptr;
        fail("FT_New_Memory_Face", code);
        return false;
    }
    face->generic.data = this;
    face->generic.finalizer = nullptr;
    return true;
}

void FT2Font::close()
{
    // This should be idempotent, in case a user manually calls close before the
    // destructor does. Note for example, that PyFT2Font _does_ call this before the
    // base destructor to ensure internal pointers are cleared early enough.

    for (auto & glyph : glyphs) {
        FT_Done_Glyph(glyph);
    }
    glyphs.clear();

    if (face) {
        FT_Done_Face(face);
        face = nullptr;
    }
}

void FT2Font::clear()
{
    pen.x = pen.y = 0;
    bbox.xMin = bbox.yMin = bbox.xMax = bbox.yMax = 0;
    advance = 0;

    for (auto & glyph : glyphs) {
        FT_Done_Glyph(glyph);
    }

    glyphs.clear();
    char_to_font.clear();

    for (auto & fallback : fallbacks) {
        fallback->clear();
    }
}

void FT2Font::set_size(double ptsize, double dpi)
{
    FT_CHECK(
        FT_Set_Char_Size,
        face, (FT_F26Dot6)(ptsize * 64), 0, (FT_UInt)dpi, (FT_UInt)dpi);
    FT_Matrix transform = { 65536, 0, 0, 65536 };
    FT_Set_Transform(face, &transform, nullptr);

    for (auto & fallback : fallbacks) {
        fallback->set_size(ptsize, dpi);
    }
}

void FT2Font::_set_transform(
    std::array<std::array<FT_Fixed, 2>, 2> matrix, std::array<FT_Fixed, 2> delta)
{
    FT_Matrix m = {matrix[0][0], matrix[0][1], matrix[1][0], matrix[1][1]};
    FT_Vector d = {delta[0], delta[1]};
    FT_Set_Transform(face, &m, &d);
    for (auto & fallback : fallbacks) {
        fallback->_set_transform(matrix, delta);
    }
}

void FT2Font::set_charmap(int i)
{
    if (i < 0 || i >= face->num_charmaps) {
        error = "i exceeds the available number of char maps";
        return;
    }
    FT_CHECK(FT_Set_Charmap, face, face->charmaps[i]);
}

void FT2Font::select_charmap(unsigned long i)
{
    FT_CHECK(FT_Select_Charmap, face, (FT_Encoding)i);
}

int FT2Font::get_kerning(FT_UInt left, FT_UInt right, FT_Kerning_Mode mode)
{
    if (!FT_HAS_KERNING(face)) {
        return 0;
    }

    FT_Vector delta;
    if (!FT_Get_Kerning(face, left, right, mode, &delta)) {
        return (int)(delta.x) / (1 << kerning_factor);
    } else {
        return 0;
    }
}

void FT2Font::set_kerning_factor(int factor)
{
    kerning_factor = factor;
    for (auto & fallback : fallbacks) {
        fallback->set_kerning_factor(factor);
    }
}

std::vector<raqm_glyph_t> FT2Font::layout(
    std::u32string_view text, FT_Int32 flags,
    std::optional<std::vector<std::string>> features, LanguageType languages,
    std::set<FT_String*>& glyph_seen_fonts)
{
    clear();

    auto rq = raqm_create();
    if (!rq) {
        { error = "failed to compute text layout"; return {}; }
    }
    [[maybe_unused]] auto const& rq_cleanup =
        std::unique_ptr<std::remove_pointer_t<raqm_t>, decltype(&raqm_destroy)>(
            rq, raqm_destroy);

    if (!raqm_set_text(rq, reinterpret_cast<const uint32_t *>(text.data()),
                       text.size()))
    {
        { error = "failed to set text for layout"; return {}; }
    }
    if (!raqm_set_freetype_face(rq, face)) {
        { error = "failed to set text face for layout"; return {}; }
    }
    if (!raqm_set_freetype_load_flags(rq, flags)) {
        { error = "failed to set text flags for layout"; return {}; }
    }
    if (features) {
        for (auto const& feature : *features) {
            if (!raqm_add_font_feature(rq, feature.c_str(), feature.size())) {
                error = "failed to set font feature " + feature;
                return {};
            }
        }
    }
    if (languages) {
        for (auto & [lang_str, start, end] : *languages) {
            if (!raqm_set_language(rq, lang_str.c_str(), start, end - start)) {
                error = "failed to set language between " + std::to_string(start) + " and " +
                        std::to_string(end) + " characters to '" + lang_str + "' for layout";
                return {};
            }
        }
    }
    if (!raqm_layout(rq)) {
        { error = "failed to layout text"; return {}; }
    }

    std::vector<std::pair<size_t, const FT_Face&>> face_substitutions;
    glyph_seen_fonts.insert(face->family_name);

    // Attempt to use fallback fonts if necessary.
    for (auto const& fallback : fallbacks) {
        size_t num_glyphs = 0;
        auto const& rq_glyphs = raqm_get_glyphs(rq, &num_glyphs);
        bool new_fallback_used = false;

        // Sort clusters (n.b. std::map is ordered), as RTL text will be returned in
        // display, not source, order.
        std::map<decltype(raqm_glyph_t::cluster), bool> cluster_missing;
        for (size_t i = 0; i < num_glyphs; i++) {
            auto const& rglyph = rq_glyphs[i];

            // Sometimes multiple glyphs are necessary for a single cluster; if any are
            // not found, we want to "poison" the whole set and keep them missing.
            cluster_missing[rglyph.cluster] |= (rglyph.index == 0);
        }

        for (auto it = cluster_missing.cbegin(); it != cluster_missing.cend(); ) {
            auto [cluster, missing] = *it;
            ++it;  // Early change so we can access the next cluster below.
            if (missing) {
                auto next = (it != cluster_missing.cend()) ? it->first : text.size();
                for (auto i = cluster; i < next; i++) {
                    face_substitutions.emplace_back(i, fallback->face);
                }
                new_fallback_used = true;
            }
        }

        if (!new_fallback_used) {
            // If we never used a fallback, then we're good to go with the existing
            // layout we have already made.
            break;
        }

        // If a fallback was used, then re-attempt the layout with the new fonts.
        if (!fallback->warn_if_used) {
            glyph_seen_fonts.insert(fallback->face->family_name);
        }

        raqm_clear_contents(rq);
        if (!raqm_set_text(rq,
                           reinterpret_cast<const uint32_t *>(text.data()),
                           text.size()))
        {
            { error = "failed to set text for layout"; return {}; }
        }
        if (!raqm_set_freetype_face(rq, face)) {
            { error = "failed to set text face for layout"; return {}; }
        }
        for (auto [cluster, fallback] : face_substitutions) {
            raqm_set_freetype_face_range(rq, fallback, cluster, 1);
        }
        if (!raqm_set_freetype_load_flags(rq, flags)) {
            { error = "failed to set text flags for layout"; return {}; }
        }
        if (features) {
            for (auto const& feature : *features) {
                if (!raqm_add_font_feature(rq, feature.c_str(), feature.size())) {
                    error = "failed to set font feature " + feature;
                    return {};
                }
            }
        }
        if (languages) {
            for (auto & [lang_str, start, end] : *languages) {
                if (!raqm_set_language(rq, lang_str.c_str(), start, end - start)) {
                    error = "failed to set language between " + std::to_string(start) + " and " +
                            std::to_string(end) + " characters to '" + lang_str + "' for layout";
                    return {};
                }
            }
        }
        if (!raqm_layout(rq)) {
            { error = "failed to layout text"; return {}; }
        }
    }

    size_t num_glyphs = 0;
    auto const& rq_glyphs = raqm_get_glyphs(rq, &num_glyphs);

    return std::vector<raqm_glyph_t>(rq_glyphs, rq_glyphs + num_glyphs);
}

void FT2Font::set_text(
    std::u32string_view text, double angle, FT_Int32 flags,
    std::optional<std::vector<std::string>> features, LanguageType languages,
    std::vector<double> &xys)
{
    FT_Matrix matrix; /* transformation matrix */

    angle = angle * (2 * M_PI / 360.0);

    // this computes width and height in subpixels so we have to multiply by 64
    double cosangle = cos(angle) * 0x10000L;
    double sinangle = sin(angle) * 0x10000L;

    matrix.xx = (FT_Fixed)cosangle;
    matrix.xy = (FT_Fixed)-sinangle;
    matrix.yx = (FT_Fixed)sinangle;
    matrix.yy = (FT_Fixed)cosangle;

    std::set<FT_String*> glyph_seen_fonts;
    auto rq_glyphs = layout(text, flags, features, languages, glyph_seen_fonts);
    if (!error.empty()) {
        return;
    }

    bbox.xMin = bbox.yMin = 32000;
    bbox.xMax = bbox.yMax = -32000;

    for (auto const& rglyph : rq_glyphs) {
        // Warn for missing glyphs.
        if (rglyph.index == 0) {
            ft_glyph_warn(text[rglyph.cluster], glyph_seen_fonts);
            continue;
        }
        FT2Font *wrapped_font = static_cast<FT2Font *>(rglyph.ftface->generic.data);
        if (wrapped_font->warn_if_used) {
            ft_glyph_warn(text[rglyph.cluster], glyph_seen_fonts);
        }

        // extract glyph image and store it in our table
        FT_Error code;
        code = FT_Load_Glyph(rglyph.ftface, rglyph.index, flags);
        if (code) {
            fail("failed to load glyph", code);
            return;
        }
        FT_Glyph thisGlyph;
        code = FT_Get_Glyph(rglyph.ftface->glyph, &thisGlyph);
        if (code) {
            fail("failed to get glyph", code);
            return;
        }

        pen.x += rglyph.x_offset;
        pen.y += rglyph.y_offset;

        FT_Glyph_Transform(thisGlyph, nullptr, &pen);
        FT_Glyph_Transform(thisGlyph, &matrix, nullptr);
        xys.push_back(pen.x);
        xys.push_back(pen.y);

        FT_BBox glyph_bbox;
        FT_Glyph_Get_CBox(thisGlyph, FT_GLYPH_BBOX_SUBPIXELS, &glyph_bbox);

        bbox.xMin = std::min(bbox.xMin, glyph_bbox.xMin);
        bbox.xMax = std::max(bbox.xMax, glyph_bbox.xMax);
        bbox.yMin = std::min(bbox.yMin, glyph_bbox.yMin);
        bbox.yMax = std::max(bbox.yMax, glyph_bbox.yMax);

        pen.x += rglyph.x_advance - rglyph.x_offset;
        pen.y += rglyph.y_advance - rglyph.y_offset;

        glyphs.push_back(thisGlyph);
    }

    FT_Vector_Transform(&pen, &matrix);
    advance = pen.x;

    if (bbox.xMin > bbox.xMax) {
        bbox.xMin = bbox.yMin = bbox.xMax = bbox.yMax = 0;
    }
}

void FT2Font::load_char(long charcode, FT_Int32 flags, FT2Font *&ft_object, bool fallback = false)
{
    // if this is parent FT2Font, cache will be filled in 2 ways:
    // 1. set_text was previously called
    // 2. set_text was not called and fallback was enabled
    std::set <FT_String *> glyph_seen_fonts;
    if (fallback && char_to_font.find(charcode) != char_to_font.end()) {
        ft_object = char_to_font[charcode];
        // since it will be assigned to ft_object anyway
        FT2Font *throwaway = nullptr;
        ft_object->load_char(charcode, flags, throwaway, false);
    } else if (fallback) {
        FT_UInt final_glyph_index;
        FT_Error charcode_error = FT_Err_Ok, glyph_error = FT_Err_Ok;
        FT2Font *ft_object_with_glyph = this;
        bool was_found = load_char_with_fallback(ft_object_with_glyph, final_glyph_index,
                                                 glyphs, char_to_font,
                                                 charcode, flags, charcode_error, glyph_error,
                                                 glyph_seen_fonts);
        if (!was_found) {
            ft_glyph_warn(charcode, glyph_seen_fonts);
            if (charcode_error) {
                fail("charcode loading", charcode_error);
            } else if (glyph_error) {
                fail("charcode loading", glyph_error);
            } else {
                error = "charcode loading failed to find glyph to load";
            }
            return;
        } else if (ft_object_with_glyph->warn_if_used) {
            ft_glyph_warn(charcode, glyph_seen_fonts);
        }
        ft_object = ft_object_with_glyph;
    } else {
        //no fallback case
        ft_object = this;
        FT_UInt glyph_index = FT_Get_Char_Index(face, (FT_ULong) charcode);
        if (!glyph_index){
            glyph_seen_fonts.insert((face != nullptr)?face->family_name: nullptr);
            ft_glyph_warn((FT_ULong)charcode, glyph_seen_fonts);
        }
        FT_CHECK(FT_Load_Glyph, face, glyph_index, flags);
        FT_Glyph thisGlyph;
        FT_CHECK(FT_Get_Glyph, face->glyph, &thisGlyph);
        glyphs.push_back(thisGlyph);
    }
}


bool FT2Font::get_char_fallback_index(FT_ULong charcode, int& index) const
{
    FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);
    if (glyph_index) {
        // -1 means the host has the char and we do not need to fallback
        index = -1;
        return true;
    } else {
        int inner_index = 0;
        bool was_found;

        for (size_t i = 0; i < fallbacks.size(); ++i) {
            // TODO handle recursion somehow!
            was_found = fallbacks[i]->get_char_fallback_index(charcode, inner_index);
            if (was_found) {
                index = i;
                return true;
            }
        }
    }
    return false;
}


bool FT2Font::load_char_with_fallback(FT2Font *&ft_object_with_glyph,
                                      FT_UInt &final_glyph_index,
                                      std::vector<FT_Glyph> &parent_glyphs,
                                      std::unordered_map<long, FT2Font *> &parent_char_to_font,
                                      long charcode,
                                      FT_Int32 flags,
                                      FT_Error &charcode_error,
                                      FT_Error &glyph_error,
                                      std::set<FT_String*> &glyph_seen_fonts)
{
    charcode_error = FT_Err_Ok;
    glyph_error = FT_Err_Ok;
    FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);
    if (!warn_if_used) {
        glyph_seen_fonts.insert(face->family_name);
    }

    if (glyph_index) {
        charcode_error = FT_Load_Glyph(face, glyph_index, flags);
        if (charcode_error) {
            return false;
        }
        FT_Glyph thisGlyph;
        glyph_error = FT_Get_Glyph(face->glyph, &thisGlyph);
        if (glyph_error) {
            return false;
        }

        final_glyph_index = glyph_index;

        // cache the result for future
        // need to store this for anytime a character is loaded from a parent
        // FT2Font object or to generate a mapping of individual characters to fonts
        ft_object_with_glyph = this;
        parent_char_to_font[charcode] = this;
        parent_glyphs.push_back(thisGlyph);
        return true;
    }
    else {
        for (auto & fallback : fallbacks) {
            bool was_found = fallback->load_char_with_fallback(
                ft_object_with_glyph, final_glyph_index, parent_glyphs,
                parent_char_to_font, charcode, flags,
                charcode_error, glyph_error, glyph_seen_fonts);
            if (was_found) {
                return true;
            }
        }
        return false;
    }
}

void FT2Font::load_glyph(FT_UInt glyph_index, FT_Int32 flags)
{
    FT_CHECK(FT_Load_Glyph, face, glyph_index, flags);
    FT_Glyph thisGlyph;
    FT_CHECK(FT_Get_Glyph, face->glyph, &thisGlyph);
    glyphs.push_back(thisGlyph);
}

FT_UInt FT2Font::get_char_index(FT_ULong charcode, bool fallback = false)
{
    FT2Font *ft_object = nullptr;
    if (fallback && char_to_font.find(charcode) != char_to_font.end()) {
        // fallback denotes whether we want to search fallback list.
        // should call set_text/load_char_with_fallback to parent FT2Font before
        // wanting to use fallback list here. (since that populates the cache)
        ft_object = char_to_font[charcode];
    } else {
        // set as self
        ft_object = this;
    }

    return FT_Get_Char_Index(ft_object->get_face(), charcode);
}

std::tuple<long, long> FT2Font::get_width_height()
{
    return {advance, bbox.yMax - bbox.yMin};
}

long FT2Font::get_descent()
{
    return -bbox.yMin;
}

std::tuple<long, long> FT2Font::get_bitmap_offset()
{
    return {bbox.xMin, 0};
}

std::string FT2Font::get_glyph_name(unsigned int glyph_number)
{
    std::string buffer;
    buffer.resize(128);

    if (!FT_HAS_GLYPH_NAMES(face)) {
        /* Note that this generated name must match the name that
           is generated by ttconv in ttfont_CharStrings_getname. */
        auto len = snprintf(buffer.data(), buffer.size(), "uni%08x", glyph_number);
        if (len >= 0) {
            buffer.resize(len);
        } else {
            error = "Failed to convert glyph to standard name";
            return {};
        }
    } else {
        if (FT_Error code = FT_Get_Glyph_Name(face, glyph_number, buffer.data(), buffer.size())) {
            fail("FT_Get_Glyph_Name", code);
            return {};
        }
        auto len = buffer.find('\0');
        if (len != buffer.npos) {
            buffer.resize(len);
        }
    }

    return buffer;
}

long FT2Font::get_name_index(char *name)
{
    return FT_Get_Name_Index(face, (FT_String *)name);
}

void FT2Font::fail(const char *what, FT_Error code)
{
    if (error.empty()) {
        error = std::string(what) + " failed with FreeType error " + std::to_string(code);
    }
}

// matplotlib warns through Python ("Glyph ... missing from font(s) ..."); the port keeps a note.
void FT2Font::ft_glyph_warn(FT_ULong charcode, std::set<FT_String*> family_names)
{
    std::string names;
    for (auto name : family_names) {
        if (name) {
            names += (names.empty() ? "" : ", ") + std::string(name);
        }
    }
    warnings.push_back("Glyph " + std::to_string(charcode) + " missing from font(s) " + names + ".");
}

/**********************************************************************
 * From the Python wrapper (src/ft2font_wrapper.cpp): layout items and rendered glyphs.
 * */

std::vector<LayoutItem> FT2Font::layout_items(
    std::u32string_view text, FT_Int32 load_flags,
    std::optional<std::vector<std::string>> features, LanguageType languages)
{
    std::set<FT_String*> glyph_seen_fonts;
    auto glyphs = layout(text, load_flags, features, languages, glyph_seen_fonts);
    if (!error.empty()) {
        return {};
    }

    std::set<decltype(raqm_glyph_t::cluster)> clusters;
    for (auto &glyph : glyphs) {
        clusters.emplace(glyph.cluster);
    }

    std::vector<LayoutItem> items;

    double x = 0.0;
    double y = 0.0;
    std::optional<double> prev_advance = std::nullopt;
    double prev_x = 0.0;
    for (auto &glyph : glyphs) {
        auto ft_object = static_cast<FT2Font *>(glyph.ftface->generic.data);

        ft_object->load_glyph(glyph.index, load_flags);

        double prev_kern = 0.0;
        if (prev_advance) {
            double actual_advance = (x + glyph.x_offset) - prev_x;
            prev_kern = actual_advance - *prev_advance;
        }

        auto next = clusters.upper_bound(glyph.cluster);
        auto end = (next != clusters.end()) ? *next : text.size();
        auto substr = text.substr(glyph.cluster, end - glyph.cluster);

        items.push_back(LayoutItem{ft_object, std::u32string(substr), int(glyph.index),
                                   (x + glyph.x_offset) / 64.0, (y + glyph.y_offset) / 64.0,
                                   prev_kern / 64.0});
        prev_x = x + glyph.x_offset;
        x += glyph.x_advance;
        y += glyph.y_advance;
        // Note, linearHoriAdvance is a 16.16 instead of 26.6 fixed-point value.
        prev_advance = ft_object->get_face()->glyph->linearHoriAdvance / 1024.0;
    }

    return items;
}

PositionedBitmap FT2Font::render_glyph(FT_UInt idx, FT_Int32 flags, FT_Render_Mode render_mode)
{
    PositionedBitmap result;
    if (FT_Error code = FT_Load_Glyph(face, idx, flags)) {
        fail("FT_Load_Glyph", code);
        return result;
    }
    if (FT_Error code = FT_Render_Glyph(face->glyph, render_mode)) {
        fail("FT_Render_Glyph", code);
        return result;
    }
    FT_GlyphSlot slot = face->glyph;
    result.left = slot->bitmap_left;
    result.top = slot->bitmap_top;
    // As matplotlib's _PositionedBitmap: converted to one byte per pixel, 1-byte aligned.
    FT_Bitmap bitmap;
    FT_Bitmap_Init(&bitmap);
    if (FT_Error code = FT_Bitmap_Convert(ft2Library(), &slot->bitmap, &bitmap, 1)) {
        fail("FT_Bitmap_Convert", code);
        return result;
    }
    result.rows = bitmap.rows;
    result.width = bitmap.width;
    result.buffer.resize(std::size_t(bitmap.rows) * bitmap.width);
    for (unsigned r = 0; r < bitmap.rows; ++r) {
        const unsigned char *row = bitmap.buffer + std::ptrdiff_t(r) * bitmap.pitch;
        std::copy(row, row + bitmap.width, result.buffer.begin() + std::ptrdiff_t(r) * bitmap.width);
    }
    FT_Bitmap_Done(ft2Library(), &bitmap);
    return result;
}
