// The text stack is the one matplotlib 3.11.2 renders with, at the same versions and options.
#include <gtest/gtest.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/config/ftoption.h>
#include <hb.h>
#include <hb-ft.h>
#include <raqm.h>
#include <SheenBidi/SheenBidi.h>

#include <string>

TEST(TextLibraries, FreeTypeIsVersion2_14_3) {
    FT_Library library = nullptr;
    ASSERT_EQ(FT_Init_FreeType(&library), 0);
    FT_Int major = 0, minor = 0, patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    FT_Done_FreeType(library);
    EXPECT_EQ(major, 2);
    EXPECT_EQ(minor, 14);
    EXPECT_EQ(patch, 3);
}

TEST(TextLibraries, FreeTypeUsesHarfBuzzLikeMatplotlibsBuild) {
#ifdef FT_CONFIG_OPTION_USE_HARFBUZZ
    SUCCEED();
#else
    FAIL() << "FreeType must be configured with FT_CONFIG_OPTION_USE_HARFBUZZ";
#endif
#ifdef FT_CONFIG_OPTION_USE_HARFBUZZ_DYNAMIC
    FAIL() << "HarfBuzz must be linked, not loaded at run time";
#endif
#ifndef FT_CONFIG_OPTION_USE_ZLIB
    FAIL() << "FreeType's internal zlib must be on";
#endif
#if defined(FT_CONFIG_OPTION_USE_PNG) || defined(FT_CONFIG_OPTION_USE_BZIP2) || defined(FT_CONFIG_OPTION_USE_BROTLI)
    FAIL() << "png, bzip2 and brotli must be off";
#endif
}

TEST(TextLibraries, HarfBuzzIsVersion14_1_0) {
    EXPECT_EQ(std::string(hb_version_string()), "14.1.0");
}

TEST(TextLibraries, RaqmIsVersion0_10_5) {
    EXPECT_EQ(std::string(raqm_version_string()), "0.10.5");
}

TEST(TextLibraries, SheenBidiResolvesADirection) {
    // "abc" is left to right; SheenBidi's own algorithm says so.
    const char text[] = "abc";
    SBCodepointSequence sequence = {SBStringEncodingUTF8, (void*)text, 3};
    SBAlgorithmRef algorithm = SBAlgorithmCreate(&sequence);
    ASSERT_NE(algorithm, nullptr);
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(algorithm, 0, INT32_MAX, SBLevelDefaultLTR);
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(SBParagraphGetBaseLevel(paragraph), 0);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(algorithm);
}

TEST(TextLibraries, RaqmShapesTextWithAFreeTypeFace) {
    FT_Library library = nullptr;
    ASSERT_EQ(FT_Init_FreeType(&library), 0);
    FT_Face face = nullptr;
    ASSERT_EQ(FT_New_Face(library, CALCULATE_PLOT_TEST_FONT, 0, &face), 0) << CALCULATE_PLOT_TEST_FONT;
    ASSERT_EQ(FT_Set_Char_Size(face, 10 * 64, 0, 100, 100), 0);
    raqm_t* rq = raqm_create();
    const char text[] = "AV";
    ASSERT_TRUE(raqm_set_text_utf8(rq, text, 2));
    ASSERT_TRUE(raqm_set_freetype_face(rq, face));
    ASSERT_TRUE(raqm_layout(rq));
    size_t count = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(rq, &count);
    ASSERT_EQ(count, 2u);
    EXPECT_NE(glyphs[0].index, 0u);
    EXPECT_GT(glyphs[0].x_advance, 0);
    raqm_destroy(rq);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
}
