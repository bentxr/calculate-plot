cmake_minimum_required(VERSION 3.25)
project(calculate-plot VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Numerical correctness: no FMA contraction, so every target computes the same doubles.
add_compile_options(-ffp-contract=off)

include(FetchContent)

add_library(calculate-plot-warnings INTERFACE)
target_compile_options(calculate-plot-warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
if(PROJECT_IS_TOP_LEVEL)  # warnings are errors here, but never break a project that includes this one
    target_compile_options(calculate-plot-warnings INTERFACE -Werror)
endif()

add_library(calculate-plot STATIC src/version.cpp)
target_include_directories(calculate-plot
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(calculate-plot PRIVATE CALCULATE_PLOT_VERSION="${PROJECT_VERSION}")
target_compile_options(calculate-plot PRIVATE -fno-exceptions)
target_link_libraries(calculate-plot PRIVATE calculate-plot-warnings)

include(CTest)
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()
