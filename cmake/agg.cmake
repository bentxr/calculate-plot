# Anti-Grain Geometry 2.4 as matplotlib 3.11.2 vendors and patches it (extern/agg24-svn), compiled
# as matplotlib's meson build compiles it.
add_library(calculate-plot-agg STATIC
    extern/agg24-svn/src/agg_bezier_arc.cpp
    extern/agg24-svn/src/agg_curves.cpp
    extern/agg24-svn/src/agg_image_filters.cpp
    extern/agg24-svn/src/agg_trans_affine.cpp
    extern/agg24-svn/src/agg_vcgen_contour.cpp
    extern/agg24-svn/src/agg_vcgen_dash.cpp
    extern/agg24-svn/src/agg_vcgen_stroke.cpp
    extern/agg24-svn/src/agg_vpgen_segmentator.cpp)
target_include_directories(calculate-plot-agg SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/extern/agg24-svn/include)
target_compile_options(calculate-plot-agg PRIVATE -w)  # upstream code, compiled as published
set_target_properties(calculate-plot-agg PROPERTIES POSITION_INDEPENDENT_CODE ON)
# matplotlib's meson build enables two of Agg's bug fixes for every C++ file (image filter tables, the end point
# of the linear span interpolator); they change the pixels of resampled images.
target_compile_definitions(calculate-plot-agg PUBLIC MPL_FIX_AGG_IMAGE_FILTER_LUT_BUGS MPL_FIX_AGG_INTERPOLATION_ENDPOINT_BUG)
