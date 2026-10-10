FetchContent_Declare(googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    SYSTEM)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(calculate-plot-tests
    test_base64.cpp
    test_cli.cpp
    test_glob.cpp
    test_json.cpp
    test_licensing.cpp
    test_model.cpp
    test_notices.cpp
    test_png.cpp
    test_rc.cpp
    test_pyformat.cpp
    test_replay.cpp
    licensing/compliance.cpp
    test_sha256.cpp
    test_svg.cpp
    test_text.cpp
    test_text_libraries.cpp
    test_ticker.cpp
    test_version.cpp
    test_xml_writer.cpp
)
target_include_directories(calculate-plot-tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(calculate-plot-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/agg)
target_compile_definitions(calculate-plot-tests PRIVATE
    CALCULATE_PLOT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    CALCULATE_PLOT_BINARY_DIR="${PROJECT_BINARY_DIR}"
    CALCULATE_PLOT_VERSION_FOR_TESTS="${PROJECT_VERSION}"
    CALCULATE_PLOT_TEST_FONT="${PROJECT_SOURCE_DIR}/fonts/ttf/DejaVuSans.ttf")
target_link_libraries(calculate-plot-tests PRIVATE calculate-plot cplot-cli calculate-plot-warnings GTest::gtest_main)
if(EMSCRIPTEN)
    target_link_options(calculate-plot-tests PRIVATE
        -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sNODERAWFS=1)
endif()

include(GoogleTest)
gtest_discover_tests(calculate-plot-tests DISCOVERY_MODE PRE_TEST)
