# Qhull 8.0.2 (the version matplotlib 3.11.2 builds with), its reentrant library libqhull_r built unmodified from the
# release archive. (matplotlib applies a patch to it that changes only diagnostic messages, which are discarded here
# as matplotlib discards them: the triangulations are the same.)
FetchContent_Declare(qhull
    URL      https://github.com/qhull/qhull/archive/v8.0.2/qhull-8.0.2.tar.gz
    URL_HASH SHA256=8774e9a12c70b0180b95d6b0b563c5aa4bea8d5960c15e18ae3b6d2521d64f8b
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(qhull)

# the sources matplotlib's meson build compiles
set(qhull_sources geom2_r.c geom_r.c global_r.c io_r.c libqhull_r.c mem_r.c merge_r.c poly2_r.c poly_r.c qset_r.c
    random_r.c rboxlib_r.c stat_r.c usermem_r.c userprintf_rbox_r.c userprintf_r.c user_r.c)
list(TRANSFORM qhull_sources PREPEND ${qhull_SOURCE_DIR}/src/libqhull_r/)
add_library(calculate-plot-qhull STATIC ${qhull_sources})
target_include_directories(calculate-plot-qhull SYSTEM PUBLIC ${qhull_SOURCE_DIR}/src)
target_compile_options(calculate-plot-qhull PRIVATE -w)  # upstream code, compiled as published
set_target_properties(calculate-plot-qhull PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
