# The text stack matplotlib 3.11.2 renders with, pinned to its exact versions and built the way its
# meson build configures them (extern/meson.build and subprojects/*.wrap in matplotlib):
#   FreeType 2.14.3  default options, zlib internal, no bzip2/png/brotli, HarfBuzz enabled
#   HarfBuzz 14.1.0  FreeType enabled, everything else off
#   SheenBidi 3.0.0  bidirectional text
#   libraqm 0.10.5   text layout over HarfBuzz and SheenBidi
# The libraries are used as published (C and C++). Programs that also load another copy (Qt links
# its own FreeType and HarfBuzz) keep ours private: executables link with --exclude-libs,ALL.

FetchContent_Declare(freetype
    URL      https://download.savannah.nongnu.org/releases/freetype/freetype-2.14.3.tar.xz
    URL_HASH SHA256=36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
    SOURCE_SUBDIR no-cmake)
FetchContent_Declare(harfbuzz
    URL      https://github.com/harfbuzz/harfbuzz/releases/download/14.1.0/harfbuzz-14.1.0.tar.xz
    URL_HASH SHA256=ee0eb3a1da2c5a28147f12dff55f6c7d60aeeeb29ac7ef334eabe84c8476c105
    SOURCE_SUBDIR no-cmake)
FetchContent_Declare(sheenbidi
    URL      https://github.com/Tehreer/SheenBidi/archive/refs/tags/v3.0.0/sheenbidi-3.0.0.tar.gz
    URL_HASH SHA256=86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786
    SOURCE_SUBDIR no-cmake)
FetchContent_Declare(libraqm
    URL      https://github.com/HOST-Oman/libraqm/archive/v0.10.5/libraqm-0.10.5.tar.gz
    URL_HASH SHA256=7f3dd21b4b3bd28a36f2c911d31d91a9d69341697713923ef1aac65d56ebcafd
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(freetype harfbuzz sheenbidi libraqm)

set(text_generated ${PROJECT_BINARY_DIR}/generated/text)

# FreeType: its default ftoption.h with HarfBuzz switched on (what matplotlib's patched meson
# option harfbuzz=static does), and the module list of its own CMake build.
file(READ ${freetype_SOURCE_DIR}/include/freetype/config/ftoption.h ftoption)
string(REPLACE "/* #define FT_CONFIG_OPTION_USE_HARFBUZZ */" "#define FT_CONFIG_OPTION_USE_HARFBUZZ"
       ftoption "${ftoption}")
file(CONFIGURE OUTPUT ${text_generated}/freetype/config/ftoption.h CONTENT "${ftoption}" @ONLY)
set(freetype_sources
    autofit/autofit.c base/ftbase.c base/ftbbox.c base/ftbdf.c base/ftbitmap.c base/ftcid.c
    base/ftfstype.c base/ftgasp.c base/ftglyph.c base/ftgxval.c base/ftinit.c base/ftmm.c
    base/ftotval.c base/ftpatent.c base/ftpfr.c base/ftstroke.c base/ftsynth.c base/fttype1.c
    base/ftwinfnt.c bdf/bdf.c bzip2/ftbzip2.c cache/ftcache.c cff/cff.c cid/type1cid.c
    gzip/ftgzip.c lzw/ftlzw.c pcf/pcf.c pfr/pfr.c psaux/psaux.c pshinter/pshinter.c
    psnames/psnames.c raster/raster.c sdf/sdf.c sfnt/sfnt.c smooth/smooth.c svg/svg.c
    truetype/truetype.c type1/type1.c type42/type42.c winfonts/winfnt.c
    base/ftsystem.c base/ftdebug.c)
list(TRANSFORM freetype_sources PREPEND ${freetype_SOURCE_DIR}/src/)
add_library(calculate-plot-freetype STATIC ${freetype_sources})
target_include_directories(calculate-plot-freetype SYSTEM BEFORE PUBLIC
    ${text_generated} ${freetype_SOURCE_DIR}/include)
target_include_directories(calculate-plot-freetype SYSTEM PRIVATE ${harfbuzz_SOURCE_DIR}/src)
target_compile_definitions(calculate-plot-freetype PRIVATE FT2_BUILD_LIBRARY)

# HarfBuzz: the amalgamated source with FreeType, as matplotlib configures it.
add_library(calculate-plot-harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(calculate-plot-harfbuzz SYSTEM PUBLIC ${harfbuzz_SOURCE_DIR}/src)
target_compile_definitions(calculate-plot-harfbuzz PRIVATE
    HAVE_FREETYPE=1 HAVE_FT_GET_VAR_BLEND_COORDINATES=1 HAVE_FT_SET_VAR_BLEND_COORDINATES=1
    HAVE_FT_DONE_MM_VAR=1 HAVE_FT_GET_TRANSFORM=1)
target_compile_options(calculate-plot-harfbuzz PRIVATE -fno-exceptions -fno-rtti)
target_link_libraries(calculate-plot-harfbuzz PUBLIC calculate-plot-freetype)

# SheenBidi: the unity source.
add_library(calculate-plot-sheenbidi STATIC ${sheenbidi_SOURCE_DIR}/Source/SheenBidi.c)
target_include_directories(calculate-plot-sheenbidi SYSTEM PUBLIC ${sheenbidi_SOURCE_DIR}/Headers)
target_include_directories(calculate-plot-sheenbidi PRIVATE ${sheenbidi_SOURCE_DIR}/Source)
# Without its pool of scratch buffers (ordinary memory instead): two threads taking and returning buffers at once can
# be handed the same buffer (the pool's lock-free stack has no guard against a buffer returning between a read and a
# swap), and figures are drawn on several threads.
target_compile_definitions(calculate-plot-sheenbidi PRIVATE SB_CONFIG_UNITY SB_CONFIG_DISABLE_SCRATCH_MEMORY)

# libraqm over SheenBidi.
file(READ ${libraqm_SOURCE_DIR}/src/raqm-version.h.in raqm_version)
set(RAQM_VERSION_MAJOR 0)
set(RAQM_VERSION_MINOR 10)
set(RAQM_VERSION_MICRO 5)
set(RAQM_VERSION 0.10.5)
file(CONFIGURE OUTPUT ${text_generated}/raqm-version.h CONTENT "${raqm_version}" @ONLY)
file(CONFIGURE OUTPUT ${text_generated}/raqm-config/config.h CONTENT "#define RAQM_SHEENBIDI 1\n")
add_library(calculate-plot-raqm STATIC ${libraqm_SOURCE_DIR}/src/raqm.c)
target_include_directories(calculate-plot-raqm SYSTEM PUBLIC ${libraqm_SOURCE_DIR}/src ${text_generated})
target_include_directories(calculate-plot-raqm PRIVATE ${text_generated}/raqm-config)
target_compile_definitions(calculate-plot-raqm PRIVATE HAVE_CONFIG_H RAQM_SHEENBIDI_GT_2_9)
target_link_libraries(calculate-plot-raqm PUBLIC
    calculate-plot-harfbuzz calculate-plot-sheenbidi calculate-plot-freetype)

foreach(lib freetype harfbuzz sheenbidi raqm)
    set_target_properties(calculate-plot-${lib} PROPERTIES
        C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
    target_compile_options(calculate-plot-${lib} PRIVATE -w)  # upstream code, compiled as published
endforeach()
