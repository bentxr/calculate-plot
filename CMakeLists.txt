cmake_minimum_required(VERSION 3.25)
project(calculate-plot VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Numerical correctness: no FMA contraction, so every target computes the same doubles.
add_compile_options(-ffp-contract=off)

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

add_library(calculate-plot STATIC
    ${PROJECT_BINARY_DIR}/generated/notices.cpp
    src/version.cpp
    src/agg/agg_hooks.cpp
    src/agg/backend_agg.cpp
    src/png/png.cpp
    src/render/replay.cpp
    src/support/base64.cpp
    src/support/glob.cpp
    src/support/json.cpp
    src/support/sha256.cpp)
target_include_directories(calculate-plot
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src)
# Matplotlib's renderer, ported: its headers keep upstream's style, so they are system headers here,
# and the files that instantiate its templates get the warnings matplotlib's own build accepts.
target_include_directories(calculate-plot SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/agg)
set_source_files_properties(src/agg/backend_agg.cpp src/render/replay.cpp PROPERTIES COMPILE_OPTIONS
    "-Wno-unused-parameter;-Wno-shadow;-Wno-sign-compare")
target_compile_definitions(calculate-plot PRIVATE CALCULATE_PLOT_VERSION="${PROJECT_VERSION}")
target_compile_options(calculate-plot PRIVATE -fno-exceptions)
target_link_libraries(calculate-plot PUBLIC calculate-plot-raqm calculate-plot-zlib calculate-plot-agg PRIVATE calculate-plot-warnings)

add_library(cplot-cli STATIC cli/cli.cpp)
target_include_directories(cplot-cli PUBLIC ${PROJECT_SOURCE_DIR}/cli)
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
