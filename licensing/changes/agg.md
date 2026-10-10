# Changes to Anti-Grain Geometry 2.4 (as vendored by Matplotlib 3.11.2)

The files in extern/agg24-svn are Matplotlib 3.11.2's copy of Agg 2.4 (extern/agg24-svn there),
which Matplotlib itself patched. calculate-plot copies them unmodified, except:

- include/agg_rasterizer_cells_aa.h: `add_curr_cell()` calls `AGG_CELL_BLOCK_LIMIT_HANDLER()` and returns
  when that macro is defined, instead of throwing `std::overflow_error`, so that the library can be built
  without C++ exceptions (src/agg/agg_hooks.h defines the macro; the renderer reports the error). Without the
  macro the file behaves exactly as Matplotlib's.
- include/agg_conv_gpc.h is not copied: it adapts the General Polygon Clipper, which is not free for
  commercial use, and nothing uses it.
- Only the headers and the eight source files Matplotlib's build uses are copied.
