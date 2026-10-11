cmake_minimum_required(VERSION 3.25)
project(calculate-plot VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Numerical correctness: no FMA contraction, so every target computes the same doubles.
add_compile_options(-ffp-contract=off)

# A program built with threads for the browser (Qt's wasm_multithread kit) needs every object
# compiled with -pthread, ours and the libraries we build.
option(CALCULATE_PLOT_THREADS "Compile with -pthread under Emscripten" OFF)
if(EMSCRIPTEN AND CALCULATE_PLOT_THREADS)
    add_compile_options(-pthread)
endif()

include(FetchContent)
include(cmake/text-libraries.cmake)
include(cmake/zlib.cmake)
include(cmake/agg.cmake)

add_library(calculate-plot-warnings INTERFACE)
target_compile_options(calculate-plot-warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
if(PROJECT_IS_TOP_LEVEL)  # warnings are errors here, but never break a project that includes this one
    target_compile_options(calculate-plot-warnings INTERFACE -Werror)
endif()

include(cmake/notices.cmake)
calculate_plot_notice_files(notice_files)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/licensing/manifest.json)
list(TRANSFORM notice_files PREPEND "${PROJECT_SOURCE_DIR}/" OUTPUT_VARIABLE notice_paths)
add_custom_command(
    OUTPUT  ${PROJECT_BINARY_DIR}/generated/notices.cpp
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} "-DFILES=${notice_files}"
            -DOUTPUT=${PROJECT_BINARY_DIR}/generated/notices.cpp -P ${PROJECT_SOURCE_DIR}/cmake/notices.cmake
    DEPENDS ${notice_paths} ${PROJECT_SOURCE_DIR}/cmake/notices.cmake ${PROJECT_SOURCE_DIR}/licensing/manifest.json
    COMMENT "Embedding the third-party notices"
    VERBATIM)

# Data files compiled into the library.
set(embedded_files data/rcdefaults.json data/colors.json data/fonts.json data/marker_paths.json data/tex2uni.json data/colormaps.json
    data/mathtext_data.json data/unicode_letters.json)
list(TRANSFORM embedded_files PREPEND "${PROJECT_SOURCE_DIR}/" OUTPUT_VARIABLE embedded_paths)
add_custom_command(
    OUTPUT  ${PROJECT_BINARY_DIR}/generated/embedded.cpp
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} "-DFILES=${embedded_files}"
            -DOUTPUT=${PROJECT_BINARY_DIR}/generated/embedded.cpp -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS ${embedded_paths} ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
    COMMENT "Embedding data files"
    VERBATIM)

add_library(calculate-plot STATIC
    ${PROJECT_BINARY_DIR}/generated/embedded.cpp
    ${PROJECT_BINARY_DIR}/generated/notices.cpp
    src/version.cpp
    src/agg/agg_hooks.cpp
    src/axes/log_ticker.cpp
    src/axes/ticker.cpp
    src/colors/colormap.cpp
    src/colors/colors.cpp
    src/engine/artist.cpp
    src/engine/axes.cpp
    src/engine/collections.cpp
    src/engine/engine.cpp
    src/engine/figure.cpp
    src/engine/tight_layout.cpp
    src/engine/gc.cpp
    src/engine/gridspec.cpp
    src/engine/legend.cpp
    src/engine/lines.cpp
    src/engine/path.cpp
    src/engine/text.cpp
    src/engine/trace_renderer.cpp
    src/agg/backend_agg.cpp
    src/model/figure_model.cpp
    src/png/png.cpp
    src/rc/rc_params.cpp
    src/render/replay.cpp
    src/render/svg_replay.cpp
    src/support/base64.cpp
    src/transforms/transforms.cpp
    src/svg/svg_renderer.cpp
    src/svg/xml_writer.cpp
    src/text/font_manager.cpp
    src/text/ft2font.cpp
    src/text/mathtext.cpp
    src/text/mathtext_fonts.cpp
    src/text/text_render.cpp
    src/support/fontconfig_pattern.cpp
    src/support/glob.cpp
    src/support/pyparse.cpp
    src/support/json.cpp
    src/support/json_writer.cpp
    src/support/pyformat.cpp
    src/support/pynum.cpp
    src/support/log10.cpp
    src/support/pyformat_round.cpp
    src/support/pysum.cpp
    src/support/sha256.cpp)
target_include_directories(calculate-plot
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src)
# Matplotlib's renderer, ported: its headers keep upstream's style, so they are system headers here,
# and the files that instantiate its templates get the warnings matplotlib's own build accepts.
target_include_directories(calculate-plot SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/agg)
set_source_files_properties(src/agg/backend_agg.cpp src/render/replay.cpp src/text/ft2font.cpp src/text/text_render.cpp
    src/svg/svg_renderer.cpp src/render/svg_replay.cpp
    PROPERTIES COMPILE_OPTIONS
    "-Wno-unused-parameter;-Wno-shadow;-Wno-sign-compare;-Wno-missing-field-initializers")
target_compile_definitions(calculate-plot PRIVATE CALCULATE_PLOT_VERSION="${PROJECT_VERSION}")
target_compile_options(calculate-plot PRIVATE -fno-exceptions)
target_link_libraries(calculate-plot PUBLIC calculate-plot-raqm calculate-plot-zlib calculate-plot-agg PRIVATE calculate-plot-warnings)

add_library(cplot-cli STATIC cli/cli.cpp)
target_include_directories(cplot-cli PUBLIC ${PROJECT_SOURCE_DIR}/cli PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(cplot-cli PRIVATE -fno-exceptions)
target_link_libraries(cplot-cli PUBLIC calculate-plot PRIVATE calculate-plot-warnings)

add_executable(cplot cli/main.cpp)
target_link_libraries(cplot PRIVATE cplot-cli calculate-plot-warnings)
if(EMSCRIPTEN)
    target_link_options(cplot PRIVATE -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sNODERAWFS=1)
endif()

include(CTest)
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()
