# zlib 1.3.2, built unmodified from its release archive. Z_PREFIX (zlib's own option) gives every
# symbol a z_ prefix, so this copy never meets another zlib in the same program (Qt has one).
FetchContent_Declare(zlib
    URL      https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(zlib)

set(zlib_sources adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c
    infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
list(TRANSFORM zlib_sources PREPEND ${zlib_SOURCE_DIR}/)
add_library(calculate-plot-zlib STATIC ${zlib_sources})
target_include_directories(calculate-plot-zlib SYSTEM PUBLIC ${zlib_SOURCE_DIR})
target_compile_definitions(calculate-plot-zlib PUBLIC Z_PREFIX PRIVATE HAVE_UNISTD_H)
target_compile_options(calculate-plot-zlib PRIVATE -w)  # upstream code, compiled as published
set_target_properties(calculate-plot-zlib PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
